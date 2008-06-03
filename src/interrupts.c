/*
 * (C) Copyright 2002
 * Lineo, Inc. <www.lineo.com>
 * Bernhard Kuhn <bkuhn@lineo.com>
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Alex Zuepke <azu@sysgo.de>
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

#include <common.h>
#include <io.h> 
#include <asm/io.h>
#include <asm/arch/hardware.h>
#include <asm/proc/ptrace.h>

extern void reset_cpu (ulong addr);

/* we always count down the max. */
#define TIMER_LOAD_VAL 0xffff

/* macro to read the 16 bit timer */
#define READ_TIMER (tmr->TC_CV & 0x0000ffff)
AT91PS_TC tmr;

#ifdef CONFIG_USE_IRQ
#error There is no IRQ support for AT91RM9200 in U-Boot yet.
#else
void
enable_interrupts (void)
{
  return;
}

int
disable_interrupts (void)
{
  return 0;
}
#endif


void
bad_mode (void)
{
  puts ("Resetting CPU ...\n");
  reset_cpu (0);
}

void
show_regs (struct pt_regs *regs)
{
  return;
}

void
do_undefined_instruction (struct pt_regs *pt_regs)
{
  puts ("undefined instruction\n");
  show_regs (pt_regs);
  bad_mode ();
}

void
do_software_interrupt (struct pt_regs *pt_regs)
{
  puts ("software interrupt\n");
  show_regs (pt_regs);
  bad_mode ();
}

void
do_prefetch_abort (struct pt_regs *pt_regs)
{
  puts ("prefetch abort\n");
  show_regs (pt_regs);
  bad_mode ();
}

void
do_data_abort (struct pt_regs *pt_regs)
{
  puts ("data abort\n");
  hexprint(*(int*)0xFFFFFF08);
  puts ("\n");
  show_regs (pt_regs);
  bad_mode ();
}

void
do_not_used (struct pt_regs *pt_regs)
{
  puts ("not used\n");
  show_regs (pt_regs);
  bad_mode ();
}

void
do_fiq (struct pt_regs *pt_regs)
{
  puts ("fast interrupt request\n");
  show_regs (pt_regs);
  bad_mode ();
}

void
do_irq (struct pt_regs *pt_regs)
{
  puts ("interrupt request\n");
  show_regs (pt_regs);
  bad_mode ();
}

static ulong timestamp;
static ulong lastinc;

int
interrupt_init (void)
{

  tmr = AT91C_BASE_TC0;

  /* enables TC1.0 clock */
  *AT91C_PMC_PCER = 1 << AT91C_ID_TC0;	/* enable clock */

  *AT91C_TCB0_BCR = 0;
  *AT91C_TCB0_BMR =
    AT91C_TCB_TC0XC0S_NONE | AT91C_TCB_TC1XC1S_NONE | AT91C_TCB_TC2XC2S_NONE;
  tmr->TC_CCR = AT91C_TC_CLKDIS;
  tmr->TC_CMR = AT91C_TC_TIMER_DIV1_CLOCK;	/* set to MCLK/2 */

  tmr->TC_IDR = ~0ul;
  tmr->TC_RC = TIMER_LOAD_VAL;
  lastinc = TIMER_LOAD_VAL;
  tmr->TC_CCR = AT91C_TC_SWTRG | AT91C_TC_CLKEN;
  timestamp = 0;

  return (0);
}

/*
 * timer without interrupts
 */

void
reset_timer (void)
{
  reset_timer_masked ();
}

ulong
get_timer (ulong base)
{
  return get_timer_masked () - base;
}

void
set_timer (ulong t)
{
  timestamp = t;
}

void
udelay (unsigned long usec)
{
  udelay_masked (usec);
}

void
reset_timer_masked (void)
{
  /* reset time */
  lastinc = READ_TIMER;
  timestamp = 0;
}

ulong
get_timer_masked (void)
{
  ulong now = READ_TIMER;

  if (now >= lastinc)
    {
      /* normal mode */
      timestamp += now - lastinc;
    }
  else
    {
      /* we have an overflow ... */
      timestamp += now + TIMER_LOAD_VAL - lastinc;
    }
  lastinc = now;

  return timestamp;
}

void
udelay_masked (unsigned long usec)
{
  ulong tmo;

  tmo = usec / 1000;
  tmo *= CFG_HZ;
  tmo /= 1000;

  reset_timer_masked ();

  while (get_timer_masked () < tmo)
     /*NOP*/;
}

/*
 * This function is derived from PowerPC code (read timebase as long long).
 * On ARM it just returns the timer value.
 */
unsigned long long
get_ticks (void)
{
  return get_timer (0);
}

/*
 * This function is derived from PowerPC code (timebase clock frequency).
 * On ARM it returns the number of timer ticks per second.
 */
ulong
get_tbclk (void)
{
  ulong tbclk;

  tbclk = CFG_HZ;
  return tbclk;
}
