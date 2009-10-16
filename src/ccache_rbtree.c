/********************************************************************
	created:	2008/10/06
	filename: 	rbtree.c
	author:		Lichuang
                
	purpose:    
*********************************************************************/

#include "ccache.h"
#include "ccache_rbtree.h"
#include "ccache_node.h"
#include "ccache_hash.h"
#include "ccache_memory.h"
#include <string.h>

#ifdef CCACHE_USE_RBTREE

static ccache_node_t* ccache_rbtree_find(ccache_t *cache, int hashindex, const ccache_data_t* data);

static ccache_node_t* ccache_rbtree_insert(ccache_t *cache, int hashindex, 
										const ccache_data_t* data, ccache_erase_t erase, 
                                        void* arg, int *exist);

static ccache_node_t* ccache_rbtree_update(int hashindex, const ccache_data_t* data,
                                        ccache_t* cache);

static ccache_node_t* ccache_rbtree_set(ccache_t *cache, int hashindex, 
										ccache_data_t* data, ccache_erase_t erase, 
                                        void* arg, ccache_update_t update);

static ccache_node_t* ccache_rbtree_erase(int hashindex, ccache_node_t* node, 
                                        ccache_t* cache);

static void           ccache_rbtree_visit(ccache_t* cache, int hashindex, 
                                        ccache_visit_t visit, void* arg);


#endif    

extern ccache_compare_t cache_compare;

int 
ccache_init_rbtree_functor(ccache_functor_t *functor)
{
#ifdef CCACHE_USE_RBTREE
    functor->find   = ccache_rbtree_find;
    functor->insert = ccache_rbtree_insert;
    functor->update = ccache_rbtree_update;
    functor->erase  = ccache_rbtree_erase;
    functor->visit  = ccache_rbtree_visit;
    functor->set 	= ccache_rbtree_set;

    return 0;
#else
    (void)functor;
    return -1;
#endif    
}

#ifdef CCACHE_USE_RBTREE

static void 
ccache_rbtree_init(ccache_node_t* node, int hashindex, const ccache_data_t* data)
{
    node->keysize = data->keysize;
    node->datasize = data->datasize;
    node->hashindex = hashindex;
    node->lrunext = node->lruprev = NULL;

    memcpy(CCACHE_NODE_KEY(node), data->key, node->keysize);
    memcpy(CCACHE_NODE_DATA(node), data->data, node->datasize);
}

/*-----------------------------------------------------------
|       node           left
|       / \            / \
|    left  y   ==>    a   node
|   / \               / \
|  a   b             b   y
-----------------------------------------------------------*/
static ccache_node_t* 
ccache_rbtree_rotate_right(ccache_node_t* node, ccache_node_t* root)
{
    ccache_node_t* left = node->left;

    if ((node->left = left->right))
    {
        left->right->parent = node;
    }
    left->right = node;

    if ((left->parent = node->parent))
    {
        if (node == node->parent->right)
        {
            node->parent->right = left;
        }
        else
        {
            node->parent->left = left;
        }
    }
    else
    {
        root = left;
    }
    node->parent = left;

    return root;
}

/*-----------------------------------------------------------
|   node           right
|   / \    ==>     / \
|   a  right     node  y
|       / \           / \
|       b  y         a   b
 -----------------------------------------------------------*/
static ccache_node_t* 
ccache_rbtree_rotate_left(ccache_node_t* node, ccache_node_t* root)
{
    ccache_node_t* right = node->right;

    if ((node->right = right->left))
    {
        right->left->parent = node;
    }
    right->left = node;

    if ((right->parent = node->parent))
    {
        if (node == node->parent->right)
        {
            node->parent->right = right;
        }
        else
        {
            node->parent->left = right;
        }
    }
    else
    {
        root = right;
    }
    node->parent = right;

    return root;
}

