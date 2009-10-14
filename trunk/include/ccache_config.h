/********************************************************************
	created:	2009/10/04
	filename: 	ccache_config.h
	author:		Lichuang
                
	purpose:    
*********************************************************************/

#ifndef __CCACHE_CONFIG_H__
#define __CCACHE_CONFIG_H__

#include "ccache.h"

extern ccache_config_t cache_config;
extern int cache_align_size;

int ccache_init_config(const char *configfile);

#endif /* __CCACHE_CONFIG_H__ */

