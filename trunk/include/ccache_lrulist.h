/********************************************************************
	created:	2008/01/23
	filename: 	lrulist.c
	author:		Lichuang
                
	purpose:    
*********************************************************************/

#ifndef __CCACHE_LRULIST_H__
#define __CCACHE_LRULIST_H__

#include "ccache.h"

void ccache_lrulist_advance(ccache_node_t *node, ccache_t *cache);
int  ccache_lrulist_free(ccache_node_t *node, ccache_t *cache);
int  ccache_lrulist_return(ccache_node_t *node, ccache_t *cache);

#endif /* __CCACHE_LRULIST_H__ */

