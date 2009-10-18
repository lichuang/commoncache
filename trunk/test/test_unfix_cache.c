/********************************************************************
	created:	2008/05/30
	filename: 	test_unfix_cache.c
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

#include <ccache/ccache.h>

ccache_t* cache;

int createchild();  
void mainloop();
void createrandstring(char* string, int len);
void print_cache_stat_info(ccache_t* cache);

int isparent = 0;

int main()
{
    cache = ccache_open("../conf/unfix_cache.conf", NULL);
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

    ccache_close(cache);

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
    printf("set: "); print_stat_info(&(cache->stat.set_stat));
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

#define STRING_LEN 800
char k[STRING_LEN] = {'\0'};

int cmp_fun(const void* data1, const void* data2, int len)
{
    return memcmp(data1, data2, sizeof(char) * len);
}

void del_fun(void* arg, const ccache_node_t* node)
{
    void *key = CCACHE_NODE_KEY(node);
    strncpy(k, key, node->keysize);

    (void)arg;
    //printf("delnode <%s, %d>\n", k, *num);
}

void update_fun(const ccache_node_t* node, ccache_data_t* data)
{
    int *num = (int*)data->data;
    *num = *num + *((int*)CCACHE_NODE_DATA(node));
}

void visit_fun(void* arg, ccache_node_t* node)
{
    int *num = (int*)CCACHE_NODE_DATA(node);
    void *key = CCACHE_NODE_KEY(node);
    strncpy(k, key, node->keysize);

    (void)arg;
    printf("visit node <%s, %d>\n", k, *num);
}

void mainloop()
{
    char string[STRING_LEN];
    int num, i, len;
    ccache_data_t data;

    srand((unsigned)time(NULL) + getpid());

    for (i = 1; i < 90000; ++i)
    {
        len = (i + 10) % (STRING_LEN - 4) + 2;
		memset(string, 0, STRING_LEN);
        createrandstring(string, len);
        num = rand();

        memset(&data, 0, sizeof(ccache_data_t));
        data.datasize = sizeof(int);
        data.keysize = sizeof(char) * len;
        data.data = (void*)&num;
        data.key  = (void*)&string;

        printf("i = %d\n", i);

        if (0 > ccache_insert(cache, &data, del_fun, NULL))
        {
            //if (ccache_errno == CCACHE_ALLOC_NODE_ERROR)
            printf("[pid = %d] insert node error!\n"
                    , getpid());
            //break;
        }
        else
        {
            printf("[pid = %d] keysize = %d, insert node success!\n", getpid(), data.keysize);

            if (0 > ccache_find(cache, &data))
            {
                printf("[pid = %d] find node error!\n", getpid());
                continue;
            }
            else 
            {

            }

#if 1
            num = 1;
            if (0 == ccache_set(cache, &data, del_fun, NULL, update_fun))
            {
                //printf("[pid = %d] set data <%s, %d> success!\n", getpid(), string, num);
            }

            num = rand() + 123;
            if (0 > ccache_update(cache, &data))
            {
                //printf("[pid = %d] update <%s, %d> error!\n", getpid(), string, num);
            }
            else
            {
                //printf("[pid = %d] update <%s, %d> success!\n", getpid(), string, num);
            }
#endif

            if (!(i % 10))
            {
                if (0 > ccache_erase(cache, &data))
                {
                    printf("[pid = %d] erase node error, errormsg = %s!\n"
                            , getpid()
                            , ccache_error_msg[ccache_errno]);
                }
                else
                {
                    printf("[pid = %d] erase node success\n", getpid());
                }
            }
        }
    }

    printf("begin visit cache\n");
    ccache_visit(cache, visit_fun, NULL);
    printf("end visit cache\n");

    printf("pid = %d, test done\n", getpid());
}

const char str[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

void createrandstring(char* string, int len)
{
    int x, i;
    for (i = 0; i < len - 1; ++i)
    {
        x = rand() % (sizeof(str) - 1);  
        //x = (i + len) % (sizeof(str) - 1);  
        
        string[i] = str[x];
    }

    string[++i] = str[len % sizeof(str) + 1];
    string[i] = '\0';
}

