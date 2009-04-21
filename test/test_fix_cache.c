/********************************************************************
	created:	2008/01/25
	filename: 	test_fix_cache.c
	author:		Lichuang
                
	purpose:    
*********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <signal.h>
#include <arpa/inet.h>

#include "ccache.h"

ccache_t* cache;

int createchild();  
void mainloop();
void createrandstring(char* string, int len);
void print_cache_stat_info(ccache_t* cache);

int isparent = 0;

int main()
{
    cache = ccache_create(100000, 1, "./testmap", 30, 30, 1);
    if (NULL == cache)
    {
        printf("create_cache error!\n");
        return -1;
    }

#if 1    
    createchild(10);
#else    
    isparent = 1;
    mainloop();
#endif

    if (isparent)
    {
        print_cache_stat_info(cache);
    }

    return 0;
}

void print_stat_info(ccache_stat_count_t* stat)
{
    printf("total_num = %d, success_num = %d, fail_num = %d\n"
            , stat->total_num
            , stat->success_num
            , stat->fail_num
            );
}

void print_cache_stat_info(ccache_t* cache)
{
    printf("find: "); print_stat_info(&(cache->stat.find_stat));
    printf("insert: "); print_stat_info(&(cache->stat.insert_stat));
    printf("update: "); print_stat_info(&(cache->stat.update_stat));
    printf("replace: "); print_stat_info(&(cache->stat.replace_stat));
    printf("erase: "); print_stat_info(&(cache->stat.erase_stat));
}

int createchild(int childnum) 
{
    int fd, ischild;
    
    if (0 < childnum)
    {
        ischild = 0;

        while (!ischild) 
        {
            if (0 < childnum)
            {
                switch((fd = fork()))
                {
                    case -1:    // fork error
                    {   
                        return -1;
                    }
                    break;
                    case 0:     // child
                    {
                        printf("[%s] [%s] [%d]: create child %d success!\n"
                                , __FILE__
                                , __FUNCTION__
                                , __LINE__
                                , getpid());
                        ischild = !0;

                        mainloop();
                    }
                    break;
                    default:    // parent
                    {
                        childnum--;
                    }
                    break;
                }
            }
            else
            {
                int status;

                if (-1 != (fd = wait(&status)))
                {
                    //++childnum;
                }
                else
                {
                    isparent = 1;
                    break;
                }
            }
        }

        if (ischild)
            return 0;
    }
    else
    {
        return -1;
    }

    return 0;
}

int cmp_fun(const void* data1, const void* data2, int len)
{
    return memcmp(data1, data2, sizeof(char) * len);
}

void del_fun(void *arg, const ccache_node_t* node)
{
    int *num = (int*)CCACHE_NODE_DATA(node);
    char k[101] = {'\0'};
    void *key = CCACHE_NODE_KEY(node);
    strncpy(k, key, node->keysize);

    printf("delnode <%s, %d>\n", k, *num);
    (void)arg;
}

void update_fun(const ccache_node_t* node, ccache_data_t* data)
{
    int *num = (int*)data->data;
    *num = *num + *((int*)CCACHE_NODE_DATA(node));
}

void visit_fun(void* arg, ccache_node_t* node)
{
    int *num = (int*)CCACHE_NODE_DATA(node);
    char k[101] = {'\0'};
    void *key = CCACHE_NODE_KEY(node);
    strncpy(k, key, node->keysize);
    (void)arg;

    printf("visit node <%s, %d>\n", k, *num);
}

void mainloop()
{
    char string[21];
    int num, i, ret;
    ccache_data_t data;

    srand((unsigned)time(NULL) + getpid());

    for (i = 1; i < 90000; ++i)
    {
        createrandstring(string, 20);
        num = rand();

        memset(&data, 0, sizeof(ccache_data_t));
        data.datasize = sizeof(int);
        data.keysize = sizeof(char) * 20;
        data.data = (void*)&num;
        data.key  = (void*)&string;

        printf("i = %d\n", i);
        if (0 > (ret = ccache_insert(&data, cache, cmp_fun, del_fun, NULL)))
        {
            printf("[pid = %d] insert node error, errormsg = %s!\n"
                    , getpid()
                    , ccache_error_msg[ccache_errno]);
            //break;
        }
        else
        {
            printf("[pid = %d] key = %s, insert node success!\n", getpid(), data.key);

            if (0 > ccache_find(&data, cache, cmp_fun))
            {
                printf("[pid = %d] find node error!\n", getpid());
                continue;
            }
            else 
            {

            }

            num = 1;
            if (0 == ccache_replace(&data, cache, cmp_fun, del_fun, NULL, update_fun))
            {
                //printf("[pid = %d] replace <%s, %d> success!\n", getpid(), string, num);
            }

			//* 把以下代码注释掉是为了测试ccache_del_t, 因为在节点不够用时需要进行节点的淘汰, 否则可以测试update, del操作            
            num = rand() + 123;
            if (0 > ccache_update(&data, cache, cmp_fun))
            {
                printf("[pid = %d] update <%s, %d> error!\n", getpid(), string, num);
            }
            else
            {
                //printf("[pid = %d] update <%s, %d> success!\n", getpid(), string, num);
            }

            if (!(i%10))
            {
                if (0 > ccache_erase(&data, cache, cmp_fun))
                {
                    printf("[%s, %d, pid = %d] delete <%s, %d> error, errormsg = %s!\n"
                            , ccache_error_file, ccache_error_line
                            , getpid(), string, num
                            , ccache_error_msg[ccache_errno]);
                }
                else
                {
                    //printf("[pid = %d] delete <%s, %d> success!\n", getpid(), string, num);
                }
            }
			//*/            
        }
    }

    printf("begin visit cache\n");
    ccache_visit(cache, visit_fun, NULL);
    printf("end visit cache\n");

    printf("pid = %d, test done\n", getpid());
}

const char CCH[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

void createrandstring(char* string, int len)
{
    int x, i;
    for (i = 0; i < len; ++i)
    {
        x = rand() % (sizeof(CCH) - 1);  
        
        string[i] = CCH[x];
    }

    string[i] = '\0';
}

