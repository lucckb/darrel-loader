/*
 * (C) Copyright 2007
 * emQbit
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>

void uintprint(unsigned int x)
{
  char stack[10];
  int size = 0;

  while(x >0)
    stack[size++] = x % 10,  x /= 10;

  if (size)
    while (size > 0)
    {
      putc('0' + stack[--size]);
    }
  else
   putc('0');
}

void hexprint(unsigned int hexval)
{
  int digit[8], pos;
  puts("0x");
  for(pos = 0; pos < 8; pos++)
    {
      digit[pos] = (hexval & 0xF);  /* last hexit */
      hexval = hexval >> 4;
    }
  for(pos = 7; pos > -1; pos--)
    {
      if(digit[pos] < 0xA)
	putc(digit[pos] + '0');
      else
	putc(digit[pos] + 'A' - 10);  
    }
  putc(' ');
}

void hexdump(int *addr, int len)
{
  while(len)
    {
      hexprint((int) addr);
      putc(' ');
      hexprint(*addr);
      addr ++;
      putc(' ');
      hexprint(*addr);
      addr ++;
      putc(' ');
      hexprint(*addr);
      addr ++;
      putc(' ');
      hexprint(*addr);
      addr ++;
      len -= 16;
      puts("\n");
    }
}

void outl(int addr, int data)
{
  *(int *)addr = data;
}

int inl(int addr)
{
  return *(int *)addr;
}
