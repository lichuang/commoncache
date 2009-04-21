/********************************************************************
	created:	2008/01/23
	filename: 	ccache.c
	author:		Lichuang
                
	purpose:    
*********************************************************************/

#include "ccache.h"
#include "ccache_lock.h"
#include "ccache_lrulist.h"
#include "ccache_functor.h"
#include "ccache_memory.h"
#ifdef CCACHE_USE_LIST
    #include "ccache_list.h"
#elif defined CCACHE_USE_RBTREE    
    #include "ccache_rbtree.h"
#endif

#include <string.h>

static int ccache_count_cache_size(int datasize, int hashitemnum);

ccache_t* 
ccache_create(int datasize, int hashitemnum, const char* mapfilename, int min_size, int max_size, int init)
{
    int filesize = ccache_count_cache_size(datasize, hashitemnum);
    ccache_t* cache = ccache_create_mmap(filesize, mapfilename, &init);
    
    if (NULL == cache)
    {
        CCACHE_SET_ERROR_NUM(CCACHE_NULL_POINTER);
        return NULL;
    }

    /* no matter if or not the init is set, we will init the thread lock */
    if (0 > ccache_init_thread_rwlock(&(cache->lock)))
    {
        CCACHE_SET_ERROR_NUM(CCACHE_INIT_ERROR);
        return NULL;
    }

    if (init)
    {
        cache->filesize = filesize;
        cache->hashitemnum = hashitemnum;

        cache->datasize = datasize;

        if (0 > ccache_init_functor(&(cache->functor)))
        {
            CCACHE_SET_ERROR_NUM(CCACHE_INIT_ERROR);
            return NULL;
        }
        if (0 > ccache_init_hashitem(cache))
        {
            CCACHE_SET_ERROR_NUM(CCACHE_INIT_ERROR);
            return NULL;
        }
        if (0 > ccache_init_freearea(cache, datasize, min_size, max_size))
        {
            CCACHE_SET_ERROR_NUM(CCACHE_INIT_ERROR);
            return NULL;
        }
    }

    CCACHE_SET_SUCCESS;

    return cache;        
}

void 
ccache_destroy(ccache_t* cache)
{
    pthread_rwlock_destroy(&(cache->lock));
    ccache_destroy_mmap(cache);
}

int 
ccache_insert(const ccache_data_t* data, ccache_t* cache, ccache_compare_t compare,
            ccache_erase_t erase, void* arg)
{
    int hashindex = ccache_hash(data->key, data->keysize, cache), exist;
    ccache_node_t* node;

    if (0 > pthread_rwlock_wrlock(&(cache->lock)))
    {
        CCACHE_SET_ERROR_NUM(CCACHE_LOCK_ERROR);
        return -1;
    }

    cache->stat.insert_stat.total_num++;

    exist = 0;
    node = cache->functor.insert(hashindex, data, cache, compare, erase, arg, &exist);

    if (!node || exist)
    {
        cache->stat.insert_stat.fail_num++;
        node = NULL;
    }
    else
    {
        cache->stat.insert_stat.success_num++;
        ccache_lrulist_advance(node, cache);
    }

    if (0 > pthread_rwlock_unlock(&(cache->lock)))
    {
        CCACHE_SET_ERROR_NUM(CCACHE_LOCK_ERROR);
        return -1;
    }

    if (!node) 
    {
        return -1;
    }
    else
    {
        CCACHE_SET_SUCCESS;
        return 0;
    }
}

int 
ccache_find(ccache_data_t* data, ccache_t* cache, ccache_compare_t compare)
{
    int hashindex = ccache_hash(data->key, data->keysize, cache);
    ccache_node_t* node;

    if (0 > pthread_rwlock_rdlock(&(cache->lock)))
    {
        CCACHE_SET_ERROR_NUM(CCACHE_LOCK_ERROR);
        return -1;
    }

    cache->stat.find_stat.total_num++;
    node = cache->functor.find(hashindex, data, cache, compare);

    if (node)
    {
        memcpy(data->data, CCACHE_NODE_DATA(node), node->datasize);
        ccache_lrulist_advance(node, cache);
        cache->stat.find_stat.success_num++;
    }
    else
    {
        cache->stat.find_stat.fail_num++;
    }

    if (0 > pthread_rwlock_unlock(&(cache->lock)))
    {
        CCACHE_SET_ERROR_NUM(CCACHE_LOCK_ERROR);
        return -1;
    }

    if (!node) 
    {
        return -1;
    }
    else
    {
        CCACHE_SET_SUCCESS;
        return 0;
    }
}

