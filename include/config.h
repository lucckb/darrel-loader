/*
 * Rick Bronson <rick@efn.org>
 * Adapted for dlharmonsbc by Darrell Harmon <dlharmon@dlharmon.com>
 *
 * Configuation settings for the DLHARMONSBC board.
 * Based on AT91RM9200 configuration
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#define __CONFIG_H


#define CFG_GBL_DATA_SIZE (128)
#define CFG_MALLOC_LEN (512 * 1024)
	
/*
 * If we are developing, we might want to start armboot from ram
 * so we MUST NOT initialize critical regs like mem-timing ...
 */
#define CONFIG_INIT_CRITICAL            /* undef for developing */

/* ARM asynchronous clock */
#define AT91C_MAIN_CLOCK  179712000  /* from 18.432 MHz crystal (18432000 / 4 * 39) */
#define AT91C_MASTER_CLOCK  59904000  /* peripheral clock (AT91C_MASTER_CLOCK / 3) */
#define AT91_SLOW_CLOCK		32768	/* slow clock */

#undef CONFIG_USE_IRQ			/* we don't need IRQ/FIQ stuff */

#define CONFIG_BAUDRATE 115200
#define CFG_AT91C_BRGR_DIVISOR	33	/* hardcode so no __divsi3 : AT91C_MASTER_CLOCK / baudrate / 16 */
#undef	CONFIG_HWFLOW			/* don't include RTS/CTS flow control support	*/
#undef	CONFIG_MODEM_SUPPORT		/* disable modem initialization stuff */
#define CFG_BAUDRATE_TABLE {115200 , 19200, 38400, 57600, 9600 }

#define CONFIG_NR_DRAM_BANKS 1
#define PHYS_SDRAM 0x20000000
#define PHYS_SDRAM_SIZE 0x2000000  /* 32 megs */

#define CONFIG_HAS_DATAFLASH    1
#define CFG_SPI_WRITE_TOUT      (5*CFG_HZ)
#define CFG_MAX_DATAFLASH_BANKS 2
#define CFG_MAX_DATAFLASH_PAGES 16384
#define CFG_DATAFLASH_LOGIC_ADDR_CS0    0xC0000000      /* Logical adress for CS0 */
#define CFG_DATAFLASH_LOGIC_ADDR_CS3    0xD0000000      /* Logical adress for CS3 */

#define	CFG_HZ AT91C_MASTER_CLOCK/2  /* AT91C_TC0_CMR is implicitly set to
					AT91C_TC_TIMER_DIV1_CLOCK */

#define CONFIG_STACKSIZE	(32*1024)	/* regular stack */


