/********************************************************************
	created:	2008/01/24
	filename: 	memory.c
	author:		Lichuang
                
	purpose:    
*********************************************************************/

#include "ccache.h"
#include "ccache_memory.h"
#include "ccache_node.h"
#include "ccache_hash.h"
#include "ccache_lock.h"
#include "ccache_lrulist.h"
#include "ccache_config.h"    

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

static int ccache_get_freeareaid(ccache_t *cache, int *size);
static int ccache_round_up(int size);
static int ccache_prealloc_freearea(ccache_t *cache);

ccache_t* 
ccache_create_mmap(int filesize, const char *mapfilename, int *init)
{
    int fd;
    struct stat st;
    ccache_t *cache;

    fd = open(mapfilename, O_RDWR);
    if (0 <= fd && 0 != fstat(fd, &st))
    {
        return NULL;
    }

    if (st.st_size != filesize || 0 > fd)
    {
        if (0 <= fd)
        {
            if (0 != unlink(mapfilename))
            {
                return NULL;
            }
            close(fd);
        }

        fd = open(mapfilename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        if (0 > fd)
        {
            return NULL;
        }

        *init = 1;
    }        

    if (*init)
    {
        if (0 > lseek(fd, filesize - 1, SEEK_SET))
        {
            return NULL;
        }

        if (0 > write(fd, " ", 1))
        {
            return NULL;
        }
    }

    cache = mmap(0, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (MAP_FAILED == cache)
    {
        close(fd);
        return NULL;
    }

    if (*init)
    {
        memset(cache, 0, sizeof(char) * filesize);
    }

    close(fd);

    return cache;
}

int 
ccache_destroy_mmap(ccache_t *cache)
{
    return munmap((void*)cache, cache->filesize);
}

ccache_node_t* 
ccache_allocate(ccache_t *cache, int size, ccache_erase_t erase, void* arg)
{
    int freeareaid;
    ccache_node_t *node;

    freeareaid = ccache_get_freeareaid(cache, &size);
    if (0 > freeareaid)
    {
        return NULL;
    }

    if (cache->freesize >= size)
    {
        node = (ccache_node_t*)cache->start_free;
        node->hashindex = CCACHE_INVALID_HASHINDEX;
        cache->start_free = cache->start_free + size;
        cache->freesize -= size;
    }
    else
    {
        if (!(node = cache->freearea[freeareaid].lrulast))
        {
            return NULL;
        }

        ccache_lrulist_free(node, cache);
        if (CCACHE_INVALID_HASHINDEX != node->hashindex)
        {
            cache->functor.erase(node->hashindex, node, cache); 
            if (erase)
            {
                erase(arg, node);
            }
        }

    }

    node->freeareaid = freeareaid;

    return node;
}

int 
ccache_init_freearea(ccache_t *cache, int datasize, int min_size, int max_size)
{
    int i;
    int size = min_size + sizeof(ccache_node_t);
    int nodesize = datasize;

    max_size = ccache_round_up(sizeof(ccache_node_t) + max_size);
    cache->freearea = (ccache_freearea_t*)(cache->hashitem + cache->hashitemnum);
    for (i = 0; size <= max_size; ++i)
    {
        size = ccache_round_up(size);
        nodesize -= sizeof(struct ccache_freearea_t) + size;
        if (0 > nodesize)
        {
            return -1;
        }

        cache->freearea[i].size = size;
        size += CCACHE_ALIGNSIZE;
    }

    cache->freeareanum = i;
    cache->freesize = nodesize;
    cache->start_free = (char*)(cache->freearea + cache->freeareanum);

    if (cache->freesize < 0)
    {
        return -1;
    }

    return  ccache_prealloc_freearea(cache);
}

int 
ccache_prealloc_freearea(ccache_t *cache)
{
    int i;
    char *start = cache->start_free;
    ccache_node_t *node;

    for (i = 0; i < cache->freeareanum; ++i)
    {
        node = (ccache_node_t*)start; 
        cache->freearea[i].lrufirst = cache->freearea[i].lrulast = node; 
        
        node->hashindex = CCACHE_INVALID_HASHINDEX;
        node->freeareaid = i;
        node->datasize = node->keysize = 0;
        node->lrunext = node->lruprev = NULL;
#ifdef CCACHE_USE_LIST
        node->next = node->prev = NULL;
#elif defined CCACHE_USE_RBTREE
        node->parent = node->left = node->right = NULL;
#endif

        start += cache->freearea[i].size;
    }
    cache->start_free = start;

    return 0;
}

/*
 * align the size according to the CCACHE_ALIGNSIZE
 */
int 
ccache_round_up(int size)
{
    return (size + (CCACHE_ALIGNSIZE) - 1) & ~((CCACHE_ALIGNSIZE) - 1);
}

/*
 * in cache freearea array find a element that its 
 * size is bigger than or equal to the *size
 */
int 
ccache_get_freeareaid(ccache_t *cache, int *size)
{
    int offset, i;

    offset = *size - cache->freearea[0].size;
    if (offset > 0)
    {
        i = (offset / CCACHE_ALIGNSIZE) + 1;
        if (i >= cache->freeareanum)
        {
            return -1;
        }

        *size = cache->freearea[i].size;
        return i;
    }
    else
    {
        *size = cache->freearea[0].size;
        return 0;
    }
}