static ccache_node_t* 
ccache_rbtree_insert_rebalance(ccache_node_t* root, ccache_node_t* node)
{
    ccache_node_t *parent, *gparent, *uncle, *tmp;

    while ((parent = node->parent) && CCACHE_COLOR_RED == parent->color)
    {
        gparent = parent->parent;

        if (parent == gparent->left)
        {
            uncle = gparent->right;
            if (uncle && CCACHE_COLOR_RED == uncle->color)
            {
                uncle->color = CCACHE_COLOR_BLACK;
                parent->color = CCACHE_COLOR_BLACK;
                gparent->color = CCACHE_COLOR_RED;
                node = gparent;
            }
            else
            {
                if (parent->right == node)
                {
                    root = ccache_rbtree_rotate_left(parent, root);
                    tmp = parent;
                    parent = node;
                    node = tmp;
                }

                parent->color = CCACHE_COLOR_BLACK;
                gparent->color = CCACHE_COLOR_RED;
                root = ccache_rbtree_rotate_right(gparent, root);
            }
        } 
        else 
        {
            uncle = gparent->left;
            if (uncle && uncle->color == CCACHE_COLOR_RED)
            {
                uncle->color = CCACHE_COLOR_BLACK;
                parent->color = CCACHE_COLOR_BLACK;
                gparent->color = CCACHE_COLOR_RED;
                node = gparent;
            }
            else
            {
                if (parent->left == node)
                {
                    root = ccache_rbtree_rotate_right(parent, root);
                    tmp = parent;
                    parent = node;
                    node = tmp;
                }

                parent->color = CCACHE_COLOR_BLACK;
                gparent->color = CCACHE_COLOR_RED;
                root = ccache_rbtree_rotate_left(gparent, root);
            }
        }
    }

    root->color = CCACHE_COLOR_BLACK;

    return root;
}

static ccache_node_t* 
ccache_rbtree_erase_rebalance(ccache_node_t* root, ccache_node_t* node, ccache_node_t* parent)
{
    ccache_node_t *other, *left, *right;

    while ((!node || CCACHE_COLOR_BLACK == node->color) && node != root)
    {
        if (parent->left == node)
        {
            other = parent->right;
            if (CCACHE_COLOR_RED == other->color)
            {
                other->color = CCACHE_COLOR_BLACK;
                parent->color = CCACHE_COLOR_RED;
                root = ccache_rbtree_rotate_left(parent, root);
                other = parent->right;
            }
            if ((!other->left  || CCACHE_COLOR_BLACK == other->left->color) &&
                (!other->right || CCACHE_COLOR_BLACK == other->right->color))
            {
                other->color = CCACHE_COLOR_RED;
                node = parent;
                parent = node->parent;
            }
            else
            {
                if (!other->right || CCACHE_COLOR_BLACK == other->right->color)
                {
                    if ((left = other->left))
                    {
                        left->color = CCACHE_COLOR_BLACK;
                    }
                    other->color = CCACHE_COLOR_RED;
                    root = ccache_rbtree_rotate_right(other, root);
                    other = parent->right;
                }
                other->color = parent->color;
                parent->color = CCACHE_COLOR_BLACK;
                if (other->right)
                {
                    other->right->color = CCACHE_COLOR_BLACK;
                }
                root = ccache_rbtree_rotate_left(parent, root);
                node = root;
                break;
            }
        }
        else
        {
            other = parent->left;
            if (CCACHE_COLOR_RED == other->color)
            {
                other->color = CCACHE_COLOR_BLACK;
                parent->color = CCACHE_COLOR_RED;
                root = ccache_rbtree_rotate_right(parent, root);
                other = parent->left;
            }
            if ((!other->left  || CCACHE_COLOR_BLACK == other->left->color) &&
                (!other->right || CCACHE_COLOR_BLACK == other->right->color))
            {
                other->color = CCACHE_COLOR_RED;
                node = parent;
                parent = node->parent;
            }
            else
            {
                if (!other->left || CCACHE_COLOR_BLACK == other->left->color)
                {
                    if ((right = other->right))
                    {
                        right->color = CCACHE_COLOR_BLACK;
                    }
                    other->color = CCACHE_COLOR_RED;
                    root = ccache_rbtree_rotate_left(other, root);
                    other = parent->left;
                }
                other->color = parent->color;
                parent->color = CCACHE_COLOR_BLACK;
                if (other->left)
                {
                    other->left->color = CCACHE_COLOR_BLACK;
                }
                root = ccache_rbtree_rotate_right(parent, root);
                node = root;
                break;
            }
        }
    }

    if (node)
    {
        node->color = CCACHE_COLOR_BLACK;
    } 

    return root;
}

