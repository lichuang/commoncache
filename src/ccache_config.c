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

ccache_config_t cache_config;
int cache_align_size;

#define CCACHE_CONFIG_MAXLEN 50
#define CCACHE_SECTION_NAME "[ccache_conf]"

enum 
{
    CCACHE_READ_SECTION,
    CCACHE_READ_ITEM,
    CCACHE_READ_ERROR
};

typedef enum ccache_itemtype_t
{
    CCACHE_ITEM_UNSET   = -1,
    CCACHE_ITEM_STRING,
    CCACHE_ITEM_INT,
    CCACHE_ITEM_BOOL,
}ccache_itemtype_t;

typedef struct ccache_conf_item_t
{
    char *name;
    ccache_itemtype_t type;
    int offset;
    int (*set)(void *p, char *buffer);
    char *defval;
}ccache_conf_item_t;

static int ccache_item_set_string(void *p, char *buffer);
static int ccache_item_set_int(void *p, char *buffer);
static int ccache_item_set_bool(void *p, char *buffer);

static ccache_conf_item_t ccache_conf_items[] = 
{
    {"mapfile",     CCACHE_ITEM_STRING,   offsetof(ccache_config_t, path),      ccache_item_set_string, "./ccache_mapfile"},
    {"min_size",    CCACHE_ITEM_INT,      offsetof(ccache_config_t, min_size),  ccache_item_set_int,    "16"},
    {"max_size",    CCACHE_ITEM_INT,      offsetof(ccache_config_t, max_size),  ccache_item_set_int,    "32"},
    {"hashitem",    CCACHE_ITEM_INT,      offsetof(ccache_config_t, hashitem),  ccache_item_set_int,    "1000"},
    {"datasize",    CCACHE_ITEM_INT,      offsetof(ccache_config_t, datasize),  ccache_item_set_int,    "1000000"},
    {"prealloc_num",CCACHE_ITEM_INT,      offsetof(ccache_config_t, prealloc_num), ccache_item_set_int, "100"},
    {"alignsize",   CCACHE_ITEM_INT,      offsetof(ccache_config_t, align_size), ccache_item_set_int, "8"},
    {"init",        CCACHE_ITEM_BOOL,     offsetof(ccache_config_t, init),      ccache_item_set_bool, "1"},
    {NULL,          CCACHE_ITEM_UNSET,    -1,                       NULL,       NULL}
};

static int  ccache_init_defconfig(char init);
static int  ccache_read_config(const char *configfile);
static void ccache_process_section(char *buffer, int *state);
static void ccache_process_item(char *buffer, int *state);

int 
ccache_init_config(const char *configfile)
{
    ccache_init_defconfig(1);

    if (configfile)
    {
        if (0 > ccache_read_config(configfile))
        {
            return -1;
        }
    }

    return ccache_init_defconfig(0);
}

int
ccache_init_defconfig(char init)
{
    int i;
    char *p, **pp;
    int  *np;
    char *conf = (char*)(&cache_config);
    ccache_conf_item_t *item;

    for (i = 0; ccache_conf_items[i].name; ++i)
    {
        item = &(ccache_conf_items[i]);

        if (item->type == CCACHE_ITEM_INT)
        {
            np = (int*)(conf + item->offset);

            if (init)
            {
                *np = (int)CCACHE_ITEM_UNSET;
            }
            else if (*np == (int)CCACHE_ITEM_UNSET)
            {
                item->set(np, item->defval);
            }
        }
        else if (item->type == CCACHE_ITEM_STRING) 
        {
            pp = (char**)(conf + item->offset);

            if (init)
            {
                *pp = (char*)CCACHE_ITEM_UNSET;
            }
            else if (*pp == (char*)CCACHE_ITEM_UNSET)
            {
                item->set(pp, item->defval);
            }
        }
        else if (item->type == CCACHE_ITEM_BOOL)
        {
            p = conf + item->offset;

            if (init)
            {
                *p = (char)CCACHE_ITEM_UNSET;
            }
            else if (*p == (char)CCACHE_ITEM_UNSET)
            {
                item->set(p, item->defval);
            }
        }
    }

    if (init)
    {
        return 0;
    }

    if (!ccache_ispowerof2(cache_config.align_size))
    {
        fprintf(stderr, "align size must be power of 2!\n");
        return -1;
    }

    cache_align_size = cache_config.align_size;

    return 0;
}

int 
ccache_read_config(const char *configfile)
{
    int state = CCACHE_READ_SECTION;
    FILE *file;
    char buffer[CCACHE_CONFIG_MAXLEN];
    int len;

    if(!(file = fopen(configfile, "r")))
    {
        fprintf(stderr, "open config file %s error: %s\n",
                configfile, strerror(errno));
        return -1;
    }

    while (state != CCACHE_READ_ERROR && fgets(buffer, sizeof(buffer), file))
    {
        if ((len = strlen(buffer)) <= 1)
        {
            continue;
        }

        /* lines begin with # is comment, ignore parsing */
        if (*buffer == '#')
        {
            continue;
        }

        buffer[len - 1]= '\0';
        ccache_string_trim(buffer);

        if (state == CCACHE_READ_ITEM)
        {
            ccache_process_item(buffer, &state);
        }
        else
        {
            ccache_process_section(buffer, &state);
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
    char *p;

    for (i = 0, p = buffer; ccache_conf_items[i].name; ++i)
    {
        item = &(ccache_conf_items[i]);
        buffer = p;

        if (!strncmp(buffer, item->name, strlen(item->name)))
        {
            buffer += strlen(item->name);

            while (isspace(*buffer))
            {
                buffer++;
            }
            if (*buffer != '=')
            {
                continue;
            }

            buffer++;

            while (isspace(*buffer))
            {
                buffer++;
            }

            item->set(((char*)&cache_config + item->offset), buffer);
            return;
        }
    }

    *state = CCACHE_READ_ERROR;
}

int 
ccache_item_set_string(void *p, char *buffer)
{
    char **pp = (char**)p;
    *pp = (char*)malloc(strlen(buffer) * sizeof(char*));
    strcpy(*pp, buffer);

    return 1;
}

int 
ccache_item_set_int(void *p, char *buffer)
{
    int *np = (int *)p;
    char *pos;

    for (pos = buffer; pos && *pos; pos++)
    {
        if (!isdigit(*pos))
        {
            return -1;
        }
    }

    *np = atoi(buffer);

    return 0;
}

int 
ccache_item_set_bool(void *p, char *buffer)
{
    char *np = (char*)p;

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

