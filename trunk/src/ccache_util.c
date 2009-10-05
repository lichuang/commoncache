/********************************************************************
	created:	2009/10/05
	filename: 	ccache_util.c
	author:		Lichuang
                
	purpose:    
*********************************************************************/

#include <stdio.h>
#include <ctype.h>
#include "ccache_util.h"

void 
ccache_string_trim(char *buffer)
{
    char *p, *tail = NULL;

    for(p = buffer; *buffer; buffer++)
    {
        if (!isspace(*buffer))
        {
            *p++ = *buffer;
            tail = p;
        }
        else
        {
            if (tail)
            {
                *p++ = *buffer;
            }
        }
    }

    if (tail)
    {
        *tail = '\0';
    }
    else
    {
        *p = '\0';
    }
}

int  ccache_ispowerof2(int value)
{
     return value > 0 ? ((value & (~value + 1)) == value ? 1 : 0) : 0;
}