static ccache_node_t* 
ccache_rbtree_insert_auxiliary(int hashindex, ccache_node_t* node, ccache_node_t* parent, 
                            ccache_t* cache) 
{
    ccache_node_t* root = cache->hashitem[hashindex].root;
    int ret;

    if (!parent)
    {
        root = node;
    }
    else
    {
        ret = parent->keysize - node->keysize;
        if (!ret)
        {
            ret = cache_compare(CCACHE_NODE_KEY(parent), CCACHE_NODE_KEY(node), node->keysize);
        }
        if (0 < ret)
        {
            parent->left = node;
        }
        else
        {
            parent->right = node;
        }
    }

    node->parent = parent;   
    node->left = node->right = NULL;
    node->color = CCACHE_COLOR_RED;

    cache->hashitem[hashindex].root = ccache_rbtree_insert_rebalance(root, node);
    cache->hashitem[hashindex].nodenum++;

    return node;
}

static ccache_node_t* 
ccache_rbtree_find_auxiliary(ccache_t *cache, int hashindex, const ccache_data_t* data,
                            ccache_node_t** save)
{
    ccache_node_t *node, *parent;
    int ret, keysize;
    char *key;

    node = cache->hashitem[hashindex].root, parent = NULL;
    keysize = data->keysize;
    key = data->key;
    while (node)
    {
        parent = node;
        ret = node->keysize - keysize;
        if (!ret)
        {
            ret = cache_compare(CCACHE_NODE_KEY(node), key, keysize);
        }
        if (0 < ret)
        {
            node = node->left;
        }
        else if (0 > ret)
        {
            node = node->right;
        }
        else
        {
            break;
        }
    }

    if (save)
    {
        *save = parent;
    }

    return node;
}

static void
ccache_rbtree_visit_auxiliary(ccache_node_t* node, ccache_visit_t visit, void* arg)
{
    if (node->left)
    {
        ccache_rbtree_visit_auxiliary(node->left, visit, arg);
    }
    visit(arg, node);
    if (node->right)
    {
        ccache_rbtree_visit_auxiliary(node->right, visit, arg);
    }
}

static ccache_node_t* 
ccache_rbtree_find(ccache_t *cache, int hashindex, const ccache_data_t* data)
{
    return ccache_rbtree_find_auxiliary(cache, hashindex, data, NULL);
}

static ccache_node_t* 
ccache_rbtree_insert(ccache_t *cache, int hashindex, 
					const ccache_data_t* data, ccache_erase_t erase, 
					void* arg, int *exist)
{
    ccache_node_t *node = NULL, *parent = NULL;

    node = ccache_rbtree_find_auxiliary(cache, hashindex, data, &parent);
    if (node)
    {
        CCACHE_SET_ERROR_NUM(CCACHE_KEY_EXIST);
        *exist = 1;
        return node;
    }

    node = ccache_allocate(cache, ccache_count_nodesize(data), erase, arg);
    if (!node)
    {
        CCACHE_SET_ERROR_NUM(CCACHE_ALLOC_NODE_ERROR);
        return NULL;
    }
    /*
       if the allocated node is in the same hash table, 
       then the rbtree may changed, we should find its parent again
    */
    if (node->hashindex == hashindex)
    {
        parent = NULL;
        ccache_rbtree_find_auxiliary(cache, hashindex, data, &parent);
    }

    ccache_rbtree_init(node, hashindex, data);
    return ccache_rbtree_insert_auxiliary(hashindex, node, parent, cache);
}

