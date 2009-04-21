/********************************************************************
	created:	2008/05/24
	filename: 	error.c
	author:		Lichuang
                
	purpose:    
*********************************************************************/

#include "ccache_error.h"

int ccache_errno;
int ccache_error_line;
char* ccache_error_file;

const char* ccache_error_msg[CCACHE_ERROR_NUM] = 
{
    "success"
    , "init error"
    , "NULL pointer"
    , "lock/unlock error"
    , "key exist"
    , "key not exist"
    , "alloc node error"
};

