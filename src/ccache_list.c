/********************************************************************
	created:	2008/01/23
	filename: 	list.c
	author:		Lichuang
                
	purpose:    
*********************************************************************/

#include "ccache.h"
#include "ccache_list.h"
#include "ccache_node.h"
#include "ccache_hash.h"
#include "ccache_memory.h"
#include "ccache_error.h"
#include <string.h>

#ifdef CCACHE_USE_LIST

static ccache_node_t* ccache_list_find(int hashindex, const ccache_data_t* data, 
                                        ccache_t* cache, ccache_compare_t compare);

static ccache_node_t* ccache_list_insert(int hashindex, const ccache_data_t* data, 
                                        ccache_t* cache, ccache_compare_t compare,
                                        ccache_erase_t erase, void* arg, int* exist);

static ccache_node_t* ccache_list_update(int hashindex, const ccache_data_t* data, 
                                        ccache_t* cache, ccache_compare_t compare);

static ccache_node_t* ccache_list_set(int hashindex, ccache_data_t* data, 
                                        ccache_t* cache, ccache_compare_t compare, 
                                        ccache_erase_t erase, void* arg, ccache_update_t update);

static ccache_node_t* ccache_list_erase(int hashindex, ccache_node_t* node, ccache_t* cache);

static void           ccache_list_visit(ccache_t* cache, int hashindex, ccache_visit_t visit, void* arg);

#endif

int 
ccache_init_list_functor(ccache_functor_t *functor)
{
#ifdef CCACHE_USE_LIST
    functor->find       = ccache_list_find;
    functor->insert     = ccache_list_insert;
    functor->update     = ccache_list_update;
    functor->erase      = ccache_list_erase;
    functor->visit      = ccache_list_visit;
    functor->set    = ccache_list_set;

    return 0;
#else
    (void)functor;
    return -1;
#endif    
}

#ifdef CCACHE_USE_LIST

static ccache_node_t* 
ccache_list_find_auxiliary(int hashindex, const ccache_data_t* data, 
                    ccache_t* cache, ccache_compare_t compare)
{
    ccache_hash_t *hashitem = &cache->hashitem[hashindex];
    char* key = data->key;
    int len = data->keysize;
    ccache_node_t* node = hashitem->first;

    while (node)
    {
        if (len == node->keysize && !compare(key, CCACHE_NODE_KEY(node), len))
        {
            break;
        }

        node = node->next;
    }

    return node;
}

static void 
ccache_list_init_node(ccache_node_t* node, int hashindex, const ccache_data_t* data, ccache_t* cache)
{
    ccache_node_t* first;
    ccache_hash_t* hashitem = &cache->hashitem[hashindex];

    node->keysize = data->keysize;
    node->datasize = data->datasize;
    node->hashindex = hashindex;
    node->lrunext = node->lruprev = NULL;

    memcpy(CCACHE_NODE_KEY(node), data->key, node->keysize);
    memcpy(CCACHE_NODE_DATA(node), data->data, node->datasize);

    hashitem->nodenum++;

    first = hashitem->first;
    if (first)
    {
        first->prev = node;
    }
    node->next = first;
    node->prev = NULL;

    hashitem->first = node;
}

static ccache_node_t* 
ccache_list_insert_auxiliary(int hashindex, const ccache_data_t* data, ccache_t* cache,
                            ccache_erase_t erase, void* arg)
{
    int nodesize = ccache_count_nodesize(data);
    ccache_node_t* node = ccache_allocate(cache, nodesize, erase, arg);

    if (!node)
    {
        CCACHE_SET_ERROR_NUM(CCACHE_ALLOC_NODE_ERROR);
        return NULL;
    }

    ccache_list_init_node(node, hashindex, data, cache);

    return node;
}

static void 
ccache_list_advance(ccache_node_t* node, int hashindex, ccache_t* cache)
{
    ccache_hash_t* hashitem = &cache->hashitem[hashindex];
    ccache_node_t *prev, *next, *prevprev;

    if (node == hashitem->first)
    {
        return;
    }

    prev = node->prev, next = node->next, prevprev = NULL;
    if (prev)
    {
        prev->next = next;
        prevprev = prev->prev;
        prev->prev = node;
    }
    if (next)
    {
        next->prev = prev;
    }

    node->next = prev;
    node->prev = prevprev;
    if (prevprev)
    {
        prevprev->next = node;
    }

    if (prev == hashitem->first)
    {
        hashitem->first = node;
    }
}

ccache_node_t* 
ccache_list_find(int hashindex, const ccache_data_t* data, ccache_t* cache, ccache_compare_t compare)
{
    ccache_node_t* node = ccache_list_find_auxiliary(hashindex, data, cache, compare);

    if (node)
    {
        ccache_list_advance(node, hashindex, cache);
    }

    return node;
}

ccache_node_t* 
ccache_list_insert(int hashindex, const ccache_data_t* data, ccache_t* cache,
                    ccache_compare_t compare, ccache_erase_t erase, void* arg, int* exist)
{
    ccache_node_t *node;

    node = ccache_list_find_auxiliary(hashindex, data, cache, compare);
    if (node)
    {
        *exist = 1;
        CCACHE_SET_ERROR_NUM(CCACHE_KEY_EXIST);
        return node;
    }
    else
    {
        return ccache_list_insert_auxiliary(hashindex, data, cache, erase, arg);
    }
}

ccache_node_t* 
ccache_list_update(int hashindex, const ccache_data_t* data, ccache_t* cache, ccache_compare_t compare)
{
    ccache_node_t* node = ccache_list_find_auxiliary(hashindex, data, cache, compare);

    if (!node)
    {
        CCACHE_SET_ERROR_NUM(CCACHE_KEY_NOT_EXIST);
        return NULL;
    }

    memcpy(CCACHE_NODE_DATA(node), data->data, node->datasize);
    ccache_list_advance(node, hashindex, cache);

    return node;
}

ccache_node_t* 
ccache_list_set(int hashindex, ccache_data_t* data, struct ccache_t* cache,
                    ccache_compare_t compare, ccache_erase_t erase, void* arg, ccache_update_t update)
{
    ccache_node_t* node = ccache_list_find_auxiliary(hashindex, data, cache, compare);

    if (node)
    {
        update(node, data);

        memcpy(CCACHE_NODE_DATA(node), data->data, node->datasize);
        ccache_list_advance(node, hashindex, cache);
    }
    else
    {
        node = ccache_list_insert_auxiliary(hashindex, data, cache, erase, arg);
    }

    return node;
}

ccache_node_t* 
ccache_list_erase(int hashindex, ccache_node_t* node, ccache_t* cache)
{
    ccache_node_t *prev, *next;
    ccache_hash_t* hashitem;

    prev = node->prev, next = node->next;
    if (prev)
    {
        prev->next = next;
    }
    if (next)
    {
        next->prev = prev;
    }

    hashitem = &cache->hashitem[hashindex];
    if (node == hashitem->first)
    {
        hashitem->first = next; 
    }

    hashitem->nodenum--;

    node->prev = node->next = NULL;
    
    return node;
}

void 
ccache_list_visit(ccache_t* cache, int hashindex, ccache_visit_t visit, void* arg)
{
    ccache_hash_t* hashitem = &cache->hashitem[hashindex];
    ccache_node_t* node;

    for (node = hashitem->first; node; node = node->next)
    {
        visit(arg, node);
    }
}

#endif

