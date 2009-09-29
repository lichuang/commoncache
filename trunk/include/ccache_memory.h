/********************************************************************
	created:	2008/01/23
	filename: 	memory.h
	author:		Lichuang
                
	purpose:    
*********************************************************************/

#ifndef __CCACHE_MEMORY_H__
#define __CCACHE_MEMORY_H__

#include "ccache.h"

typedef struct ccache_freearea_t
{
    ccache_node_t *lrufirst;
    ccache_node_t *lrulast;
    int size;
}ccache_freearea_t;

ccache_t*       ccache_create_mmap(int filesize, const char *mapfilename, int *init);
int             ccache_destroy_mmap(ccache_t *cache);
ccache_node_t*  ccache_allocate(ccache_t *cache, int size, ccache_erase_t erase, void* arg);
int             ccache_init_freearea(ccache_t *cache, int datasize, int min_size, int max_size);

#endif /* __CCACHE_MEMORY_H__ */

