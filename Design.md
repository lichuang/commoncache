# The design and implementation of CommonCache Library(base on version 0.6) #

## Introduction ##

### the functions of a cache ###
cache the most frequently visited data in the memory, so:
  * if the the data get losed is not a big matter,it means that you should not store data in cache only, offen, cache is between the DB server and application,hence reduce visiting DB times and speed up application.
  * since the memory is not quite big, so the eliminating algorithm such LRU must to be used.
  * must support key-value-like API such as find/erase/set and so forth.

### ccache feature (version 0.6) ###
  * using mmap to cache data in file, so it can be used in multi-thead and multi-process application.
  * support unfix size key node
  * support hash-rbtree and hash-list structure
  * use LRU algorithm to eliminate nodes when it is running out of its memory size
  * fast, 100W/read operation on fix-size key node in no more than 0.5 second

### what is the difference between ccache and memcached? ###
ccache is a static library, memcached is a cache sever, so users who use ccache must
design the protocols between clients and server, and write the server application themself.

> ## implementation ##
### the cache file structure ###
```
                                +----------------+
                                |                |
                                | cache header   |
                                |                |
                                |                |
            cache->hashitem[0]--.----------------+
                                |                |
                                |                |
                                | hashitem array |
                                |                |
            cache->freearea ----+----------------+
                                |                |
                                |                |
                                | freearea array |
                                |                |
                                +----------------+
                                |                |
                                | prealloc nodes |
                                |                |
            cache->start_free---+----------------+
                                |                |
                                |                |
                                |                |
                                | data  zone     |
                                |                |
                                |                |
                                +----------------+

                        (Figure 1, the cache file strture)
```

## the hashitem array detail ##
the hashitem array is used to find the node base on the node key.Every nodes in a same hashitem has a same hash num.All the nodes in the same hashitem are strutured in list or rbtree:
```
      +---------------------+
      |                     |
      |                     |
      |                     | first           next
      |    hashitem[n]      ___________node1________\node2
      |                     |                             ''''''''
      |                     |               \--------
      |                     |                 prev
      |                     |
      +---------------------+

      (Figure 2, the hash-list structrue)

                                    parent
                                  +-----------+
                                  |           |
     +---------------------+      |        \  |
     |                     |      |  ------/node2
     |                     |      |  | right
     |                     |root \|/ |
     |   hashitem[n]       |------node1
     |                     |     /|\ |
     |                     |      |  | left \
     |                     |      |  ------'/node3
     +---------------------+      |           |
                                  |           |
                                  +___________+
                                     parent
    (Figure 3, the hash-rbtree structrue)
```
when compiling the ccache, define the macro CCACHE\_USE\_RBTREE to specily using rbtree,
else macro CCACHE\_USE\_LIST using list. The structrue ccache\_node\_t has following fields
about this:

```
typedef struct ccache_node_t
{
        /* 
          ......
         */
#ifdef CCACHE_USE_LIST
    struct ccache_node_t *next, *prev;
#elif defined CCACHE_USE_RBTREE    
    ccache_color_t color;
    struct ccache_node_t *parent, *left, *right;
#endif

        /* 
          ......
         */
}ccache_node_t;
```
the hashitem array size is defined in the configure file when open the cache.

## the freearea array detail ##
ccache use slab-like algorithm to allocate node memory.In the configure file, there is an
item called "alignsize", this value specify the align size between different freearea,
assume the alignsize is 8 bytes:
```
  +-------------------+
  |                   |
  |freearea[0](size=8)|
  |                   |
  +-------------------+ lrufirst       lrunext
  |                   |_________node1__________node2       nodelast
  |freearea[1](size=16)              ..........     ```````   |
  |                   |                lruprev                |
  |                   | ______________________________________|
  +-------------------+                lrulast
  |                   |
  |freearea[2](size=24)
  |                   |
  |                   |
  +-------------------+
  |                   |
  |   ..........      |
  |                   |
  |                   |
  +-------------------+
(Figure 4, freearea array strture)
```
all the nodes in the same freearea has the same size, when allocating a node(ccache\_memory.c/ccache\_allocate):
  1. allign the node size with the alignsize value, then find the fit
freearea(ccache\_memory.c/ccache\_get\_freeareaid) and return the freeareaid
  1. if the data zone has enough memory for the node, change the cache->start\_free and cache->freesize and return the node pointer
  1. else, erase the  cache->freearea[freeareaid](freeareaid.md)->lrulast node, use this node memory for this allocation.
  1. every time a node allocated, place it in the freearea's tail(freearea->lrulast = node), when the node has been visited, it move on one step in the freearea list.So, the more a node has been
visited, the closer it moves to the freearea head(freearea->lrufirst).Hence, the freearea lrulast
node is the less frequently visited node in the freearea(although not so accurate:).

the ccache\_node\_t structure has following fields about LRU:
```
typedef struct ccache_node_t
{
    /*
       ....
    */       
    struct ccache_node_t *lrunext, *lruprev;

    /*
       ....
    */       
}ccache_node_t;
```

## configure file items ##
  1. mapfile
the cache file path, default is ./ccache\_mapfile

  1. min\_size
min size of a node, default is 16 bytes

  1. max\_size
max size of a node, default is 32 bytes

if min\_size == max\_size, means that the cache is fix-key-size cache

  1. hashitem
elements number of the the hashitem array, default is 1000

  1. datasize
the data zone size,default is 1000000 bytes

  1. prealloc\_num
the prealloc number of nodes in a freearea when initializing the cache, default is 10

  1. alignsize
the align size, default is 8 bytes

# init
memset the cache or not?