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
#include "linux/string.h"

#ifdef BUILD_TAG
#define LOADER_TAG0 " [" BUILD_TAG "]"
#else
#define LOADER_TAG0 ""
#endif
/* Test builds (never for production) are marked in the banner */
#if defined(TEST_HOOKS) || defined(DRYRUN_FLASH) || CONFIG_SDRAM_TEST
#define LOADER_TAG LOADER_TAG0 " TEST-BUILD"
#else
#define LOADER_TAG LOADER_TAG0
#endif

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
/* u-boot must fit below the 32 MB boundary; common to the 32 and 64 MB builds */
#define UBOOT_MAX_SIZE (0x22000000 - 0x21f00000)
/* u-boot image in dataflash: [compressed length][UBOOT_HDR_MAGIC<<16 | crc16]
 * [LZF data]. Only this format is accepted. */
#define UBOOT_HDR_MAGIC 0xB0FF
#define UBOOT_HDR_SIZE (2 * sizeof(long))
/* time to wait for a key before u-boot is started */
#define AUTOBOOT_DELAY (1 * CFG_HZ)


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

#if CONFIG_SDRAM_TEST
/* Extended test (menu option 6): four passes over the whole range, each one
 * writes everything and then reads everything back.
 *   0: word index   - a unique value per word, finds aliased addresses
 *   1: ~word index  - together with 0 every bit is seen as 0 and as 1
 *   2: 0x55555555 / 3: 0xAAAAAAAA - shorted neighbouring data lines
 * Prints the first failing word. */
static unsigned int mt_pattern(unsigned int i, int pass)
{
	switch (pass) {
	case 0:  return i;
	case 1:  return ~i;
	case 2:  return 0x55555555;
	default: return 0xAAAAAAAA;
	}
}

static int memory_test_full(unsigned int base, size_t len)
{
	volatile unsigned int *p;
	unsigned int i, n = len / sizeof(int), v, got;
	int pass;

	for (pass = 0; pass < 4; pass++) {
		p = (volatile unsigned int *)base;
		for (i = 0; i < n; i++) {
			*p++ = mt_pattern(i, pass);
			if (!(i & 0x3FFFF))	/* progress: a dot per MB */
				putc('.');
		}
		p = (volatile unsigned int *)base;
		for (i = 0; i < n; i++) {
			v = mt_pattern(i, pass);
			if (!(i & 0x3FFFF))
				putc('.');
			got = *p++;
			if (got != v) {
				puts("\nFAIL @"), hexprint((unsigned int)(p - 1));
				puts(" exp "), hexprint(v);
				puts(" got "), hexprint(got), putc('\n');
				return 1;
			}
		}
		putc('|');
	}
	return 0;
}
#endif


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
	udelay(1);
	outl(0x20000080, 0);
	outl(SDRAMC_TR, SDRAM_REFRESH);
	/* Second access while still in load-mode-register mode. Kept on
	 * purpose: this sequence (same as U-Boot's lowlevel_init for the
	 * RM9200) runs on 32 MB boards for years and was verified on the 64 MB
	 * part. Do not change it without a logic analyzer measurement. */
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

/* Second RAM buffer for the read-back verification */
#define VERIFY_RAM_ADR (AT91_SDRAM_BASE + 0x800000)

/* Throw away pending UART input until the line is quiet for 1 s (at most
 * 32 K chars), e.g. the tail of an aborted XMODEM transfer. */
static void drain_uart (void)
{
	unsigned int n = 0;

	reset_timer_masked();
	while (get_timer_masked() < CFG_HZ && n < 32768) {
		if (tstc()) {
			getc();
			n++;
			reset_timer_masked();
		}
	}
}

/* A loader image starts with the 8 word exception vector table:
 * "b reset" followed by seven "ldr pc, [pc, #x]". */
static int loader_image_ok (const unsigned char *img)
{
	const unsigned int *w = (const unsigned int *)img;
	unsigned int i;

	if ((w[0] & 0xFF000000) != 0xEA000000)
		return 0;
	for (i = 1; i < 8; i++)
		if ((w[i] & 0xFFFFF000) != 0xE59FF000)
			return 0;
	return 1;
}

/* Wait up to 5 s for an explicit 'y' */
static int confirm_yes (const char *msg)
{
	drain_uart();
	puts(msg);
	reset_timer_masked();
	while (get_timer_masked() < 5 * CFG_HZ)
		if (tstc())
			return getc() == 'y';
	return 0;
}

/* The SPI PDC receives into the TX buffer too, so the source image is
 * destroyed by write_dataflash(). Keep a copy and compare the read-back
 * data (placed in the original buffer) against it. */
