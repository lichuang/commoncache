/********************************************************************
	created:	2008/01/23
	filename: 	lrulist.c
	author:		Lichuang
                
	purpose:    
*********************************************************************/

#include "ccache_lock.h"
#include "ccache_lrulist.h"
#include "ccache_memory.h"
#include "ccache_node.h"

void 
ccache_lrulist_advance(ccache_t *cache, ccache_node_t *node)
{
    ccache_freearea_t *freearea = &(cache->freearea[node->freeareaid]);
    ccache_node_t *lruprev, *lrunext, *lruprevprev;

    if (!freearea->lrufirst && !freearea->lrulast)
    {
        freearea->lrufirst = freearea->lrulast = node;
        return;
    }
    if (freearea->lrufirst == node)
    {
        return;
    }
    lruprev = node->lruprev, lrunext = node->lrunext, lruprevprev = NULL;
    if (!lruprev && !lrunext)
    {
        node->lruprev = freearea->lrulast;
        freearea->lrulast = node;
        return;
    }
    if (NULL != lruprev)
    {
        lruprev->lrunext = lrunext;
        lruprevprev = lruprev->lruprev;
        lruprev->lruprev = node;
    }
    if (NULL != lrunext)
    {
        lrunext->lruprev = lruprev;
    }

    node->lrunext = lruprev;
    node->lruprev = lruprevprev;
    if (NULL != lruprevprev)
    {
        lruprevprev->lrunext = node;
    }

    if (node == freearea->lrulast)
    {
        freearea->lrulast = lruprev;
    }

    if (lruprev == freearea->lrufirst)
    {
        freearea->lrufirst = node;
    }
}

int  
ccache_lrulist_free(ccache_t *cache, ccache_node_t *node)
{
    ccache_node_t *lrunext = node->lrunext, *lruprev = node->lruprev;
    ccache_freearea_t* freearea = &(cache->freearea[node->freeareaid]);

    if (NULL != lrunext)
    {
        lrunext->lruprev = lruprev;
    }
    if (NULL != lruprev)
    {
        lruprev->lrunext = lrunext;
    }

    if (node == freearea->lrufirst)
    {
        freearea->lrufirst = lrunext;
    }
    if (node == freearea->lrulast)
    {
        freearea->lrulast = lruprev; 
    }

    return 0;
}

int  
ccache_lrulist_return(ccache_t *cache, ccache_node_t *node)
{
    ccache_freearea_t* freearea = &(cache->freearea[node->freeareaid]);
    if (node == freearea->lrulast)
    {
        return 0;
    }

    if (NULL != freearea->lrulast)
    {
        freearea->lrulast->lrunext = node;
    }
    node->lruprev = freearea->lrulast;
    node->lrunext = NULL;
    freearea->lrulast = node;

    return 0;    
}

