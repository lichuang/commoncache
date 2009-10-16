/********************************************************************
	created:	2008/01/23
	filename: 	node.h
	author:		Lichuang
                
	purpose:    
*********************************************************************/

#ifndef __CCACHE_NODE_H__
#define __CCACHE_NODE_H__

#ifdef CCACHE_USE_RBTREE
typedef enum ccache_color_t
{
    CCACHE_COLOR_RED     = 0,
    CCACHE_COLOR_BLACK   = 1
}ccache_color_t;
#endif

typedef struct ccache_node_t
{
    int datasize;
    int keysize;
    int hashindex;
    int freeareaid;
    struct ccache_node_t *lrunext, *lruprev;

#ifdef CCACHE_USE_LIST
    struct ccache_node_t *next, *prev;
#elif defined CCACHE_USE_RBTREE    
    ccache_color_t color;
    struct ccache_node_t *parent, *left, *right;
#endif

    char key[0];
}ccache_node_t;

struct ccache_data_t;
int ccache_count_nodesize(const struct ccache_data_t* data);

#define CCACHE_NODE_KEY(node)      ((char*)&((node)->key[0]))
#define CCACHE_NODE_DATA(node)     ((char*)&((node)->key[0]) + (node)->keysize)

#endif /* __CCACHE_NODE_H__ */

