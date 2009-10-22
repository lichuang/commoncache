/********************************************************************
	created:	2009/10/23
	filename: 	demo.c
	author:		Lichuang
                
	purpose:    a demo show how to use ccache APIS
*********************************************************************/

#include <stdio.h>
#include <ccache/ccache.h>

static void visit_fun(void* arg, ccache_node_t* node)
{
    /* shut up gcc warning */
    arg = arg;
    int *key = (int*)CCACHE_NODE_KEY(node);
    int *num = (int*)CCACHE_NODE_DATA(node);

    printf("node is <%d, %d>\n", *key, *num);
}

int main()
{
    ccache_data_t data;
    int key, value;
    ccache_t *cache = ccache_open("../conf/fix_cache.conf", NULL);

    key = 1;
    value = 2;
    data.datasize = sizeof(int);
    data.keysize = sizeof(int);
    data.key = (void*)&key;
    data.data  = (void*)&value;

    if (ccache_insert(cache, &data, NULL, NULL) < 0)
    {
        printf("ccache_insert error!\n");
    }
    else
    {
        printf("ccache_insert <%d, %d> success!\n", key, value);
    }

    if (ccache_find(cache, &data) < 0)
    {
        printf("ccache_find error!\n");
    }
    else
    {
        printf("ccache_find <%d, %d>\n", *((int*)(data.key)), *((int*)(data.data)));
    }

    value = 100;
    if (ccache_set(cache, &data, NULL, NULL, NULL) < 0)
    {
        printf("ccache_set error!\n");
    }
    else
    {
        ccache_find(cache, &data);
        printf("ccache_set now set the key %d with value %d\n", *((int*)data.key), *((int*)data.data));
    }

    ccache_visit(cache, visit_fun, NULL);

    ccache_erase(cache, &data);

    ccache_close(cache);

    return 0;
}

