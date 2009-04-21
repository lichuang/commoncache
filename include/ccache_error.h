/********************************************************************
	created:	2008/05/24
	filename: 	error.h
	author:		Lichuang
                
	purpose:    
*********************************************************************/

#ifndef __CCACHE_ERROR_H__
#define __CCACHE_ERROR_H__

enum ccache_error_t
{
    CCACHE_SUCCESS          = 0,
    CCACHE_INIT_ERROR       = 1,
    CCACHE_NULL_POINTER     = 2,
    CCACHE_LOCK_ERROR       = 3,
    CCACHE_KEY_EXIST        = 4,
    CCACHE_KEY_NOT_EXIST    = 5,
    CCACHE_ALLOC_NODE_ERROR = 6,
    CCACHE_ERROR_NUM  
};

/* note: not thread safe! */
extern const char* ccache_error_msg[CCACHE_ERROR_NUM];
extern int ccache_errno;
extern int ccache_error_line;
extern char* ccache_error_file;

#define CCACHE_SET_ERROR_NUM(errno)     \
    do                                  \
    {                                   \
        ccache_errno = errno;           \
        ccache_error_line = __LINE__;   \
        ccache_error_file = __FILE__;   \
    } while (0)

#define CCACHE_SET_SUCCESS                     \
    do                                  \
    {                                   \
        ccache_errno = CCACHE_SUCCESS;  \
    } while (0)

#endif /* __CCACHE_ERROR_H__ */

