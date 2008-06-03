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

extern int AT91F_DataflashInit (void);
extern void dataflash_print_info (void);


#define DATAFLASH_MAX_PAGESIZE		1056
#define DATAFLASH_LOADER_BASE		   (0*DATAFLASH_MAX_PAGESIZE)
#define DATAFLASH_UBOOT_BASE		  (12*DATAFLASH_MAX_PAGESIZE)
#define DATAFLASH_ENV_UBOOT_BASE	 (122*DATAFLASH_MAX_PAGESIZE)
#define DATAFLASH_KERNEL_BASE		 (130*DATAFLASH_MAX_PAGESIZE)


#define EBI_CSA 0xFFFFFF60
#define SDRAMC_MR 0xFFFFFF90
#define SDRAMC_TR 0xFFFFFF94
#define SDRAMC_CR 0xFFFFFF98
#define PIOC_ASR (0xFFFFF800+0x70)
#define PIOC_BSR (0xFFFFF800+0x74)
#define PIOC_PDR (0xFFFFF800+0x4)

const int ONE_MBYTES = 1024 * 1024;

/* TODO:
 * This test has to be improved using different patterns, just as memtest86 does.
 * This one will help us anyway.
 * */

int memory_test(int base, int len)
{
  volatile int *ptr;
  int i;

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
      return 1; /* Error */

  return 0;
}



/*
SDRAMC_CR = 2188A15X      where X = NR  NC  (NR, NC 2bits)
NC    ColumnBits
0         8
1         9
2         10
3         11


NR    RowBits
0         11
1         12
2         13
3         Reserved
*/

int try_configure_sdram (int mb)
{
  int i;

#ifdef DEBUG
  uintprint(mb), puts ("MB?\n");
#endif

  //Setup GPIO
  outl(PIOC_ASR,0xffff0000);
  outl(PIOC_BSR,0);
  outl(PIOC_PDR,0xffff0000);
  //Select memory controler 
  outl(EBI_CSA, 0x2);

  switch(mb)
  {
    case 16:
      outl(SDRAMC_CR, 0x2188A154);    // SDRAM 8M    Row=A0-A11, COL=A0-A7
      break;
   case 32:
      outl(SDRAMC_CR, 0x2188A155);    // SDRAM 16M   Row=A0-A11, COL=A0-A8
      break;
   case 64:
      outl(SDRAMC_CR, 0x2188A159);    // SDRAM 32M   Row=A0-A12, COL=A0-A8
      break;
   case 128:
      outl(SDRAMC_CR, 0x2188A15A);    // SDRAM 64M   Row=A0-A12, COL=A0-A9
      break;
   default:
      puts("\ntry_configure_sdram\n");
      hang();
  }

  outl(SDRAMC_MR, 0x2);
  outl(AT91_SDRAM_BASE, 0);
  outl(SDRAMC_MR, 0x4);
  outl(AT91_SDRAM_BASE, 0);
  outl(AT91_SDRAM_BASE, 0);
  outl(AT91_SDRAM_BASE, 0);
  outl(AT91_SDRAM_BASE, 0);
  outl(AT91_SDRAM_BASE, 0);
  outl(AT91_SDRAM_BASE, 0);
  outl(AT91_SDRAM_BASE, 0);
  outl(AT91_SDRAM_BASE, 0);
  outl(SDRAMC_MR, 0x3);
  for(i = 0; i < 100; i++);
  outl(0x20000080, 0);
  outl(SDRAMC_TR, 0x1C0);
  outl(AT91_SDRAM_BASE, 0);
  outl(SDRAMC_MR, 0x0);

  for (i = mb; i >= 1; --i)
    *((char*) (AT91_SDRAM_BASE + i * ONE_MBYTES - 1)) = i;

  for (i = mb; i >= 1; --i)
   if (*((char*) (AT91_SDRAM_BASE + i * ONE_MBYTES - 1)) != i)
     return 1;

  if (memory_test(AT91_SDRAM_BASE, 0xff))
  {
    puts("\nPreliminary RAM test failed");
    hang();
  }

  return 0;
}


