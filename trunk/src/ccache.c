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
#include "ccache_config.h"

#ifdef CCACHE_USE_LIST
    #include "ccache_list.h"
#elif defined CCACHE_USE_RBTREE    
    #include "ccache_rbtree.h"
#else
    #error MUST define CCACHE_USE_LIST or CCACHE_USE_RBTREE in makefile
#endif

#include <string.h>

ccache_compare_t cache_compare;
static int ccache_count_cache_size(int datasize, int hashitemnum);
static int ccache_compare_key(const void* data1, const void* data2, int len);

ccache_t* 
ccache_open(const char *configfile, ccache_compare_t compare)
{
    if (ccache_init_config(configfile) < 0)
    {
        CCACHE_SET_ERROR_NUM(CCACHE_INIT_ERROR);
        return NULL;
    }

    return ccache_open2(&cache_config, compare);
}

ccache_t* 
ccache_open2(ccache_config_t *config, ccache_compare_t compare)
{
    int filesize;
    ccache_t *cache;

    if (!config && ccache_init_config(NULL) < 0)
    {
        CCACHE_SET_ERROR_NUM(CCACHE_INIT_ERROR);
        return NULL;
    }

    if (config)
    {
      cache_config = *config;
    }
    filesize = ccache_count_cache_size(cache_config.datasize, cache_config.hashitem);
    cache = ccache_create_mmap(filesize, cache_config.path, &(cache_config.init));
    
    if (!cache)
    {
        CCACHE_SET_ERROR_NUM(CCACHE_NULL_POINTER);
        return NULL;
    }

    /* no matter if or not the init is set, we will init the thread lock */
    if (ccache_init_thread_rwlock(&(cache->lock)) < 0)
    {
        CCACHE_SET_ERROR_NUM(CCACHE_INIT_ERROR);
        return NULL;
    }

    if (cache_config.init)
    {
        cache->filesize = filesize;
        cache->hashitemnum = cache_config.hashitem;

        cache->datasize = cache_config.datasize;

        if (ccache_init_functor(&(cache->functor)) < 0)
        {
            CCACHE_SET_ERROR_NUM(CCACHE_INIT_ERROR);
            return NULL;
        }
        if (ccache_init_hashitem(cache) < 0)
        {
            CCACHE_SET_ERROR_NUM(CCACHE_INIT_ERROR);
            return NULL;
        }
        if (ccache_init_freearea(cache, cache_config.datasize, cache_config.min_size, cache_config.max_size) < 0)
        {
            CCACHE_SET_ERROR_NUM(CCACHE_INIT_ERROR);
            return NULL;
        }
    }

    if (!compare)
    {
        cache_compare = ccache_compare_key;
    }
    else
    {
        cache_compare = compare;
    }

    CCACHE_SET_SUCCESS;

    return cache;        
}

void 
ccache_close(ccache_t *cache)
{
    pthread_rwlock_destroy(&(cache->lock));
    ccache_destroy_mmap(cache);
}