static ccache_node_t* 
ccache_rbtree_erase(int hashindex, ccache_node_t* node, ccache_t* cache)
{
    ccache_node_t *child, *parent, *old, *left, *root;
    ccache_color_t color;

    old = node;
    root = cache->hashitem[hashindex].root;

    if (node->left && node->right)
    {
        node = node->right;
        while ((left = node->left))
        {
            node = left;
        }
        child = node->right;
        parent = node->parent;
        color = node->color;

        if (child)
        {
            child->parent = parent;
        }
        if (parent)
        {
            if (parent->left == node)
            {
                parent->left = child;
            }
            else
            {
                parent->right = child;
            }
        }
        else
        {
            root = child;
        }

        if (node->parent == old)
        {
            parent = node;
        }

        node->parent = old->parent;
        node->color = old->color;
        node->right = old->right;
        node->left = old->left;

        if (old->parent)
        {
            if (old->parent->left == old)
            {
                old->parent->left = node;
            }
            else
            {
                old->parent->right = node;
            }
        } 
        else
        {
            root = node;
        }

        old->left->parent = node;
        if (old->right)
        {
            old->right->parent = node;
        }
    }
    else
    {
        if (!node->left)
        {
            child = node->right;
        }
        else if (!node->right)
        {
            child = node->left;
        }
        parent = node->parent;
        color = node->color;

        if (child)
        {
            child->parent = parent;
        }
        if (parent)
        {
            if (parent->left == node)
            {
                parent->left = child;
            }
            else
            {
                parent->right = child;
            }
        }
        else
        {
            root = child;
        }
    }

    if (CCACHE_COLOR_BLACK == color)
    {
        root = ccache_rbtree_erase_rebalance(root, child, parent);
    }

    cache->hashitem[hashindex].root = root;
    cache->hashitem[hashindex].nodenum--;

    old->left = old->right = old->parent = NULL;

    return old;
}

static ccache_node_t* 
ccache_rbtree_update(int hashindex, const ccache_data_t* data, ccache_t* cache)
{
    ccache_node_t* node, *parent;

    node = ccache_rbtree_find_auxiliary(cache, hashindex, data, &parent);
    if (!node)
    {
        CCACHE_SET_ERROR_NUM(CCACHE_KEY_NOT_EXIST);
        return NULL;
    }

    memcpy(CCACHE_NODE_DATA(node), data->data, node->datasize);

    return node;
}

static ccache_node_t* 
ccache_rbtree_set(ccache_t *cache, int hashindex, ccache_data_t* data,
                    ccache_erase_t erase, void* arg, ccache_update_t update)
{
    ccache_node_t *node = NULL, *parent = NULL;

    node = ccache_rbtree_find_auxiliary(cache, hashindex, data, &parent);

    if (node)
    {
        update(node, data);

        memcpy(CCACHE_NODE_DATA(node), data->data, node->datasize);
    }
    else
    {
        node = ccache_allocate(cache, ccache_count_nodesize(data), erase, arg);
        if (!node)
        {
            CCACHE_SET_ERROR_NUM(CCACHE_ALLOC_NODE_ERROR);
            return NULL;
        }

        /*
           if the allocated node is in the same hash table, 
           then the rbtree may changed, we should find its parent again
         */
        if (node->hashindex == hashindex)
        {
            parent = NULL;
            ccache_rbtree_find_auxiliary(cache, hashindex, data, &parent);
        }

        ccache_rbtree_init(node, hashindex, data);
        ccache_rbtree_insert_auxiliary(hashindex, node, parent, cache);
    }

    return node;
}

static void 
ccache_rbtree_visit(ccache_t* cache, int hashindex, ccache_visit_t visit, void* arg)
{
    ccache_hash_t* hashitem = &cache->hashitem[hashindex];

    if (hashitem->root)
    {
        ccache_rbtree_visit_auxiliary(hashitem->root, visit, arg);
    }
}

#endif