void start_armboot (void)
{
  int len, i;
  int ram_size;


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

  puts("\n.\n.\n.\nDarrell's loader - Thanks to the u-boot project\nVersion 1.0. Build " __DATE__ " " __TIME__ "\n");
  puts("Modified to BOFF board by Lucjan Bryndza <lucjan.bryndza@ep.com.pl>\n");

  /* Let's detect how much RAM we have */
  for (i = 128; i >= 16; i /= 2)
    if (!try_configure_sdram(i))
    {
      ram_size = i;
      break;
    }

  if (i < 8)
    puts ("Couldn't determine ram size"), hang();

  puts("DRAM:"), uintprint(ram_size), puts("MB\n");

  int key = 0, autoboot = 1, scans = 0, dispmenu = 1;

  while(1) /* loop forever until u-boot gets booted or the board is reset */
{
    if(dispmenu)
    {
      puts("\n1: Upload Darrell's loader to Dataflash\n");
      puts("2: Upload u-boot to Dataflash\n");
      puts("3: Upload Kernel to Dataflash\n");
      puts("4: Start u-boot\n");
      puts("5: Erase bootsector in flash\n");
      puts("6: Memory test\n");
      dispmenu = 0;
    }
    if(tstc())
    {
      key = getc();
      autoboot = 0;
    }
    else
      key = 0;
    if(key == '1'){
      puts("Please transfer darrel loader via Xmodem\n\0");
      len = rxmodem((char *)0x20000000);
      puts("Received ");
      hexprint(len);
      puts(" bytes\n");
      outl(0x20000014, ((528 << 17) + (12 << 13) + 24));
      puts("Modified ARM vector 6\n");
      AT91F_DataflashInit ();
      dataflash_print_info ();
      if(write_dataflash(DATAFLASH_LOADER_BASE, 0x20000000, len))
        puts("Dataflash write successful\n");
      dispmenu = 1;
    }
    else if(key == '2'){
      puts("Please transfer u-boot.bin via Xmodem\n\0");
      len = rxmodem((char *)0x20000000);
      AT91F_DataflashInit ();
      dataflash_print_info ();
      if(write_dataflash(DATAFLASH_UBOOT_BASE, 0x20000000, len))
        puts("Dataflash write successful\n");
      dispmenu = 1;
    }
    else if(key == '3'){
      puts("Please transfer Kernel via Xmodem\n\0");
      len = rxmodem((char *)0x20000000);
      puts("\nPlease wait...\n");
      AT91F_DataflashInit ();
      dataflash_print_info ();
      if(write_dataflash(DATAFLASH_KERNEL_BASE, 0x20000000, len))
        puts("Dataflash write successful\n");
      dispmenu = 1;
    }
    else if(key == '4' || ((scans > 300000) && autoboot)){
      if(AT91F_DataflashInit ()){
        dataflash_print_info ();
        if(read_dataflash(DATAFLASH_UBOOT_BASE, 0x1C000, (char *)0x20700000)){
          puts("Dataflash read successful: Starting U-boot\n");
          asm("ldr pc, =0x20700000");
        }
      }

      puts("Dataflash not found\n");
      scans = 0;
      dispmenu = 1;
    }
     else if(key == '6'){
      puts ("\nTesting RAM, Detected "), uintprint(ram_size), puts("MB ==> ");

      if (!memory_test(AT91_SDRAM_BASE, ram_size * ONE_MBYTES))
        puts ("OK\n");
      else
        puts ("FAILED\n");

      dispmenu = 1;
    }
    else if(key == '5')
    {
        puts("Erase dataflash - ");
        for(i=0;i<256;i+=4)
        outl(0x20000000+i, 0xff);
        AT91F_DataflashInit ();
        dataflash_print_info ();
        if(write_dataflash(DATAFLASH_LOADER_BASE, 0x20000000, 256))
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

void
hang (void)
{
  puts ("### ERROR ### Please RESET the board ###\n");
  for (;;);
}

