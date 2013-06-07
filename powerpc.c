/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	PowerPC support code

Copyright (C) 2008, 2009	Sven Peter <svenpeter@gmail.com>
Copyright (C) 2009			Andre Heider "dhewg" <dhewg@wiibrew.org>
Copyright (C) 2010			Alex Marshall <trap15@raidenii.net>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"
#include "memory.h"
#include "powerpc.h"
#include "powerpc_elf.h"
#include "hollywood.h"
#include "utils.h"
#include "string.h"
#include "start.h"
#include "gecko.h"

void powerpc_upload_stub(u32 start, u32 entry)
{
	(void )start;
	//u32 i;

//	set32(HW_EXICTRL, EXICTRL_ENABLE_EXI);


/*	// lis r3, 1800@h
	write32(start + 4 * 0, 0x3c600000);
	// ori r3, r3, 0x1800@l
	write32(start + 4 * 1, 0x60631800);
	// lwz r4, r3
	write32(start + 4 * 2, 0x80830000);
	// addi r4, r4 1
	write32(start + 4 * 3, 0x38840001);
	// stw r4, r3
	write32(start + 4 * 4, 0x90830000);
	// b 0 ? (infinte loop)
	write32(start + 4 * 5, 0x48000000);
	// lis r3, entry@h
	write32(start + 4 * 6, 0x3c600000 | entry >> 16);
	// ori r3, r3, entry@l
	write32(start + 4 * 7, 0x60630000 | (entry & 0xffff));
	// mtsrr0 r3
	write32(start + 4 * 8, 0x7c7a03a6);
	// li r3, 0
	write32(start + 4 * 9, 0x38600000);
	// mtsrr1 r3
	write32(start + 4 * 10, 0x7c7b03a6);
	// rfi
	write32(start + 4 * 11, 0x4c000064);
*/

    write32(0x1800, 0x7c9f42a6); //mfpvr   r4
    write32(0x1804, 0x5484843e); //rlwinm  r4,r4,16,16,31
    write32(0x1808, 0x28047001); //cmplwi  r4,28673
    write32(0x180c, 0x408200b4); //bne-    0x18c0
    write32(0x1810, 0x7c79faa6); //mfl2cr  r3
    write32(0x1814, 0x5463003e); //rotlwi  r3,r3,0
    write32(0x1818, 0x7c79fba6); //mtl2cr  r3
    write32(0x181c, 0x7c0004ac); //sync    
    write32(0x1820, 0x7c79faa6); //mfl2cr  r3
    write32(0x1824, 0x64630020); //oris    r3,r3,32
    write32(0x1828, 0x7c79fba6); //mtl2cr  r3
    write32(0x182c, 0x7c79faa6); //mfl2cr  r3
    write32(0x1830, 0x546307fe); //clrlwi  r3,r3,31
    write32(0x1834, 0x2c030000); //cmpwi   r3,0
    write32(0x1838, 0x4082fff4); //bne+    0x182c
    write32(0x183c, 0x7c79faa6); //mfl2cr  r3
    write32(0x1840, 0x546302d2); //rlwinm  r3,r3,0,11,9
    write32(0x1844, 0x7c79fba6); //mtl2cr  r3
    write32(0x1848, 0x7c79faa6); //mfl2cr  r3
    write32(0x184c, 0x546307fe); //clrlwi  r3,r3,31
    write32(0x1850, 0x2c030000); //cmpwi   r3,0
    write32(0x1854, 0x4082fff4); //bne+    0x1848
    write32(0x1858, 0x48000004); //b       0x185c
    write32(0x185c, 0x7c70eaa6); //mfspr   r3,944
    write32(0x1860, 0x5463007e); //clrlwi  r3,r3,1
    write32(0x1864, 0x7c70eba6); //mtspr   944,r3
    write32(0x1868, 0x60000000); //nop
    write32(0x186c, 0x7c0004ac); //sync    
    write32(0x1870, 0x60000000); //nop
    write32(0x1874, 0x60000000); //nop
    write32(0x1878, 0x60000000); //nop
    write32(0x187c, 0x7c75eaa6); //mfspr   r3,949
    write32(0x1880, 0x64631000); //oris    r3,r3,4096
    write32(0x1884, 0x7c75eba6); //mtspr   949,r3
    write32(0x1888, 0x388000ff); //li      r4,255
    write32(0x188c, 0x3884ffff); //addi    r4,r4,-1
    write32(0x1890, 0x2c040000); //cmpwi   r4,0
    write32(0x1894, 0x4082fff8); //bne+    0x188c
    write32(0x1898, 0x60000000); //nop
    write32(0x189c, 0x3c600000); //lis     r3,0
    write32(0x18a0, 0x606318c0); //ori     r3,r3,6336
    write32(0x18a4, 0x5463007e); //clrlwi  r3,r3,1
    write32(0x18a8, 0x7c7a03a6); //mtsrr0  r3
    write32(0x18ac, 0x38800000); //li      r4,0
    write32(0x18b0, 0x7c9b03a6); //mtsrr1  r4
    write32(0x18b4, 0x4c000064); //rfi
    write32(0x18b8, 0x60000000); //nop
    write32(0x18bc, 0x60000000); //nop
    write32(0x18c0, 0x3c800011); //lis     r4,17
    write32(0x18c4, 0x60840c64); //ori     r4,r4,3172
    write32(0x18c8, 0x7c90fba6); //mtspr   1008,r4
    write32(0x18cc, 0x3c800000); //lis     r4,0
    write32(0x18d0, 0x60842000); //ori     r4,r4,8192
    write32(0x18d4, 0x7c800124); //mtmsr   r4
    write32(0x18d8, 0x7c93faa6); //mfspr   r4,1011
    write32(0x18dc, 0x64840200); //oris    r4,r4,512
    write32(0x18e0, 0x7c93fba6); //mtspr   1011,r4
    write32(0x18e4, 0x7c90faa6); //mfspr   r4,1008
    write32(0x18e8, 0x6084c000); //ori     r4,r4,49152
    write32(0x18ec, 0x7c90fba6); //mtspr   1008,r4
    write32(0x18f0, 0x4c00012c); //isync
    write32(0x18f4, 0x38800000); //li      r4,0
    write32(0x18f8, 0x7c9883a6); //mtdbatu 0,r4
    write32(0x18fc, 0x7c9983a6); //mtdbatl 0,r4
    write32(0x1900, 0x7c9a83a6); //mtdbatu 1,r4
    write32(0x1904, 0x7c9b83a6); //mtdbatl 1,r4
    write32(0x1908, 0x7c9c83a6); //mtdbatu 2,r4
    write32(0x190c, 0x7c9d83a6); //mtdbatl 2,r4
    write32(0x1910, 0x7c9e83a6); //mtdbatu 3,r4
    write32(0x1914, 0x7c9f83a6); //mtdbatl 3,r4
    write32(0x1918, 0x7c988ba6); //mtspr   568,r4
    write32(0x191c, 0x7c998ba6); //mtspr   569,r4
    write32(0x1920, 0x7c9a8ba6); //mtspr   570,r4
    write32(0x1924, 0x7c9b8ba6); //mtspr   571,r4
    write32(0x1928, 0x7c9c8ba6); //mtspr   572,r4
    write32(0x192c, 0x7c9d8ba6); //mtspr   573,r4
    write32(0x1930, 0x7c9e8ba6); //mtspr   574,r4
    write32(0x1934, 0x7c9f8ba6); //mtspr   575,r4
    write32(0x1938, 0x7c9083a6); //mtibatu 0,r4
    write32(0x193c, 0x7c9183a6); //mtibatl 0,r4
    write32(0x1940, 0x7c9283a6); //mtibatu 1,r4
    write32(0x1944, 0x7c9383a6); //mtibatl 1,r4
    write32(0x1948, 0x7c9483a6); //mtibatu 2,r4
    write32(0x194c, 0x7c9583a6); //mtibatl 2,r4
    write32(0x1950, 0x7c9683a6); //mtibatu 3,r4
    write32(0x1954, 0x7c9783a6); //mtibatl 3,r4
    write32(0x1958, 0x7c908ba6); //mtspr   560,r4
    write32(0x195c, 0x7c918ba6); //mtspr   561,r4
    write32(0x1960, 0x7c928ba6); //mtspr   562,r4
    write32(0x1964, 0x7c938ba6); //mtspr   563,r4
    write32(0x1968, 0x7c948ba6); //mtspr   564,r4
    write32(0x196c, 0x7c958ba6); //mtspr   565,r4
    write32(0x1970, 0x7c968ba6); //mtspr   566,r4
    write32(0x1974, 0x7c978ba6); //mtspr   567,r4
    write32(0x1978, 0x4c00012c); //isync
    write32(0x197c, 0x3c808000); //lis     r4,-32768
    write32(0x1980, 0x60840000); //ori     r4,r4,0
    write32(0x1984, 0x7c8001a4); //mtsr    0,r4
    write32(0x1988, 0x7c8101a4); //mtsr    1,r4
    write32(0x198c, 0x7c8201a4); //mtsr    2,r4
    write32(0x1990, 0x7c8301a4); //mtsr    3,r4
    write32(0x1994, 0x7c8401a4); //mtsr    4,r4
    write32(0x1998, 0x7c8501a4); //mtsr    5,r4
    write32(0x199c, 0x7c8601a4); //mtsr    6,r4
    write32(0x19a0, 0x7c8701a4); //mtsr    7,r4
    write32(0x19a4, 0x7c8801a4); //mtsr    8,r4
    write32(0x19a8, 0x7c8901a4); //mtsr    9,r4
    write32(0x19ac, 0x7c8a01a4); //mtsr    10,r4
    write32(0x19b0, 0x7c8b01a4); //mtsr    11,r4
    write32(0x19b4, 0x7c8c01a4); //mtsr    12,r4
    write32(0x19b8, 0x7c8d01a4); //mtsr    13,r4
    write32(0x19bc, 0x7c8e01a4); //mtsr    14,r4
    write32(0x19c0, 0x7c8f01a4); //mtsr    15,r4
    write32(0x19c4, 0x3c800000); //lis     r4,0
    write32(0x19c8, 0x60840002); //ori     r4,r4,2
    write32(0x19cc, 0x7c9183a6); //mtibatl 0,r4
    write32(0x19d0, 0x7c9983a6); //mtdbatl 0,r4
    write32(0x19d4, 0x3c608000); //lis     r3,-32768
    write32(0x19d8, 0x60631fff); //ori     r3,r3,8191
    write32(0x19dc, 0x7c7083a6); //mtibatu 0,r3
    write32(0x19e0, 0x7c7883a6); //mtdbatu 0,r3
    write32(0x19e4, 0x4c00012c); //isync
    write32(0x19e8, 0x3c800000); //lis     r4,0
    write32(0x19ec, 0x6084002a); //ori     r4,r4,42
    write32(0x19f0, 0x7c9b83a6); //mtdbatl 1,r4
    write32(0x19f4, 0x3c60c000); //lis     r3,-16384
    write32(0x19f8, 0x60631fff); //ori     r3,r3,8191
    write32(0x19fc, 0x7c7a83a6); //mtdbatu 1,r3
    write32(0x1a00, 0x4c00012c); //isync
    write32(0x1a04, 0x3c801000); //lis     r4,4096
    write32(0x1a08, 0x60840002); //ori     r4,r4,2
    write32(0x1a0c, 0x7c918ba6); //mtspr   561,r4
    write32(0x1a10, 0x7c998ba6); //mtspr   569,r4
    write32(0x1a14, 0x3c609000); //lis     r3,-28672
    write32(0x1a18, 0x60631fff); //ori     r3,r3,8191
    write32(0x1a1c, 0x7c708ba6); //mtspr   560,r3
    write32(0x1a20, 0x7c788ba6); //mtspr   568,r3
    write32(0x1a24, 0x4c00012c); //isync
    write32(0x1a28, 0x3c801000); //lis     r4,4096
    write32(0x1a2c, 0x6084002a); //ori     r4,r4,42
    write32(0x1a30, 0x7c9b8ba6); //mtspr   571,r4
    write32(0x1a34, 0x3c60d000); //lis     r3,-12288
    write32(0x1a38, 0x60631fff); //ori     r3,r3,8191
    write32(0x1a3c, 0x7c7a8ba6); //mtspr   570,r3
    write32(0x1a40, 0x4c00012c); //isync
    write32(0x1a44, 0x38600000); //li      r3,0
    write32(0x1a48, 0x38800000); //li      r4,0
    write32(0x1a4c, 0x908300f4); //stw     r4,244(r3)
    write32(0x1a50, 0x3c600000 | entry >> 16 ); //lis     r3,entry@h
    write32(0x1a54, 0x60630000 | (entry & 0xffff) ); //ori     r3,r3,entry@l
    write32(0x1a58, 0x7c7a03a6); //mtsrr0  r3
    write32(0x1a5c, 0x7c8000a6); //mfmsr   r4
    write32(0x1a60, 0x60840030); //ori     r4,r4,48
    write32(0x1a64, 0x7c9b03a6); //mtsrr1  r4
    write32(0x1a68, 0x4c000064); //rfi

	//for (i = 6; i < 0x10; ++i)
		//write32(EXI_BOOT_BASE + 4 * i, 0);

//	set32(HW_DIFLAGS, DIFLAGS_BOOT_CODE);
	set32(HW_AHBPROT, 0xFFFFFFFF);

//	gecko_printf("disabling EXI now...\n");
//	clear32(HW_EXICTRL, EXICTRL_ENABLE_EXI);
}

