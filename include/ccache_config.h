
#ifndef __CCACHE_CONFIG_H__
#define __CCACHE_CONFIG_H__

typedef struct ccache_config_t
{
    char    *path;          /* the map file path */
    int     min_size;
    int     max_size;
    int     hashitem;
    int     datasize;
    char    prealloc;
    int     prealloc_num;
    int     align_size;
} ccache_config_t;

extern ccache_config_t ccache_config;
extern int ccache_align_size;

int ccache_init_config(const char *configfile);

#endif /* __CCACHE_CONFIG_H__ */

