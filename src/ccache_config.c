/********************************************************************
	created:	2009/10/04
	filename: 	ccache_config.c
	author:		Lichuang
                
	purpose:    
*********************************************************************/

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "ccache_config.h"
#include "ccache_util.h"

ccache_config_t ccache_config;
int ccache_align_size;

#define CCACHE_CONFIG_MAXLEN 50
#define CCACHE_SECTION_NAME "[ccache_conf]"
#define CCACHE_ITEM_UNSET -1

enum 
{
    CCACHE_READ_SECTION,
    CCACHE_READ_ITEM,
    CCACHE_READ_ERROR
};

typedef enum ccache_itemtype_t
{
    CCACHE_ITEM_STRING,
    CCACHE_ITEM_INT,
    CCACHE_ITEM_BOOL,
    CCACHE_ITEM_INVALID
}ccache_itemtype_t;

typedef struct ccache_conf_item_t
{
    char *name;
    ccache_itemtype_t type;
    int offset;
    int (*set)(char *buffer, int offset);
    char *defval;
}ccache_conf_item_t;

static int ccache_item_set_string(char *buffer, int offset);
static int ccache_item_set_int(char *buffer, int offset);
static int ccache_item_set_bool(char *buffer, int offset);

static ccache_conf_item_t ccache_conf_items[] = 
{
    {"mapfile",     CCACHE_ITEM_STRING,   offsetof(ccache_config_t, path),      ccache_item_set_string, "./ccache_mapfile"},
    {"min_size",    CCACHE_ITEM_INT,      offsetof(ccache_config_t, min_size),  ccache_item_set_int,    "16"},
    {"max_size",    CCACHE_ITEM_INT,      offsetof(ccache_config_t, max_size),  ccache_item_set_int,    "32"},
    {"hashitem",    CCACHE_ITEM_INT,      offsetof(ccache_config_t, hashitem),  ccache_item_set_int,    "1000"},
    {"datasize",    CCACHE_ITEM_INT,      offsetof(ccache_config_t, datasize),  ccache_item_set_int,    "1000000"},
    {"prealloc",    CCACHE_ITEM_BOOL,     offsetof(ccache_config_t, prealloc),  ccache_item_set_bool,   "1"},
    {"prealloc_num",CCACHE_ITEM_INT,      offsetof(ccache_config_t, prealloc_num), ccache_item_set_int, "100"},
    {"alignsize",   CCACHE_ITEM_INT,      offsetof(ccache_config_t, align_size), ccache_item_set_int, "8"},
    {NULL,          CCACHE_ITEM_INVALID,  -1, NULL, NULL}
};

static int  ccache_init_defconfig();
static int  ccache_read_config(const char *configfile);
static void ccache_process_section(char *buffer, int *state);
static void ccache_process_item(char *buffer, int *state);

int 
ccache_init_config(const char *configfile)
{
    memset((void*)&ccache_config, CCACHE_ITEM_UNSET, sizeof(ccache_config_t));

    if (!configfile)
    {
        if (0 > ccache_read_config(configfile))
        {
            return -1;
        }
    }

    return ccache_init_defconfig();
}

int
ccache_init_defconfig()
{
    int i;
    char *p;
    int  *np;
    char *conf = (char*)(&ccache_config);
    ccache_conf_item_t *item;

    for (i = 0; ccache_conf_items[i].name; ++i)
    {
        item = &(ccache_conf_items[i]);

        if (item->type == CCACHE_ITEM_STRING) 
        {
            p = conf + item->offset;
            if (p == (char*)CCACHE_ITEM_UNSET)
            {
                item->set(item->defval, item->offset);
            }
        }
        else if (item->type == CCACHE_ITEM_BOOL)
        {
            p = conf + item->offset;
            if (*p == (char)CCACHE_ITEM_UNSET)
            {
                item->set(item->defval, item->offset);
            }
        }
        else if (item->type == CCACHE_ITEM_INT)
        {
            np = (int*)(conf + item->offset);
            if (*p == (int)CCACHE_ITEM_UNSET)
            {
                item->set(item->defval, item->offset);
            }
        }
    }

    if (!ccache_ispowerof2(ccache_config.align_size))
    {
        fprintf(stderr, "align size must be power of 2!\n");
        return -1;
    }

    ccache_align_size = ccache_config.align_size;

    return 0;
}

int 
ccache_read_config(const char *configfile)
{
    int state = CCACHE_READ_SECTION;
    FILE *file;
    char buffer[CCACHE_CONFIG_MAXLEN];

    if(!(file = fopen(configfile, "r")))
    {
        fprintf(stderr, "open config file %s error: %s\n",
                configfile, strerror(errno));
        return -1;
    }

    while (state != CCACHE_READ_ERROR && fgets(buffer, sizeof(buffer), file))
    {
        buffer[strlen(buffer)] = '\0';
        ccache_string_trim(buffer);

        if (state == CCACHE_READ_SECTION)
        {
            ccache_process_section(buffer, &state);
        }
        else
        {
            ccache_process_item(buffer, &state);
        }
    }

    fclose(file);

    if (state == CCACHE_READ_ERROR)
    {
        return -1;
    }
    
    return 0;
}

void 
ccache_process_section(char *buffer, int *state)
{
    if (!strncmp(buffer, CCACHE_SECTION_NAME, strlen(CCACHE_SECTION_NAME)))
    {
        *state = CCACHE_READ_ITEM;
        return;
    }

    *state = CCACHE_READ_ERROR;
}

void 
ccache_process_item(char *buffer, int *state)
{
    int i;
    ccache_conf_item_t *item;

    for (i = 0; ccache_conf_items[i].name; ++i)
    {
        item = &(ccache_conf_items[i]);

        if (!strncmp(buffer, item->name, strlen(item->name)))
        {
            buffer += strlen(item->name);
            ccache_string_trim(buffer);
            if (*buffer != '=')
            {
                fprintf(stderr, "process item %s error!\n", item->name);
                break;
            }
            buffer++;
            ccache_string_trim(buffer);

            item->set(buffer, item->offset);
        }
    }

    *state = CCACHE_READ_ERROR;
}

int 
ccache_item_set_string(char *buffer, int offset)
{
    char *p = (char*)(&ccache_config) + offset;

    p = (char*)malloc(strlen(buffer) * sizeof(char*));
    strcpy(p, buffer);

    return 1;
}

int 
ccache_item_set_int(char *buffer, int offset)
{
    int *np = (int *)((char*)(&ccache_config) + offset);
    char *p = buffer;

    while (p && *p)
    {
        if (!isdigit(*p))
        {
            return -1;
        }
    }

    *np = atoi(buffer);

    return 0;
}

int 
ccache_item_set_bool(char *buffer, int offset)
{
    char *np = (char*)(&ccache_config) + offset;

    if (!strcmp(buffer, "1"))
    {
        *np = 1;
    }
    else if (!strcmp(buffer, "0"))
    {
        *np = 0;
    }
    else
    {
        return -1;
    }

    return 0;
}

