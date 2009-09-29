/********************************************************************
	created:	2008/01/24
	filename: 	hash.c
	author:		Lichuang
                
	purpose:    
*********************************************************************/

#include "ccache_hash.h"
#include "ccache.h"
#include <string.h>

int 
ccache_hash(const void* key, int keysize, ccache_t *cache)
{
    int index, i;
    unsigned int hash = 0, tempindex = 0;
    unsigned int* temp = (unsigned int *)key;
    char *byte = NULL;

    for (i = 0; i < (int)(keysize / sizeof(unsigned int)); i++)
        hash += temp[i];

    if (keysize % sizeof(unsigned int) > 0)
    {
        byte = (char *)key;
        byte += keysize - (keysize % sizeof(unsigned int));
        memcpy((void *)&tempindex, (const void *)byte, keysize % sizeof(unsigned int));
    }

    hash += tempindex;
    index = (int)(hash & ((unsigned int)0x7fffffff));

    return index % cache->hashitemnum;
}

int 
ccache_init_hashitem(ccache_t *cache)
{
    int hashitemnum = cache->hashitemnum, i;

    for (i = 0; i < hashitemnum; ++i)
    {
#ifdef CCACHE_USE_LIST
		cache->hashitem[i].first = NULL;
#elif defined CCACHE_USE_RBTREE    
		cache->hashitem[i].root = NULL;
#endif		
        
        cache->hashitem[i].nodenum = 0;
    }

    return 0;
}