int 
ccache_update(const ccache_data_t* data, ccache_t* cache, ccache_compare_t compare)
{
    int hashindex = ccache_hash(data->key, data->keysize, cache);
    ccache_node_t* node;

    if (0 > pthread_rwlock_wrlock(&(cache->lock)))
    {
        CCACHE_SET_ERROR_NUM(CCACHE_LOCK_ERROR);
        return -1;
    }

    cache->stat.update_stat.total_num++;
    node = cache->functor.update(hashindex, data, cache, compare);

    if (node)
    {
        cache->stat.update_stat.success_num++;
        ccache_lrulist_advance(node, cache);
    }
    else
    {
        cache->stat.update_stat.fail_num++;
    }

    if (0 > pthread_rwlock_unlock(&(cache->lock)))
    {
        CCACHE_SET_ERROR_NUM(CCACHE_LOCK_ERROR);
        return -1;
    }

    if (!node) 
    {
        return -1;
    }
    else
    {
        CCACHE_SET_SUCCESS;
        return 0;
    }
}

int 
ccache_erase(ccache_data_t* data, ccache_t* cache, ccache_compare_t compare)
{
    int hashindex = ccache_hash(data->key, data->keysize, cache);
    ccache_node_t* node;

    if (0 > pthread_rwlock_wrlock(&(cache->lock)))
    {
        CCACHE_SET_ERROR_NUM(CCACHE_LOCK_ERROR);
        return -1;
    }

    cache->stat.erase_stat.total_num++;

    node = cache->functor.find(hashindex, data, cache, compare);

    if (!node)
    {
        cache->stat.erase_stat.fail_num++;
        CCACHE_SET_ERROR_NUM(CCACHE_KEY_NOT_EXIST);
        pthread_rwlock_unlock(&(cache->lock));
        return -1;
    }

    node = cache->functor.erase(hashindex, node, cache);

    if (node)
    {
        if (data->data)
        {
            memcpy(data->data, CCACHE_NODE_DATA(node), node->datasize);
        }

        ccache_lrulist_return(node, cache);
        node->hashindex = CCACHE_INVALID_HASHINDEX;
        cache->stat.erase_stat.success_num++;
    }
    else
    {
        cache->stat.erase_stat.fail_num++;
    }

    if (0 > pthread_rwlock_unlock(&(cache->lock)))
    {
        CCACHE_SET_ERROR_NUM(CCACHE_LOCK_ERROR);
        return -1;
    }

    if (!node) 
    {
        return -1;
    }
    else
    {
        CCACHE_SET_SUCCESS;
        return 0;
    }
}

int 
ccache_replace(ccache_data_t* data, ccache_t* cache, ccache_compare_t compare,
                ccache_erase_t erase, void* arg, ccache_update_t update)
{
    int hashindex = ccache_hash(data->key, data->keysize, cache);
    ccache_node_t* node;

    if (0 > pthread_rwlock_wrlock(&(cache->lock)))
    {
        CCACHE_SET_ERROR_NUM(CCACHE_LOCK_ERROR);
        return -1;
    }

    cache->stat.replace_stat.total_num++;
    node = cache->functor.replace(hashindex, data, cache, compare, erase, arg, update);

    if (node)
    {
        cache->stat.replace_stat.success_num++;
        ccache_lrulist_advance(node, cache);
    }
    else
    {
        cache->stat.replace_stat.fail_num++;
    }

    if (0 > pthread_rwlock_unlock(&(cache->lock)))
    {
        CCACHE_SET_ERROR_NUM(CCACHE_LOCK_ERROR);
        return -1;
    }

    if (!node) 
    {
        return -1;
    }
    else
    {
        CCACHE_SET_SUCCESS;
        return 0;
    }
}

int 
ccache_visit(ccache_t* cache, ccache_visit_t visit, void* arg)
{
    int hashindex;

    if (0 > pthread_rwlock_rdlock(&(cache->lock)))
    {
        CCACHE_SET_ERROR_NUM(CCACHE_LOCK_ERROR);
        return -1;
    }

    for (hashindex = 0; hashindex < cache->hashitemnum; ++hashindex)
    {
        cache->functor.visit(cache, hashindex, visit, arg);
    }

    if (0 > pthread_rwlock_unlock(&(cache->lock)))
    {
        CCACHE_SET_ERROR_NUM(CCACHE_LOCK_ERROR);
        return -1;
    }

    return 0;
}

int 
ccache_count_cache_size(int datasize, int hashitemnum)
{
    return (sizeof(struct ccache_t)
            + datasize
            + hashitemnum * sizeof(struct ccache_hash_t));
}

