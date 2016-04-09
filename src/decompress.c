/*
 * =====================================================================================
 *
 *       Filename:  compress.c
 *
 *    Description:   LZF block compression utility
 *
 *        Version:  1.0
 *        Created:  09.04.2016 00:18:46
 *       Revision:  none
 *       Compiler:  gcc
 *		 Based on: http://oldhome.schmorp.de/marc/liblzf.html
 * 		   License: GPL
 *         Author:  Lucjan Bryndza (LB), lucck(at)boff(dot)pl
 *   Organization:  BoFF
 *
 * ===================================================================================== 
 */
#include <decompress.h>
#include <common.h>
#include <linux/types.h>
#include "linux/string.h"

#if CONFIG_UBOOT_LZF_COMPRESSED

#define TYPE0_HDR_SIZE 5
#define TYPE1_HDR_SIZE 7
#define MAX_HDR_SIZE 7
#define MIN_HDR_SIZE 5




/** Decompress image using liblzf format 
 * @param[in] Output memory
 * @param[in] Compesed liblzf image
 * @param[in] LZF input buffer length
 * @return Decompressed image size or 0 if fail
 */
size_t decompress_image( void* outmem, const void* inmem, const size_t insize )
{
	const uint8_t* in = (const uint8_t*)inmem;
	uint8_t* out = (uint8_t*)outmem;
	size_t ret = 0;
	ssize_t cs, us;
	while( ret < insize  ) {
		if( insize<MIN_HDR_SIZE || in[0]!='Z' || in[1]!='V' ) {
			//Fatal return 0
			break;
		}
		if( in[2] == 0 ) {
			cs = -1;
			us = (in[3] << 8) | in[4];
			in += TYPE0_HDR_SIZE;
		} else if( in[2] == 1 ) {
			cs = (in[3] << 8) | in[4];
            us = (in[5] << 8) | in[6];
			in += TYPE1_HDR_SIZE;
			
		} else {
			break;
		}
		if( cs == - 1 ) {
			memcpy( out, in, us );
			in += us; out+= us; 
			ret += us;
		} else {
          if (lzf_decompress (in, cs, out, us) != us ) {
			  return 0;
		  }
		  in += cs;
		  out += us;
		  ret += us;
		}
	}
	return ret;
}

#else
size_t decompress_image( void* outmem, const void* inmem, const size_t insize ) 
{
	memcpy( outmem, inmem, insize );
	return insize;
}
#endif
