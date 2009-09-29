/********************************************************************
	created:	2008/01/23
	filename: 	hash.h
	author:		Lichuang
                
	purpose:    
*********************************************************************/

#ifndef __CCACHE_HASH_H__
#define __CCACHE_HASH_H__

#include "ccache_node.h"

struct ccache_t;

typedef struct ccache_hash_t
{
    union
    {
        ccache_node_t* first;
        ccache_node_t* root;
    };

    int nodenum;
}ccache_hash_t;

int ccache_hash(const void* key, int keysize, struct ccache_t *cache);
int ccache_init_hashitem(struct ccache_t *cache);

#define CCACHE_INVALID_HASHINDEX   -1

#endif /* __CCACHE_HASH_H__ */

