/********************************************************************
	created:	2008/01/24
	filename: 	hash.c
	author:		Lichuang
                
	purpose:    

	Jenkins hash support.

	Copyright (C) 1996 Bob Jenkins (bob_jenkins@burtleburtle.net)

	http://burtleburtle.net/bob/hash/
	These are the credits from Bob's sources:
	lookup2.c, by Bob Jenkins, December 1996, Public Domain.
	hash(), hash2(), hash3, and mix() are externally useful functions.
	Routines to test the hash are included if SELF_TEST is defined.
	You can use this free for any purpose.  It has no warranty.

*********************************************************************/

#include "ccache_hash.h"
#include "ccache.h"
#include <string.h>

/* NOTE: Arguments are modified. */
#define ccache_jhash_mix(a, b, c) \
{ \
	  a -= b; a -= c; a ^= (c>>13); \
	  b -= c; b -= a; b ^= (a<<8); \
	  c -= a; c -= b; c ^= (b>>13); \
	  a -= b; a -= c; a ^= (c>>12);  \
	  b -= c; b -= a; b ^= (a<<16); \
	  c -= a; c -= b; c ^= (b>>5); \
	  a -= b; a -= c; a ^= (c>>3);  \
	  b -= c; b -= a; b ^= (a<<10); \
	  c -= a; c -= b; c ^= (b>>15); \
}

/* The golden ration: an arbitrary value */
#define CCACHE_JHASH_GOLDEN_RATIO	0x9e3779b9

typedef unsigned long ccache_u32;
typedef unsigned char ccache_u8;

/* The most generic version, hashes an arbitrary sequence
 * of bytes.  No alignment or length assumptions are made about
 * the input key.
 */
static inline ccache_u32 ccache_jhash(const void *key, ccache_u32 length, ccache_u32 initval)
{
	ccache_u32 a, b, c, len;
	const ccache_u8 *k = key;

	len = length;
	a = b = CCACHE_JHASH_GOLDEN_RATIO;
	c = initval;

	while (len >= 12) 
	{
		a += (k[0] +((ccache_u32)k[1]<<8) +((ccache_u32)k[2]<<16) +((ccache_u32)k[3]<<24));
		b += (k[4] +((ccache_u32)k[5]<<8) +((ccache_u32)k[6]<<16) +((ccache_u32)k[7]<<24));
		c += (k[8] +((ccache_u32)k[9]<<8) +((ccache_u32)k[10]<<16)+((ccache_u32)k[11]<<24));

		ccache_jhash_mix(a,b,c);

		k += 12;
		len -= 12;
	}

	c += length;
	switch (len) 
	{
	case 11: c += ((ccache_u32)k[10]<<24);
	case 10: c += ((ccache_u32)k[9]<<16);
	case 9 : c += ((ccache_u32)k[8]<<8);
	case 8 : b += ((ccache_u32)k[7]<<24);
	case 7 : b += ((ccache_u32)k[6]<<16);
	case 6 : b += ((ccache_u32)k[5]<<8);
	case 5 : b += k[4];
	case 4 : a += ((ccache_u32)k[3]<<24);
	case 3 : a += ((ccache_u32)k[2]<<16);
	case 2 : a += ((ccache_u32)k[1]<<8);
	case 1 : a += k[0];
	};

	ccache_jhash_mix(a,b,c);

	return c;
}

int 
ccache_hash(ccache_t *cache, const void* key, int keysize)
{
    return ccache_jhash(key, keysize, 0) % cache->hashitemnum;
}

int 
ccache_init_hashitem(ccache_t *cache)
{
    int hashitemnum = cache->hashitemnum, i;

    for (i = 0; i < hashitemnum; ++i)
    {
#ifdef CCACHE_USE_LIST
		cache->hashitem[i].first = NULL;
#elif defined CCACHE_USE_RBTREE    
		cache->hashitem[i].root = NULL;
#endif		
        
        cache->hashitem[i].nodenum = 0;
    }

    return 0;
}

