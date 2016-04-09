/*
 * (C) Copyright 2004-2005 Darrell Harmon mail@dlharmon.com
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

/* #define DEBUG  */

#include <common.h>
#include <version.h>
#include <asm/hardware.h>
#include <dataflash.h>
#include <xmodem.h>
#include <io.h>
#include <decompress.h>

extern int AT91F_DataflashInit (void);
extern void dataflash_print_info (void);

#define SCRATCH_RAM_ADR AT91_SDRAM_BASE
#define SCRATCH_RAM_BPTR ((uint8_t*)SCRATCH_RAM_ADR)



#define DATAFLASH_LOADER_LEN (DATAFLASH_UBOOT_BASE-DATAFLASH_LOADER_BASE)
#define DATAFLASH_UBOOT_LEN (DATAFLASH_ENV_UBOOT_BASE-DATAFLASH_UBOOT_BASE)
#define DATAFLASH_TOTAL_LEN (DATAFLASH_ENV_UBOOT_END-DATAFLASH_LOADER_BASE)
#define DATAFLASH_UBOOT_MINLEN 8192
#define MIN_IMAGE_SIZE 256

#define UBOOT_SDRAM_COMPR_ADDR 0x21e00000
#define UBOOT_SDRAM_COMPR ((unsigned char *)UBOOT_SDRAM_COMPR_ADDR)
#define UBOOT_SDRAM_START ((unsigned char *)0x21f00000)


#define EBI_CSA 0xFFFFFF60
#define SDRAMC_MR 0xFFFFFF90
#define SDRAMC_TR 0xFFFFFF94
#define SDRAMC_CR 0xFFFFFF98
#define PIOC_ASR (0xFFFFF800+0x70)
#define PIOC_BSR (0xFFFFF800+0x74)
#define PIOC_PDR (0xFFFFF800+0x4)
#define PIOC_PER (0xFFFFF800+0x00)

#define ONE_MBYTES (1024 * 1024)

/* TODO:
 * This test has to be improved using different patterns, just as memtest86 does.
 * This one will help us anyway.
 * */

static int memory_test(unsigned int base, size_t len)
{
	volatile int *ptr;
	size_t i;

#ifdef DEBUG
	puts ("Writing... ");
#endif
	ptr = (int *) base;
	for (i = 0; i < len / sizeof(int); i++)
		*(ptr++) = i;

#ifdef DEBUG
	puts ("Reading... ");
#endif

	ptr = (int *) base;
	for (i = 0; i < len / sizeof(int); i++)
		if (*(ptr++) != i)
		{
			return 1; /* Error */
		}

	return 0;
}


static void configure_sdram (void)
{
	int i;

#ifdef DEBUG
	uintprint(mb), puts ("MB?\n");
#endif

#if SDRAM_BUS_16BIT==0
	//Setup GPIO
	outl(PIOC_ASR,0xffff0000);
	outl(PIOC_BSR,0);
	outl(PIOC_PDR,0xffff0000);
#endif
	//PC gpio AS IO fix PC7 PC8 PC9 PC10 PC11 PC12 PC13
	outl(PIOC_PER,0x3F80);
	//Select memory controler
	outl(EBI_CSA, 0x2);

	//outl(SDRAMC_CR, 0x2188A159);    // SDRAM 64M   Row=A0-A12, COL=A0-A8
	outl(
			SDRAMC_CR, (SDRAM_COL)|(SDRAM_ROW<<2)|(SDRAM_BANKS<<4)|(SDRAM_CAS<<5)|(SDRAM_TWR<<7)|
			(SDRAM_TRC<<11)|(SDRAM_TRP<<15)|(SDRAM_TRCD<<19)|(SDRAM_TRAS<<23)|SDRAM_TXSR<<27
		);

	outl(SDRAMC_MR, 0x2 | (SDRAM_BUS_16BIT<<4) );
	outl(AT91_SDRAM_BASE, 0);
	outl(SDRAMC_MR, 0x4 | (SDRAM_BUS_16BIT<<4) );
	outl(AT91_SDRAM_BASE, 0);
	outl(AT91_SDRAM_BASE, 0);
	outl(AT91_SDRAM_BASE, 0);
	outl(AT91_SDRAM_BASE, 0);
	outl(AT91_SDRAM_BASE, 0);
	outl(AT91_SDRAM_BASE, 0);
	outl(AT91_SDRAM_BASE, 0);
	outl(AT91_SDRAM_BASE, 0);
	outl(SDRAMC_MR, 0x3 | (SDRAM_BUS_16BIT<<4) );
	for(i = 0; i < 100; i++);
	outl(0x20000080, 0);
	outl(SDRAMC_TR, SDRAM_REFRESH);
	outl(AT91_SDRAM_BASE, 0);
	outl(SDRAMC_MR, 0x0 | (SDRAM_BUS_16BIT<<4) );

	//Some delay
	for(i=0;i<10000;i++) asm volatile("nop");

	if (memory_test(AT91_SDRAM_BASE+(SDRAM_SIZE-1)*ONE_MBYTES, ONE_MBYTES))
	{
		puts("\nPreliminary RAM test failed");
		hang();
	}
}