int 
ccache_insert(ccache_t *cache, const ccache_data_t *data,
            ccache_erase_t erase, void* arg)
{
    int hashindex = ccache_hash(cache, data->key, data->keysize), exist;
    ccache_node_t *node;

    if (pthread_rwlock_wrlock(&(cache->lock)) < 0)
    {
        CCACHE_SET_ERROR_NUM(CCACHE_LOCK_ERROR);
        return -1;
    }

    cache->stat.insert_stat.total_num++;

    exist = 0;
    node = cache->functor.insert(cache, hashindex, data, erase, arg, &exist);

    //if (!node || exist)
    if (!node)
    {
        cache->stat.insert_stat.fail_num++;
        node = NULL;
    }
    else
    {
        cache->stat.insert_stat.success_num++;
        ccache_lrulist_advance(cache, node);
    }

    if (pthread_rwlock_unlock(&(cache->lock)) < 0)
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
ccache_find(ccache_t *cache, ccache_data_t *data)
{
    int hashindex = ccache_hash(cache, data->key, data->keysize);
    ccache_node_t *node;

    if (pthread_rwlock_wrlock(&(cache->lock)) < 0)
    {
        CCACHE_SET_ERROR_NUM(CCACHE_LOCK_ERROR);
        return -1;
    }

    cache->stat.find_stat.total_num++;
    node = cache->functor.find(cache, hashindex, data);

    if (node)
    {
        memcpy(data->data, CCACHE_NODE_DATA(node), node->datasize);
        ccache_lrulist_advance(cache, node);
        cache->stat.find_stat.success_num++;
    }
    else
    {
        cache->stat.find_stat.fail_num++;
    }

    if (pthread_rwlock_unlock(&(cache->lock)) < 0)
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
ccache_update(ccache_t *cache, const ccache_data_t *data)
{
    int hashindex = ccache_hash(cache, data->key, data->keysize);
    ccache_node_t *node;

    if (pthread_rwlock_wrlock(&(cache->lock)) < 0)
    {
        CCACHE_SET_ERROR_NUM(CCACHE_LOCK_ERROR);
        return -1;
    }

    cache->stat.update_stat.total_num++;
    node = cache->functor.update(cache, hashindex, data);

    if (node)
    {
        cache->stat.update_stat.success_num++;
        ccache_lrulist_advance(cache, node);
    }
    else
    {
        CCACHE_SET_ERROR_NUM(CCACHE_KEY_EXIST);
        cache->stat.update_stat.fail_num++;
    }

    if (pthread_rwlock_unlock(&(cache->lock)) < 0)
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
ccache_erase(ccache_t *cache, ccache_data_t *data)
{
    int hashindex = ccache_hash(cache, data->key, data->keysize);
    ccache_node_t *node;

    if (pthread_rwlock_wrlock(&(cache->lock)) < 0)
    {
        CCACHE_SET_ERROR_NUM(CCACHE_LOCK_ERROR);
        return -1;
    }

    cache->stat.erase_stat.total_num++;

    node = cache->functor.find(cache, hashindex, data);

    if (!node)
    {
        cache->stat.erase_stat.fail_num++;
        CCACHE_SET_ERROR_NUM(CCACHE_KEY_NOT_EXIST);
        pthread_rwlock_unlock(&(cache->lock));
        return -1;
    }

    node = cache->functor.erase(cache, hashindex, node);

    if (node)
    {
        if (data->data)
        {
            memcpy(data->data, CCACHE_NODE_DATA(node), node->datasize);
        }

        ccache_lrulist_return(cache, node);
        node->hashindex = CCACHE_INVALID_HASHINDEX;
        cache->stat.erase_stat.success_num++;
    }
    else
    {
        cache->stat.erase_stat.fail_num++;
    }

    if (pthread_rwlock_unlock(&(cache->lock)) < 0)
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
ccache_set(ccache_t *cache, ccache_data_t *data,
                ccache_erase_t erase, void* arg, ccache_update_t update)
{
    int hashindex = ccache_hash(cache, data->key, data->keysize);
    ccache_node_t *node;

    if (pthread_rwlock_wrlock(&(cache->lock)) < 0)
    {
        CCACHE_SET_ERROR_NUM(CCACHE_LOCK_ERROR);
        return -1;
    }

    cache->stat.set_stat.total_num++;
    node = cache->functor.set(cache, hashindex, data, erase, arg, update);

    if (node)
    {
        cache->stat.set_stat.success_num++;
        ccache_lrulist_advance(cache, node);
    }
    else
    {
        cache->stat.set_stat.fail_num++;
    }

    if (pthread_rwlock_unlock(&(cache->lock)) < 0)
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
ccache_visit(ccache_t *cache, ccache_visit_t visit, void* arg)
{
    int hashindex;

    if (pthread_rwlock_rdlock(&(cache->lock)) < 0)
    {
        CCACHE_SET_ERROR_NUM(CCACHE_LOCK_ERROR);
        return -1;
    }

    for (hashindex = 0; hashindex < cache->hashitemnum; ++hashindex)
    {
        cache->functor.visit(cache, hashindex, visit, arg);
    }

    if (pthread_rwlock_unlock(&(cache->lock)) < 0)
    {
        CCACHE_SET_ERROR_NUM(CCACHE_LOCK_ERROR);
        return -1;
    }

    return 0;
}

static int 
ccache_count_cache_size(int datasize, int hashitemnum)
{
    return (sizeof(struct ccache_t)
            + datasize
            + hashitemnum * sizeof(struct ccache_hash_t));
}

static int 
ccache_compare_key(const void* data1, const void* data2, int len)
{
    return memcmp(data1, data2, sizeof(char) * len);
}

