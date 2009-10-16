/********************************************************************
	created:	2008/09/22
	filename: 	functor.c
	author:		Lichuang
                
	purpose:    
*********************************************************************/

#include "ccache_functor.h"
#include <stdlib.h>

#ifdef CCACHE_USE_LIST
    #include "ccache_list.h"
#elif defined CCACHE_USE_RBTREE    
    #include "ccache_rbtree.h"
#endif

int 
ccache_init_functor(ccache_functor_t* functor)
{
#ifdef CCACHE_USE_LIST
    return ccache_init_list_functor(functor);
#elif defined CCACHE_USE_RBTREE    
    return ccache_init_rbtree_functor(functor);
#else    
    (void)functor;
    return -1;
#endif    
}