void start_armboot (void)
{
	int len, i;

	/* PMC Clock Initialization */
	AT91PS_PMC pmc = (AT91PS_PMC)AT91C_BASE_PMC;
	pmc->CKGR_PLLAR = 0x20269004;

	while(!(pmc->PMC_SR & 0x2));
	pmc->CKGR_PLLBR = 0x10193E05;
	while(!(pmc->PMC_SR & 0x4));
	pmc->PMC_MCKR = 0x202;    /*Select PLLA as MCK*/

	interrupt_init();
	serial_init();
	for(i=0; i < 10000; i++); /* Some of the parts want some time after powerup */
	puts("\n.\n.\n.\nBoFFLDR based on darrel-loader\nVersion 2.0 Build " __DATE__ " " __TIME__ "\n");

	configure_sdram();
	puts("DRAM:"), uintprint(SDRAM_SIZE), puts("MB\n");

	int key = 0, autoboot = 1, scans = 0, dispmenu = 1;

	while(1) /* loop forever until u-boot gets booted or the board is reset */
	{
		if(dispmenu)
		{
			puts("\n1: Upload loader\n");
			puts("2: Upload u-boot\n");
			//puts("3: Upload Kernel to Dataflash\n");
			puts("4: Start u-boot\n");
			puts("5: Erase flash\n");
#if CONFIG_SDRAM_TEST
			puts("6: Memory test\n");
#endif
			dispmenu = 0;
		}
		if(tstc())
		{
			key = getc();
			if( key == CONFIG_AUTOBOOT_ESCAPE ) {
				autoboot = 0;
				key = 0;
			}
		}
		else
			key = 0;
		if(key == '1'){
			puts("Please send loader.bin via Xmodem\n");
			len = rxmodem(SCRATCH_RAM_BPTR);
			if( len > MIN_IMAGE_SIZE && len < DATAFLASH_LOADER_LEN ) {
				outl(SCRATCH_RAM_ADR + 0x00000014, ((528 << 17) + (12 << 13) + 24));
				AT91F_DataflashInit ();
				dataflash_print_info ();
				if(write_dataflash(DATAFLASH_LOADER_BASE, SCRATCH_RAM_ADR, len))
					puts("Flash WR OK\n");
			} else {
				puts("\nFlash size Error\n");
			}
			dispmenu = 1;
		}
		else if(key == '2'){
			puts("Please send u-boot.bin via Xmodem\n");
			len = rxmodem(SCRATCH_RAM_BPTR+sizeof(long));
			if( len > MIN_IMAGE_SIZE && len < DATAFLASH_UBOOT_LEN ) {
				outl( SCRATCH_RAM_ADR,  len ); //Uboot bin length first
				AT91F_DataflashInit ();
				dataflash_print_info ();
				if(write_dataflash(DATAFLASH_UBOOT_BASE, SCRATCH_RAM_ADR, len+sizeof(long)))
					puts("Flash WR OK\n");
			} else {
				puts("\nFlash size Error\n");
			}
			dispmenu = 1;
		}
		else if(key == '4' || ((scans > 300000) && autoboot)){
			if(AT91F_DataflashInit ()){
				dataflash_print_info ();
				if(read_dataflash(DATAFLASH_UBOOT_BASE,DATAFLASH_UBOOT_LEN, UBOOT_SDRAM_COMPR )){
					int l = inl(UBOOT_SDRAM_COMPR_ADDR);
					if( l>= DATAFLASH_UBOOT_MINLEN && l <= DATAFLASH_UBOOT_LEN ) {
						l = decompress_image(UBOOT_SDRAM_START,UBOOT_SDRAM_COMPR+sizeof(long),l);
						if(l) {
							puts("Decompress u-boot siz ");
							uintprint(l); 
							putc('\n');
							goto *(UBOOT_SDRAM_START);
						} else {
							puts("Uncompress u-boot fail\n");
						}
					} else {
						puts("Invalid compr len\n");
					}
				}
			} else {
				puts("Dataflash not found\n");
			}
			scans = 0;
			dispmenu = 1;
			autoboot = 0;
		}
#if CONFIG_SDRAM_TEST
		else if(key == '6'){
			puts ("\nTesting RAM, Detected "), uintprint(SDRAM_SIZE), puts("MB ==> ");

			if (!memory_test(AT91_SDRAM_BASE, SDRAM_SIZE * ONE_MBYTES))
				puts ("OK\n");
			else
				puts ("FAILED\n");

			dispmenu = 1;
		}
#endif
		else if(key == '5')
		{
			puts("Erase dataflash - ");
			for(i=0;i<DATAFLASH_TOTAL_LEN/sizeof(long);i+=sizeof(long))
				outl(SCRATCH_RAM_ADR+i, 0xff);
			AT91F_DataflashInit ();
			dataflash_print_info ();
			if(write_dataflash(DATAFLASH_LOADER_BASE, SCRATCH_RAM_ADR, DATAFLASH_TOTAL_LEN))
				puts("Done\n");
			else
				puts("Failed\n");
			dispmenu = 1;
		}
		else if(key == 0)
			dispmenu = 0;
		else{
			puts("Invalid input\n");
			dispmenu = 1;
		}
		scans++;
	}
	/* NOTREACHED - no way out of command loop except booting */
}

void hang (void)
{
	puts ("### ERROR ### Please RESET the board ###\n");
	for (;;);
}
