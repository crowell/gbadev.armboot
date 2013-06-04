/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	PowerPC ELF file loading

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>
Copyright (C) 2009			Andre Heider "dhewg" <dhewg@wiibrew.org>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"
#include "powerpc.h"
#include "hollywood.h"
#include "utils.h"
#include "start.h"
#include "gecko.h"
#include "ff.h"
#include "powerpc_elf.h"
#include "elf.h"
#include "memory.h"
#include "string.h"

//obcd
//missing
typedef signed int bool;
#define false 0
#define true 1

extern u8 __mem2_area_start[];

#define PPC_MEM1_END	(0x017fffff)
#define PPC_MEM2_START	(0x10000000)
#define PPC_MEM2_END	((u32) __mem2_area_start)

#define PHDR_MAX 10

static int _check_physaddr(u32 addr) {
	if ((addr >= PPC_MEM2_START) && (addr <= PPC_MEM2_END))
		return 2;

	if (addr < PPC_MEM1_END)
		return 1;

	return -1;
}

static int _check_physrange(u32 addr, u32 len) {
	switch (_check_physaddr(addr)) {
	case 1:
		if ((addr + len) < PPC_MEM1_END)
			return 1;
		break;
	case 2:
		if ((addr + len) < PPC_MEM2_END)
			return 2;
		break;
	}

	return -1;
}

static Elf32_Ehdr elfhdr;
static Elf32_Phdr phdrs[PHDR_MAX];
//obcd
//parentheses to get rid of warnings
u32 virtualToPhysical(u32 virtualAddress)
{	if(virtualAddress & 0x80000000)
		return (virtualAddress & ~0x80000000);
	if(virtualAddress & 0xC0000000)
		return (virtualAddress & ~0xC0000000);
	if(virtualAddress & 0x90000000)
		return ((virtualAddress & ~0x90000000) | 0x10000000);
	if(virtualAddress & 0xD0000000)
		return ((virtualAddress & ~0xD0000000) | 0x10000000);
	return virtualAddress;
}

u32 makeRelativeBranch(u32 currAddr, u32 destAddr, bool linked)
{
	u32 ret = 0x48000000 | (( destAddr - currAddr ) & 0x3FFFFFC );
	if(linked)
		ret |= 1;
	return ret;
}

u32 makeAbsoluteBranch(u32 destAddr, bool linked)
{
    u32 ret = 0x48000002 | ( destAddr & 0x3FFFFFC );
    if(linked)
        ret |= 1;
    return ret;
}

inline int powerpc_load_dol(const char *path, u32 *endAddress)
{	int c;
	u32 read, BSSAddress, BSSSize, entryPoint, fileCounter = 0;
	FIL fd;
	FRESULT fres;
	u32 textFileOffsets[7],
	dataFileOffsets[11],
	textLoadingAddresses[7],
	dataLoadingAddresses[11],
	textSizes[7],
	dataSizes[11];

	fres = f_open(&fd, path, FA_READ);
	if (fres != FR_OK)
		return -fres;

	fres = f_read(&fd, (void *)&textFileOffsets, 7*4, &read);
	if (fres != FR_OK)
		return -fres;
	fileCounter+=7*4;

	fres = f_read(&fd, (void *)&dataFileOffsets, 11*4, &read);
	if (fres != FR_OK)
		return -fres;
	fileCounter+=11*4;

	fres = f_read(&fd, (void *)&textLoadingAddresses, 7*4, &read);
	if (fres != FR_OK)
		return -fres;
	fileCounter+=7*4;
	for(c=0;c<7;c++)
		textLoadingAddresses[c] = virtualToPhysical(textLoadingAddresses[c]);

	fres = f_read(&fd, (void *)&dataLoadingAddresses, 11*4, &read);
	if (fres != FR_OK)
		return -fres;
	fileCounter+=11*4;
	for(c=0;c<7;c++)
		dataLoadingAddresses[c] = virtualToPhysical(dataLoadingAddresses[c]);

	fres = f_read(&fd, (void *)&textSizes, 7*4, &read);
	if (fres != FR_OK)
		return -fres;
	fileCounter+=7*4;

	fres = f_read(&fd, (void *)&dataSizes, 11*4, &read);
	if (fres != FR_OK)
		return -fres;
	fileCounter+=11*4;

	fres = f_read(&fd, (void *)&BSSAddress, 4, &read);
	if (fres != FR_OK)
		return -fres;
	fileCounter+=4;
	fres = f_read(&fd, (void *)&BSSSize, 4, &read);
	if (fres != FR_OK)
		return -fres;
	fileCounter+=4;
	fres = f_read(&fd, (void *)&entryPoint, 4, &read);
	if (fres != FR_OK)
		return -fres;
	fileCounter+=4;
	
	// I know for this case there's only 1 text and 1 data area so I'm cheating here.
	
	fres = f_lseek(&fd, textFileOffsets[0]);
	if (fres != FR_OK)
		return -fres;
	fileCounter = textFileOffsets[0];
	
	fres = f_read(&fd, (void *)textLoadingAddresses[0], textSizes[0], &read);
	if (fres != FR_OK)
		return -fres;
	fileCounter+=textSizes[0];
	
	// 
	
	fres = f_lseek(&fd, dataFileOffsets[0]);
	if (fres != FR_OK)
		return -fres;
	fileCounter = dataFileOffsets[0];
	
	fres = f_read(&fd, (void *)dataLoadingAddresses[0], dataSizes[0], &read);
	if (fres != FR_OK)
		return -fres;
	fileCounter+=dataSizes[0];
	
	/////////////////////////////////

	//dc_flushall();
	//f_close(&fd);
	gecko_printf("DOL load done");
	
	if(endAddress)
		*endAddress = dataLoadingAddresses[0] + dataSizes[0] - 1;
	
	return 0;
}

