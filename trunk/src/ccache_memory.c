/********************************************************************
	created:	2008/01/24
	filename: 	ccache_memory.c
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
#include <stdio.h>

static int      ccache_get_freeareaid(ccache_t *cache, int *size);
static int      ccache_round_up(int size);
static char*    ccache_prealloc_freearea(ccache_t *cache, int index, char *start);
static int      ccache_prealloc(ccache_t *cache);

ccache_t* 
ccache_create_mmap(int filesize, const char *mapfilename, char *init)
{
    int fd;
    struct stat st;
    ccache_t *cache;

    fd = open(mapfilename, O_RDWR | O_CREAT);
    if (fd < 0)
    {
        return NULL;
    }

    if (fstat(fd, &st) != 0)
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

    cache->fd = fd;

    return cache;
}

int 
ccache_destroy_mmap(ccache_t *cache)
{
    int fd = cache->fd;
    msync(cache, cache->filesize, MS_SYNC);
    munmap((void*)cache, cache->filesize);
    close(fd);
    return 0;
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

        ccache_lrulist_free(cache, node);
        if (node->hashindex != CCACHE_INVALID_HASHINDEX)
        {
            cache->functor.erase(cache, node->hashindex, node); 
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
        nodesize -= sizeof(struct ccache_freearea_t) + size * cache_config.prealloc_num;
        if (0 > nodesize)
        {
            fprintf(stderr, "not enough memory for prealloc!\n");
            return -1;
        }

        cache->freearea[i].size = size;
        size += cache_config.align_size;
    }

    cache->freeareanum = i;
    cache->freesize = nodesize;
    cache->start_free = (char*)(cache->freearea + cache->freeareanum);

    return  ccache_prealloc(cache);
}

char*
ccache_prealloc_freearea(ccache_t *cache, int index, char *start)
{
    int i;
    ccache_freearea_t *freearea = &(cache->freearea[index]);
    int size = freearea->size;
    ccache_node_t *node = NULL;
    ccache_node_t *head = NULL, *last = NULL;

    for (i = 0; i < cache_config.prealloc_num; ++i, start += size)
    {
        node = (ccache_node_t*)start;
        node->hashindex = CCACHE_INVALID_HASHINDEX;
        node->freeareaid = index;
        node->datasize = node->keysize = 0;

        if (!head)
        {
            head = node;
            node->lruprev = NULL;
        }
        else
        {
            last->lrunext = node;
            node->lruprev = last;
        }

        last = node;

#ifdef CCACHE_USE_LIST
        node->next = node->prev = NULL;
#elif defined CCACHE_USE_RBTREE
        node->parent = node->left = node->right = NULL;
#endif
    }

    freearea->lrufirst = head;
    freearea->lrulast = node;
    node->lrunext = NULL;

    return (start);
}

int 
ccache_prealloc(ccache_t *cache)
{
    int i;
    char *start = cache->start_free;

    if (cache_config.prealloc_num > 0)
    {
        for (i = 0; i < cache->freeareanum; ++i)
        {
            start = ccache_prealloc_freearea(cache, i, start);
        }
        cache->start_free = start;
    }

    return 0;
}

/*
 * align the size according to the CCACHE_ALIGNSIZE
 */
int 
ccache_round_up(int size)
{
    return (size + (cache_align_size) - 1) & ~((cache_align_size) - 1);
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
        i = (offset / cache_align_size) + 1;
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

