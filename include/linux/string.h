/*
 * =====================================================================================
 *
 *       Filename:  string.h
 *
 *    Description:  Minimal subset of libc small as possible
 *
 *        Version:  1.0
 *        Created:  08.04.2016 23:50:45
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Lucjan Bryndza (LB), lucck(at)boff(dot)pl
 *   Organization:  BoFF
 *
 * =====================================================================================
 */


#ifndef  string_INC
#define  string_INC

#include <stddef.h>

static inline void *memcpy(void *dest, const void *src, size_t n)
{
	for( size_t i=0; i<n; ++i ) 
		((char*)dest)[i] = ((char*)src)[i];
	return dest;
}


#endif   /* ----- #ifndef string_INC  ----- */
