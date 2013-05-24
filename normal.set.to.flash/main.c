/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.

Copyright (C) 2008, 2009	Haxx Enterprises <bushing@gmail.com>
Copyright (C) 2008, 2009	Sven Peter <svenpeter@gmail.com>
Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>
Copyright (C) 2009			Andre Heider "dhewg" <dhewg@wiibrew.org>
Copyright (C) 2009		John Kelley <wiidev@kelley.ca>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/
#include "gpio.h"
#include "types.h"
#include "utils.h"
#include "start.h"
#include "hollywood.h"
#include "sdhc.h"
#include "string.h"
#include "memory.h"
#include "gecko.h"
#include "ff.h"
#include "panic.h"
#include "powerpc_elf.h"
#include "irq.h"
#include "ipc.h"
#include "exception.h"
#include "crypto.h"
#include "nand.h"
#include "boot2.h"
#include "git_version.h"

#define PPC_BOOT_FILE "/bootmii/ppcboot.elf"

FATFS fatfs;

void flashSensor()
{	set32(HW_GPIO1OUT, GP_SENSORBAR);
	udelay(200000);
	clear32(HW_GPIO1OUT, GP_SENSORBAR);
	udelay(200000);
}

u32 _main(void *base)
{
	FRESULT fres;
	int res;
	u32 vector;
	(void)base;
	
	flashSensor();
	
	gecko_init();
	gecko_printf("mini %s loading\n", git_version);
	
	flashSensor();
	
	gecko_printf("Initializing exceptions...\n");
	exception_initialize();
	
	flashSensor();
	
	gecko_printf("Configuring caches and MMU...\n");
	mem_initialize();
	
	flashSensor();
	
	gecko_printf("IOSflags: %08x %08x %08x\n",
		read32(0xffffff00), read32(0xffffff04), read32(0xffffff08));
	gecko_printf("          %08x %08x %08x\n",
		read32(0xffffff0c), read32(0xffffff10), read32(0xffffff14));

	irq_initialize();
//	irq_enable(IRQ_GPIO1B);
	irq_enable(IRQ_GPIO1);
	irq_enable(IRQ_RESET);
	irq_enable(IRQ_TIMER);
	irq_set_alarm(20, 1);
	gecko_printf("Interrupts initialized\n");
	
	flashSensor();
	
	crypto_initialize();
	gecko_printf("crypto support initialized\n");

	flashSensor();
	
	nand_initialize();
	gecko_printf("NAND initialized.\n");
	
	flashSensor();

	boot2_init();
	
	flashSensor();
	
	gecko_printf("Initializing IPC...\n");
	ipc_initialize();
	
	flashSensor();
	
	gecko_printf("Initializing SDHC...\n");
	sdhc_init();

	flashSensor();
	
	gecko_printf("Mounting SD...\n");
	fres = f_mount(0, &fatfs);

	flashSensor();
	
	if (read32(0x0d800190) & 2) {
		gecko_printf("GameCube compatibility mode detected...\n");
		vector = boot2_run(1, 0x101);
		goto shutdown;
	}

	if(fres != FR_OK) {
		gecko_printf("Error %d while trying to mount SD\n", fres);
		panic2(0, PANIC_MOUNT);
	}

	gecko_printf("Trying to boot:" PPC_BOOT_FILE "\n");

	res = powerpc_boot_file(PPC_BOOT_FILE);
	if(res < 0) {
		gecko_printf("Failed to boot PPC: %d\n", res);
		gecko_printf("Hopefully performing system reset\n");
		clear32(HW_RESETS, 0x30);
		udelay(100);
		set32(HW_RESETS, 0x20);
		udelay(100);
		set32(HW_RESETS, 0x10);
		gecko_printf("Booting System Menu\n");
		vector = boot2_run(1, 2);
		goto shutdown;
	}

	flashSensor();
	
	gecko_printf("Going into IPC mainloop...\n");
	vector = ipc_process_slow();
	
	flashSensor();
	
	gecko_printf("IPC mainloop done!\n");
	gecko_printf("Shutting down IPC...\n");
	ipc_shutdown();
	
	flashSensor();
	
shutdown:
	gecko_printf("Shutting down interrupts...\n");
	irq_shutdown();
	
	flashSensor();
	
	gecko_printf("Shutting down caches and MMU...\n");
	mem_shutdown();

	flashSensor();
	
	gecko_printf("Vectoring to 0x%08x...\n", vector);
	return vector;
}