int powerpc_boot_file(const char *path)
{
	// do race attack here
	u32 decryptionEndAddress;
	powerpc_hang();
	udelay(300000);
	sensorbarOn();
	udelay(300000);
	powerpc_load_dol("/bootmii/00000003.app", &decryptionEndAddress);
	sensorbarOff();
	udelay(300000);
	
	u32 oldValue = read32(0x1330120);
	sensorbarOn();
	udelay(300000);
	//oldValue2 = read32(decryptionEndAddress);
	u32 Core0JumpInstruction = makeAbsoluteBranch(0x100, false);
	// We'll trap PPC here with an infinite loop until we're done loading other stuff
	sensorbarOff();
	udelay(300000);
	
	write32(0x100, makeAbsoluteBranch(0x104, false));
	write32(0x104, makeAbsoluteBranch(0x100, false));
	
	// lis r3, entry@h
	write32(0x104 + 4 * 1, 0x3c600000 | elfhdr.e_entry >> 16);
	// ori r3, r3, entry@l
	write32(0x104 + 4 * 2, 0x60630000 | (elfhdr.e_entry & 0xffff));
	// mtsrr0 r3
	write32(0x104 + 4 * 3, 0x7c7a03a6);
	// li r3, 0
	write32(0x104 + 4 * 4, 0x38600000);
	// mtsrr1 r3
	write32(0x104 + 4 * 5, 0x7c7b03a6);
	// rfi
	write32(0x104 + 4 * 6, 0x4c000064);
	
	sensorbarOn();
	udelay(300000);
	dc_flushall();//range((void*)0x100,64);

	sensorbarOff();
	udelay(300000);
	//powerpc_reset();
	gecko_printf("Resetting PPC. End debug output.");
	gecko_enable(0);
	clear32(HW_RESETS, 0x30);
	udelay(100);
	set32(HW_RESETS, 0x20);
	udelay(100);
	set32(HW_RESETS, 0x10);
	
	do
	{	dc_invalidaterange((void*)0x1330100,64);
		ahb_flush_from(AHB_1);
	}while(oldValue == read32(0x1330120));
	
	// where core 0 will end up once the ROM is done decrypting 1-200
	powerpc_upload_stub(0x1330100, elfhdr.e_entry);
	dc_flushrange((void*)0x1330100,32);

	sensorbarOn();
	oldValue = read32(0x1330100);

	// wait for decryption / validation to finish and PPC to flag that we have control.
	do
	{	dc_invalidaterange((void*)0x1330100,32);
		ahb_flush_from(AHB_1);
	}while(oldValue== read32(0x1330100));
//	udelay(2000000);
	udelay(300000);
	sensorbarOff();
	udelay(300000);
/*	sensorbarOn();
	udelay(300000);
*/
	u32 read;
	FIL fd;
	FRESULT fres;

	fres = f_open(&fd, path, FA_READ);
	if (fres != FR_OK)
		return -fres;

	fres = f_read(&fd, &elfhdr, sizeof(elfhdr), &read);
	
	if (fres != FR_OK)
		return -fres;

	if (read != sizeof(elfhdr))
		return -100;

	if (memcmp("\x7F" "ELF\x01\x02\x01\x00\x00",elfhdr.e_ident,9)) {
		gecko_printf("Invalid ELF header! 0x%02x 0x%02x 0x%02x 0x%02x\n",elfhdr.e_ident[0], elfhdr.e_ident[1], elfhdr.e_ident[2], elfhdr.e_ident[3]);
		return -101;
	}

	if (_check_physaddr(elfhdr.e_entry) < 0) {
		gecko_printf("Invalid entry point! 0x%08x\n", elfhdr.e_entry);
		return -102;
	}

	if (elfhdr.e_phoff == 0 || elfhdr.e_phnum == 0) {
		gecko_printf("ELF has no program headers!\n");
		return -103;
	}

	if (elfhdr.e_phnum > PHDR_MAX) {
		gecko_printf("ELF has too many (%d) program headers!\n", elfhdr.e_phnum);
		return -104;
	}

	fres = f_lseek(&fd, elfhdr.e_phoff);
	if (fres != FR_OK)
		return -fres;

	fres = f_read(&fd, phdrs, sizeof(phdrs[0])*elfhdr.e_phnum, &read);
	if (fres != FR_OK)
		return -fres;

	if (read != sizeof(phdrs[0])*elfhdr.e_phnum)
		return -105;

	u16 count = elfhdr.e_phnum;
	Elf32_Phdr *phdr = phdrs;

	while (count--) {
		if (phdr->p_type != PT_LOAD) {
			gecko_printf("Skipping PHDR of type %d\n", phdr->p_type);
		} else {
			if (_check_physrange(phdr->p_paddr, phdr->p_memsz) < 0) {
				gecko_printf("PHDR out of bounds [0x%08x...0x%08x]\n",
								phdr->p_paddr, phdr->p_paddr + phdr->p_memsz);
				return -106;
			}

			void *dst = (void *) phdr->p_paddr;

			gecko_printf("LOAD 0x%x @0x%08x [0x%x]\n", phdr->p_offset, phdr->p_paddr, phdr->p_filesz);
			fres = f_lseek(&fd, phdr->p_offset);
			if (fres != FR_OK)
				return -fres;
			fres = f_read(&fd, dst, phdr->p_filesz, &read);
			if (fres != FR_OK)
				return -fres;
			if (read != phdr->p_filesz)
				return -107;
		}
		phdr++;
	}

	dc_flushall();

	gecko_printf("ELF load done, booting PPC...\n");

	//udelay(300000);
	sensorbarOn();
	udelay(300000);

	write32(0x100, makeAbsoluteBranch(0x108, false));
	dc_flushrange((void*)0x100,32);
	gecko_printf("PPC booted!\n");
	sensorbarOff();

	return 0;
}

