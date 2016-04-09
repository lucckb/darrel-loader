/*
 * =====================================================================================
 *
 *       Filename:  lzf.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09.04.2016 00:18:46
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Lucjan Bryndza (LB), lucck(at)boff(dot)pl
 *   Organization:  BoFF
 *
 * =====================================================================================
 */

#pragma once

#include <stddef.h>
#include <common.h>


int lzf_decompress (const void *const in_data,  unsigned int in_len,
                void             *out_data, unsigned int out_len);
size_t decompress_image( void* outmem, const void* inmem, size_t insize );

