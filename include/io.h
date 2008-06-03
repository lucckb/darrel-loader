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

/* print a unsigned integer */
void uintprint(unsigned int x);

/* print hexadecimal */
void hexprint(unsigned int hexval);

/* dump memory */
void hexdump(int *addr, int len);

/* write to memory / perhaps to a port */
void outl(int addr, int data);

/* read from memory / perhaps from a port */
int inl(int addr);