int powerpc_boot_mem(const u8 *addr, u32 len)
{
	if (len < sizeof(Elf32_Ehdr))
		return -100;

	Elf32_Ehdr *ehdr = (Elf32_Ehdr *) addr;

	if (memcmp("\x7F" "ELF\x01\x02\x01\x00\x00", ehdr->e_ident, 9)) {
		gecko_printf("Invalid ELF header! 0x%02x 0x%02x 0x%02x 0x%02x\n",
						ehdr->e_ident[0], ehdr->e_ident[1],
						ehdr->e_ident[2], ehdr->e_ident[3]);
		return -101;
	}

	if (_check_physaddr(ehdr->e_entry) < 0) {
		gecko_printf("Invalid entry point! 0x%08x\n", ehdr->e_entry);
		return -102;
	}

	if (ehdr->e_phoff == 0 || ehdr->e_phnum == 0) {
		gecko_printf("ELF has no program headers!\n");
		return -103;
	}

	if (ehdr->e_phnum > PHDR_MAX) {
		gecko_printf("ELF has too many (%d) program headers!\n",
						ehdr->e_phnum);
		return -104;
	}

	u16 count = ehdr->e_phnum;
	if (len < ehdr->e_phoff + count * sizeof(Elf32_Phdr))
		return -105;

	Elf32_Phdr *phdr = (Elf32_Phdr *) &addr[ehdr->e_phoff];

	// TODO: add more checks here
	// - loaded ELF overwrites itself?

	powerpc_hang();

	while (count--) {
		if (phdr->p_type != PT_LOAD) {
			gecko_printf("Skipping PHDR of type %d\n", phdr->p_type);
		} else {
			if (_check_physrange(phdr->p_paddr, phdr->p_memsz) < 0) {
				gecko_printf("PHDR out of bounds [0x%08x...0x%08x]\n",
								phdr->p_paddr, phdr->p_paddr + phdr->p_memsz);
				return -106;
			}

			gecko_printf("LOAD 0x%x @0x%08x [0x%x]\n", phdr->p_offset, phdr->p_paddr, phdr->p_filesz);
			memcpy((void *) phdr->p_paddr, &addr[phdr->p_offset],
					phdr->p_filesz);
		}
		phdr++;
	}

	dc_flushall();

	gecko_printf("ELF load done, booting PPC...\n");
	powerpc_upload_stub(0x104, ehdr->e_entry);
	powerpc_reset();
	gecko_printf("PPC booted!\n");

	return 0;
}