void powerpc_hang(void)
{
	gecko_printf("Hanging PPC. End debug output.");
	gecko_enable(0);
	clear32(HW_RESETS, 0x30);
	udelay(100);
	set32(HW_RESETS, 0x20);
	udelay(100);
}

void powerpc_reset(void)
{
	// enable the broadway IPC interrupt
	gecko_printf("Resetting PPC. End debug output.");
	gecko_enable(0);
	write32(HW_PPCIRQMASK, (1<<30));
	clear32(HW_RESETS, 0x30);
	udelay(100);
	set32(HW_RESETS, 0x20);
	udelay(100);
	set32(HW_RESETS, 0x10);
	udelay(100000);
	set32(HW_EXICTRL, EXICTRL_ENABLE_EXI);
}

void powerpc_ipc(volatile ipc_request *req)
{
	switch (req->req) {
	case IPC_PPC_BOOT:
		if (req->args[0]) {
			// Enqueued from ARM side, do not invalidate mem nor ipc_post
			powerpc_boot_mem((u8 *) req->args[1], req->args[2]);
		} else {
			dc_invalidaterange((void *) req->args[1], req->args[2]);
			int res = powerpc_boot_mem((u8 *) req->args[1], req->args[2]);
			if (res)
				ipc_post(req->code, req->tag, 1, res);
		}

		break;

	case IPC_PPC_BOOT_FILE:
		if (req->args[0]) {
			// Enqueued from ARM side, do not invalidate mem nor ipc_post
			powerpc_boot_file((char *) req->args[1]);
		} else {
			dc_invalidaterange((void *) req->args[1],
								strnlen((char *) req->args[1], 256));
			int res = powerpc_boot_file((char *) req->args[1]);
			if (res)
				ipc_post(req->code, req->tag, 1, res);
		}

		break;

	default:
		gecko_printf("IPC: unknown SLOW PPC request %04X\n", req->req);
	}
}

