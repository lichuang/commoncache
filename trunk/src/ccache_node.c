/********************************************************************
	created:	2008/01/23
	filename: 	node.c
	author:		Lichuang
                
	purpose:    
*********************************************************************/

#include "ccache.h"
#include "ccache_node.h"

int 
ccache_count_nodesize(const ccache_data_t* data)
{
    return (sizeof(ccache_node_t) +
            sizeof(char) * data->keysize +
            sizeof(char) * data->datasize);
}