static int flash_verify (unsigned long addr, unsigned long len)
{
	const unsigned char *a = (const unsigned char *)VERIFY_RAM_ADR;
	const unsigned char *b = (const unsigned char *)SCRATCH_RAM_ADR;

	if (read_dataflash(addr, len, (unsigned char *)b) != DATAFLASH_OK)
		return 0;
	while (len--)
		if (*a++ != *b++)
			return 0;
	return 1;
}

/* Write len bytes from SCRATCH_RAM_ADR to dst and verify them by reading back */
static void flash_store (unsigned long dst, unsigned long len)
{
	int rc;

	if (!AT91F_DataflashInit()) {
		puts("Dataflash not found\n");
		return;
	}
	dataflash_print_info();
	memcpy((void *)VERIFY_RAM_ADR, (void *)SCRATCH_RAM_ADR, len);
	rc = write_dataflash(dst, SCRATCH_RAM_ADR, len);
	if (rc != DATAFLASH_OK) {
		puts("Flash WR FAIL "), uintprint(rc), putc('\n');
		return;
	}
	if (!flash_verify(dst, len)) {
		puts("Verify FAIL\n");
		return;
	}
	puts("Flash WR OK\n");
}

/* PMC clock initialization: PLLA -> master clock */
static void clock_init (void)
{
	AT91PS_PMC pmc = (AT91PS_PMC)AT91C_BASE_PMC;

	pmc->CKGR_PLLAR = 0x20269004;
	while(!(pmc->PMC_SR & 0x2));
	pmc->CKGR_PLLBR = 0x10193E05;
	while(!(pmc->PMC_SR & 0x4));
	pmc->PMC_MCKR = 0x202;    /*Select PLLA as MCK*/
}

static void print_menu (void)
{
	puts("\n1: Upload loader\n");
	puts("2: Upload u-boot\n");
	puts("4: Start u-boot\n");
	puts("5: Erase flash\n");
#if CONFIG_SDRAM_TEST
	puts("6: Memory test\n");
#endif
}

/* Menu 1: receive loader.bin by XMODEM and write it to the dataflash */
static void upload_loader (void)
{
	int len;

	puts("Please send loader.bin via Xmodem\n");
	len = rxmodem(SCRATCH_RAM_BPTR, DATAFLASH_LOADER_LEN);
	if( len > MIN_IMAGE_SIZE && len < DATAFLASH_LOADER_LEN &&
	    loader_image_ok(SCRATCH_RAM_BPTR) ) {
		/* dataflash geometry for the BootROM goes to the vector 6 slot */
		outl(SCRATCH_RAM_ADR + 0x00000014, ((528 << 17) + (12 << 13) + 24));
		flash_store(DATAFLASH_LOADER_BASE, len);
	} else {
		drain_uart();
		puts("\nImage error\n");
	}
}

/* Menu 2: receive the LZF compressed u-boot, add the length and CRC header */
static void upload_uboot (void)
{
	int len;

	puts("Please send u-boot.bin via Xmodem\n");
	len = rxmodem(SCRATCH_RAM_BPTR+UBOOT_HDR_SIZE, DATAFLASH_UBOOT_LEN - UBOOT_HDR_SIZE);
	/* LZF framed image: starts with "ZV" */
	if( len > MIN_IMAGE_SIZE && len <= DATAFLASH_UBOOT_LEN - UBOOT_HDR_SIZE &&
	    SCRATCH_RAM_BPTR[UBOOT_HDR_SIZE] == 'Z' && SCRATCH_RAM_BPTR[UBOOT_HDR_SIZE+1] == 'V' ) {
		outl( SCRATCH_RAM_ADR,  len ); //Uboot bin length first
		outl( SCRATCH_RAM_ADR+sizeof(long), (UBOOT_HDR_MAGIC << 16) |
			crc16_ccitt(SCRATCH_RAM_BPTR+UBOOT_HDR_SIZE, len) );
		flash_store(DATAFLASH_UBOOT_BASE, len+UBOOT_HDR_SIZE);
	} else {
		drain_uart();
		puts("\nImage error\n");
	}
}

/* Menu 4 and autoboot: read, check and decompress u-boot, then jump to it.
 * Returns only when something failed. */
