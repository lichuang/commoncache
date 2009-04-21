/********************************************************************
	created:	2008/01/23
	filename: 	ccache.h
	author:		Lichuang
                
	purpose:    
*********************************************************************/

#ifndef __CCACHE_CCACHE_H__
#define __CCACHE_CCACHE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "ccache_hash.h"
#include "ccache_node.h"
#include "ccache_error.h"
#include <pthread.h>

typedef struct ccache_data_t
{
    int     datasize;               /* the size of the data */
    int     keysize;                /* the size of the key */
    char*   data;                   /* the pointer of the data */
    char*   key;                    /* the pointer of the key */
}ccache_data_t;

/**
 * @brief   function pointer type used to compare data
 * @return  > 0 data1 > data2, = 0 data1 = data2, < 0 data1 < data2
 */
typedef     int (*ccache_compare_t)(const void* data1, const void* data2, int len);

/**
 * @brief   function pointer type used when deleting data
 * @return 
 */
typedef     void (*ccache_erase_t)(void* arg, const ccache_node_t* node);

/**
 * @brief   function pointer type used to update a node when node exist, the result will be saved in data
 * @param   node: original data
 * @param   data: update data and saved the result 
 * @return 
 */
typedef     void (*ccache_update_t)(const ccache_node_t* node, ccache_data_t* data);

/**
 * @brief   function pointer type used to visit all the nodes in the cache
 * @param   
 * @return 
 */
typedef     void (*ccache_visit_t)(void* arg, ccache_node_t* node);

struct ccache_t;

/* function pointers operating data */
typedef struct ccache_functor_t
{
    /*
     * find a data in the hashindex hashtable  
     */
    ccache_node_t* (*find)(int hashindex, const ccache_data_t* data, 
                            struct ccache_t* cache, ccache_compare_t cmp);

    /*
     * insert a data in the hashindex hashtable, if the key exist,
     * return the node and set exist 
     */
    ccache_node_t* (*insert)(int hashindex, const ccache_data_t* data,
                            struct ccache_t* cache, ccache_compare_t cmp,
                            ccache_erase_t erase, void* arg, int* exist);

    /*
     * replace a data in the hashindex hashtable,
     * if the key exist, insert the data 
     */
    ccache_node_t* (*replace)(int hashindex, ccache_data_t* data,
                            struct ccache_t* cache, ccache_compare_t cmp, 
                            ccache_erase_t erase, void* arg, ccache_update_t update);

    /*
     * update a data in the hashindex hashtable, 
     * if the key is not exist, return NULL
     */
    ccache_node_t* (*update)(int hashindex, const ccache_data_t* data, 
                            struct ccache_t* cache, ccache_compare_t cmp);

    /*
     * erase a data in the hashindex hashtable and return the erased node, 
     * if the key is not exist, return NULL
     */
    ccache_node_t* (*erase)(int hashindex, ccache_node_t* node, struct ccache_t* cache);

    /*
     * visit the nodes in the hashindex hash-table
     */
    void           (*visit)(struct ccache_t* cache, int hashindex, ccache_visit_t visit, void* arg);
}ccache_functor_t;

typedef struct ccache_stat_count_t
{
    int total_num;
    int success_num;
    int fail_num;
}ccache_stat_count_t;

typedef struct ccache_stat_t
{
    ccache_stat_count_t find_stat;
    ccache_stat_count_t update_stat;
    ccache_stat_count_t replace_stat;
    ccache_stat_count_t insert_stat;
    ccache_stat_count_t erase_stat;
}ccache_stat_t;

struct ccache_freearea_t;

