/********************************************************************
	created:	2008/01/23
	filename: 	lock.c
	author:		Lichuang
                
	purpose:    
*********************************************************************/

#include "ccache_lock.h"
#include <pthread.h>
#include <fcntl.h>

int 
ccache_init_thread_rwlock(pthread_rwlock_t *lock)
{
    pthread_rwlockattr_t attr;

    if (0 != pthread_rwlockattr_init(&attr))
    {
        return -1;
    }

    if (0 != pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED))
    {
        return -1;
    }

    if(0 != pthread_rwlock_init(lock, &attr))
    {
        return -1;
    }

    return 0;
}

