/********************************************************************
	created:	2008/01/23
	filename: 	lock.h
	author:		Lichuang
                
	purpose:    
*********************************************************************/

#ifndef __CCACHE_LOCK_H__
#define __CCACHE_LOCK_H__

#include <pthread.h>

int ccache_init_thread_rwlock(pthread_rwlock_t *lock);

#endif /* __CCACHE_LOCK_H__ */