static void boot_uboot (void)
{
	int l;
	unsigned int w;

	if(!AT91F_DataflashInit ()){
		puts("Dataflash not found\n");
		return;
	}
	dataflash_print_info ();
	if(read_dataflash(DATAFLASH_UBOOT_BASE,DATAFLASH_UBOOT_LEN, UBOOT_SDRAM_COMPR ) != DATAFLASH_OK)
		return;
	l = inl(UBOOT_SDRAM_COMPR_ADDR);
	w = inl(UBOOT_SDRAM_COMPR_ADDR + sizeof(long));

	if( (w >> 16) != UBOOT_HDR_MAGIC ) {
		/* images without the CRC header are not accepted */
		puts("Invalid u-boot header\n");
	} else if( l < DATAFLASH_UBOOT_MINLEN || l > DATAFLASH_UBOOT_LEN - UBOOT_HDR_SIZE ) {
		puts("Invalid compr len\n");
	} else if( crc16_ccitt(UBOOT_SDRAM_COMPR+UBOOT_HDR_SIZE, l) != (w & 0xFFFF) ) {
		puts("u-boot CRC error\n");
	} else {
		l = decompress_image(UBOOT_SDRAM_START,UBOOT_SDRAM_COMPR+UBOOT_HDR_SIZE,l,UBOOT_MAX_SIZE);
		if(l) {
			puts("Decompress u-boot size ");
			uintprint(l); 
			putc('\n');
			((void (*)(void))UBOOT_SDRAM_START)();
		} else {
			puts("Uncompress u-boot fail\n");
		}
	}
}

/* Menu 5: erase the whole dataflash (after an explicit 'y') */
static void erase_flash (void)
{
	unsigned int i;

	if(!confirm_yes("Erase ALL dataflash incl. loader? [y/N] ")) {
		puts("\nCancelled\n");
		return;
	}
	puts("\nErase dataflash - ");
	for(i=0;i<DATAFLASH_TOTAL_LEN;i+=sizeof(long))
		outl(SCRATCH_RAM_ADR+i, 0xFFFFFFFF);
#ifdef DRYRUN_FLASH
	{
		const unsigned char *q = (const unsigned char *)SCRATCH_RAM_ADR;
		unsigned int mn = 255, mx = 0, k;

		for(k=0;k<DATAFLASH_TOTAL_LEN;k++){
			if(q[k] < mn) mn = q[k];
			if(q[k] > mx) mx = q[k];
		}
		puts("DRYRUN len "), uintprint(DATAFLASH_TOTAL_LEN);
		puts(" min "), hexprint(mn), puts(" max "), hexprint(mx), putc('\n');
	}
#else
	flash_store(DATAFLASH_LOADER_BASE, DATAFLASH_TOTAL_LEN);
#endif
}

#if CONFIG_SDRAM_TEST
/* Menu 6: extended SDRAM test */
static void ram_test (void)
{
	puts ("\nTesting RAM, Detected "), uintprint(SDRAM_SIZE), puts("MB ==> ");
	if (!memory_test_full(AT91_SDRAM_BASE, SDRAM_SIZE * ONE_MBYTES))
		puts ("OK\n");
	else
		puts ("FAILED\n");
}
#endif

void start_armboot (void)
{
	int key, autoboot = 1, dispmenu = 1;

	clock_init();
	interrupt_init();
	serial_init();
	udelay(200); /* SDRAM needs >= 200 us after power-up before init */
	puts("\n.\n.\n.\nBoFFLDR based on darrel-loader\nVersion " LOADER_VERSION " SDRAM " STR(SDRAM_MB) "MB" LOADER_TAG " Build " __DATE__ " " __TIME__ "\n");

	configure_sdram();
	puts("DRAM:"), uintprint(SDRAM_SIZE), puts("MB\n");

	while(1) /* loop forever until u-boot gets booted or the board is reset */
	{
		if(dispmenu)
		{
			reset_timer_masked();	/* restart the autoboot countdown */
			print_menu();
			dispmenu = 0;
		}
		key = 0;
		if(tstc())
		{
			key = getc();
			if( key == CONFIG_AUTOBOOT_ESCAPE ) {
				autoboot = 0;
				key = 0;
			}
		}
		if(key == 0 && autoboot && get_timer_masked() >= AUTOBOOT_DELAY)
			key = '4';

		switch(key)
		{
		case 0:
			break;
		case '1':
			upload_loader();
			dispmenu = 1;
			break;
		case '2':
			upload_uboot();
			dispmenu = 1;
			break;
		case '4':
			boot_uboot();	/* returns only on failure */
			autoboot = 0;
			dispmenu = 1;
			break;
		case '5':
			erase_flash();
			dispmenu = 1;
			break;
#if CONFIG_SDRAM_TEST
		case '6':
			ram_test();
			dispmenu = 1;
			break;
#endif
#ifdef TEST_HOOKS
		case 'u':
			/* exercise the exception path: permanently undefined instruction */
			puts("TEST: undefined instruction\n");
			asm volatile (".word 0xe7f000f0");
			break;
		case 'f':
			/* write to an invalid dataflash address: must fail before any SPI command */
			flash_store(0x7FFFFFFF, 128);
			dispmenu = 1;
			break;
#endif
		default:
			puts("Invalid input\n");
			dispmenu = 1;
			break;
		}
	}
	/* NOTREACHED - no way out of command loop except booting */
}

void hang (void)
{
	puts ("### ERROR ### Please RESET the board ###\n");
	for (;;);
}