/* data struct that operating cache */
typedef struct ccache_t
{
    int hashitemnum;                /* number of hashitem */
    int freeareanum;                /* number of freearea */
    int datasize;                   /* size of data */
    int filesize;                   /* size of share memory file */
    int freesize;                   /* the size of free data */
    char *start_free;               /* the pointer to the free data, if there is not free data, equals to NULL */

    ccache_functor_t functor;
    ccache_stat_t stat;
    pthread_rwlock_t lock;          /* thread reader-writer lock */
    struct ccache_freearea_t* freearea;    /* pointer to the freearea array */

    ccache_hash_t hashitem[0];      /* pointer to the hashitem array */
}ccache_t;

/**
 * @brief   create a ccache_t pointer 
 * @param   datasize: the data size contained in cache
 * @param   hashitemnum: the hash item number in the cache
 * @param   mapfilename: the share memory file name
 * @param   min_size: the min size of the node data zone, if init is not set, ignore this param
 * @param   max_size: the max size of the node data zone, if init is not set, ignore this param
 * @param   init: flag if or not initialise the cache, if the share memory file not exist or the 
 *          size of the file is not the same as required, the function will ignore the init 
 *          agrument and the cache will be initialised 
 * @return  NULL if failed
 */
ccache_t*    ccache_create(int datasize, int hashitemnum, const char* mapfilename, int min_size, int max_size, int init);

/**
 * @brief   destroy a ccache pointer
 * @param   
 * @return  
 */
void         ccache_destroy(ccache_t* cache);

/**
 * @brief   insert a data into the cache
 * @param   data: the data will be inserted
 * @param   cache: the cache pointer
 * @param   compare: the function used to compare key
 * @param   erase: when there is no more space to insert data, use LRU algorithm to allocate a new node, 
 *               this function used to manage the deleted node, if NULL delete the node directly
 * @param   arg: the argument passed to the erase function
 * @return  0 if success, -1 if failed
 */
int         ccache_insert(const ccache_data_t* data, ccache_t* cache, 
                        ccache_compare_t compare, ccache_erase_t erase, void* arg);

/**
 * @brief   find a node in the cache
 * @param   data  if success, the value of the node contained in this data, so it must not be NULL
 * @param   cache: the cache pointer
 * @param   compare: the function used to compare key
 * @return  0 if success, -1 if failed
 */
int         ccache_find(ccache_data_t* data, ccache_t* cache, ccache_compare_t compare);

/**
 * @brief   update a node in the cache
 * @param   data: the data will be update to the node,if success, the value of the node contained in this data,
 *          so it must not be NULL
 * @param   cache: the cache pointer
 * @param   compare: the function used to compare key
 * @return  0 if success, -1 if failed
 * @NOTE    the data size and key size MUST equal to the previous
 */
int         ccache_update(const ccache_data_t* data, ccache_t* cache, ccache_compare_t compare);

/**
 * @brief   erase a node in the cache
 * @param   data:  if success, the value of the deleted node contained in this data,so it must not be NULL
 * @param   cache: the cache pointer
 * @param   compare: the function used to compare key
 * @return  0 if success, -1 if failed
 */
int         ccache_erase(ccache_data_t* data, ccache_t* cache, ccache_compare_t compare);

/**
 * @brief   if the key does not in the cache, insert the data in the cache, 
 *          otherwise use the update function to update the node in the cache
 * @param   data:   the data updated to the node, if success, the new value of node contained in this data,so it must not be NULL
 * @param   cache: the cache pointer
 * @param   compare: the function used to compare key
 * @param   arg: the argument passed to the erase function
 * @param   update: the function used to update node
 * @return  0 if success, -1 if failed
 */
int         ccache_replace(ccache_data_t* data, ccache_t* cache, ccache_compare_t compare,
                        ccache_erase_t erase, void* arg, ccache_update_t update);

/**
 * @brief   use the visit function to visit all the node in the cache
 * @param   cache: the cache pointer
 * @param   visit: the function used to visit node
 * @param   arg: the argument passed to the visit function
 * @return  0 if success, -1 if failed
 */
int        ccache_visit(ccache_t* cache, ccache_visit_t visit, void* arg);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CCACHE_CCACHE_H__ */

