/*
 * (C) Copyright 2002-2006
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
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

#if (CONFIG_COMMANDS & CFG_CMD_NAND)
void nand_init (void);
#endif

ulong monitor_flash_len;

#ifdef CONFIG_HAS_DATAFLASH
extern int  AT91F_DataflashInit(void);
extern void dataflash_print_info(void);
#endif

const char version_string[] =
	U_BOOT_VERSION" (" __DATE__ " - " __TIME__ ")"CONFIG_IDENT_STRING;


/*
 * Begin and End of memory area for malloc(), and current "brk"
 */
static ulong mem_malloc_start = 0;
static ulong mem_malloc_end = 0;
static ulong mem_malloc_brk = 0;

static
void mem_malloc_init (ulong dest_addr)
{
	mem_malloc_start = dest_addr;
	mem_malloc_end = dest_addr + CFG_MALLOC_LEN;
	mem_malloc_brk = mem_malloc_start;

	memset ((void *) mem_malloc_start, 0,
			mem_malloc_end - mem_malloc_start);
}

void *sbrk (ptrdiff_t increment)
{
	ulong old = mem_malloc_brk;
	ulong new = old + increment;

	if ((new < mem_malloc_start) || (new > mem_malloc_end)) {
		return (NULL);
	}
	mem_malloc_brk = new;

	return ((void *) old);
}

/************************************************************************
 * Init Utilities							*
 ************************************************************************
 * Some of this code should be moved into the core functions,
 * or dropped completely,
 * but let's get it working (again) first...
 */

static int init_baudrate (void)
{
	char tmp[64];	/* long enough for environment variables */
	int i = getenv_r ("baudrate", tmp, sizeof (tmp));
	gd->bd->bi_baudrate = gd->baudrate = (i > 0)
			? (int) simple_strtoul (tmp, NULL, 10)
			: CONFIG_BAUDRATE;

	return (0);
}


/*
 * Breathe some life into the board...
 *
 * Initialize a serial port as console, and carry out some hardware
 * tests.
 *
 * The first part of initialization is running from Flash memory;
 * its main purpose is to initialize the RAM so that we
 * can relocate the monitor code to RAM.
 */

/*
 * All attempts to come up with a "common" initialization sequence
 * that works for all boards and architectures failed: some of the
 * requirements are just _too_ different. To get rid of the resulting
 * mess of board dependent #ifdef'ed code we now make the whole
 * initialization sequence configurable to the user.
 *
 * The requirements for any new initalization function is simple: it
 * receives a pointer to the "global data" structure as it's only
 * argument, and returns an integer return code, where 0 means
 * "continue" and != 0 means "fatal error, hang the system".
 */
typedef int (init_fnc_t) (void);

init_fnc_t *init_sequence[] = {
	cpu_init,		/* basic cpu dependent setup */
	board_init,		/* basic board dependent setup */
	interrupt_init,		/* set up exceptions */
	env_init,		/* initialize environment */
	init_baudrate,		/* initialze baudrate settings */
	serial_init,		/* serial communications setup */
	console_init_f,		/* stage 1 init of console */
	display_banner,		/* say that we are here */
#if defined(CONFIG_DISPLAY_CPUINFO)
	print_cpuinfo,		/* display cpu info (and speed) */
#endif
#if defined(CONFIG_DISPLAY_BOARDINFO)
	checkboard,		/* display board info */
#endif
	dram_init,		/* configure available RAM banks */
	display_dram_config,
	NULL,
};

void start_armboot (void)
{
	init_fnc_t **init_fnc_ptr;
	char *s;
#ifndef CFG_NO_FLASH
	ulong size;
#endif
#if defined(CONFIG_VFD) || defined(CONFIG_LCD)
	unsigned long addr;
#endif

	/* Pointer is writable since we allocated a register for it */
	gd = (gd_t*)(_armboot_start - CFG_MALLOC_LEN - sizeof(gd_t));
	/* compiler optimization barrier needed for GCC >= 3.4 */
	__asm__ __volatile__("": : :"memory");

	memset ((void*)gd, 0, sizeof (gd_t));
	gd->bd = (bd_t*)((char*)gd - sizeof(bd_t));
	memset (gd->bd, 0, sizeof (bd_t));

	monitor_flash_len = _bss_start - _armboot_start;

	for (init_fnc_ptr = init_sequence; *init_fnc_ptr; ++init_fnc_ptr) {
		if ((*init_fnc_ptr)() != 0) {
			hang ();
		}
	}

#ifndef CFG_NO_FLASH
	/* configure available FLASH banks */
	size = flash_init ();
	display_flash_config (size);
#endif /* CFG_NO_FLASH */

#ifdef CONFIG_VFD
#	ifndef PAGE_SIZE
#	  define PAGE_SIZE 4096
#	endif
	/*
	 * reserve memory for VFD display (always full pages)
	 */
	/* bss_end is defined in the board-specific linker script */
	addr = (_bss_end + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);
	size = vfd_setmem (addr);
	gd->fb_base = addr;
#endif /* CONFIG_VFD */

	/* armboot_start is defined in the board-specific linker script */
	mem_malloc_init (_armboot_start - CFG_MALLOC_LEN);

#if (CONFIG_COMMANDS & CFG_CMD_NAND)
	puts ("NAND:  ");
	nand_init();		/* go init the NAND */
#endif

#ifdef CONFIG_HAS_DATAFLASH
	AT91F_DataflashInit();
	dataflash_print_info();
#endif

	/* initialize environment */
	env_relocate ();

#ifdef CONFIG_VFD
	/* must do this after the framebuffer is allocated */
	drv_vfd_init();
#endif /* CONFIG_VFD */

	devices_init ();	/* get the devices list going. */

#ifdef CONFIG_CMC_PU2
	load_sernum_ethaddr ();
#endif /* CONFIG_CMC_PU2 */

	jumptable_init ();

	console_init_r ();	/* fully init console as a device */

#if defined(CONFIG_MISC_INIT_R)
	/* miscellaneous platform dependent initialisations */
	misc_init_r ();
#endif

	/* enable exceptions */
	enable_interrupts ();

#ifdef BOARD_LATE_INIT
	board_late_init ();
#endif

	/* main_loop() can return to retry autoboot, if so just run it again. */
	for (;;) {
	}

	/* NOTREACHED - no way out of command loop except booting */
}

void hang (void)
{
	puts ("### ERROR ### Please RESET the board ###\n");
	for (;;);
}


