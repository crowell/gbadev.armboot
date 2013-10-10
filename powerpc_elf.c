/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	PowerPC ELF file loading

Copyright (C) 2008, 2009        Hector Martin "marcan" <marcan@marcansoft.com>
Copyright (C) 2009                      Andre Heider "dhewg" <dhewg@wiibrew.org>

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
#include "stubsb1.h"

extern u8 __mem2_area_start[];

#define PPC_MEM1_END    (0x017fffff)
#define PPC_MEM2_START  (0x10000000)
#define PPC_MEM2_END    ((u32) __mem2_area_start)

#define PHDR_MAX 10

typedef struct dol_t dol_t;
struct dol_t
{
	u32 offsetText[7];
	u32 offsetData[11];
	u32 addressText[7];
	u32 addressData[11];
	u32 sizeText[7];
	u32 sizeData[11];
	u32 addressBSS;
	u32 sizeBSS;
	u32 entrypt;
	u8 pad[0x1C];
};

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

u32 virtualToPhysical(u32 virtualAddress)
{
	if ((virtualAddress & 0xC0000000) == 0xC0000000) return virtualAddress & ~0xC0000000;
	if ((virtualAddress & 0x80000000) == 0x80000000) return virtualAddress & ~0x80000000;
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


int powerpc_load_dol(const char *path, u32 *entry)
{
	u32 read;
	FIL fd;
	FRESULT fres;
	dol_t dol_hdr;
	gecko_printf("Loading DOL file: %s .\n", path);
	fres = f_open(&fd, path, FA_READ);
	if (fres != FR_OK)
		return -fres;

	fres = f_read(&fd, &dol_hdr, sizeof(dol_t), &read);
	if (fres != FR_OK)
		return -fres;

	u32 end = 0;
	int ii;

	/* TEXT SECTIONS */
	for (ii = 0; ii < 7; ii++)
	{
		if (!dol_hdr.sizeText[ii])
			continue;
		fres = f_lseek(&fd, dol_hdr.offsetText[ii]);
		if (fres != FR_OK)
			return -fres;
		u32 phys = virtualToPhysical(dol_hdr.addressText[ii]);
		fres = f_read(&fd, (void*)phys, dol_hdr.sizeText[ii], &read);
		if (fres != FR_OK)
			return -fres;
		if (phys + dol_hdr.sizeText[ii] > end)
			end = phys + dol_hdr.sizeText[ii];
		gecko_printf("Text section of size %08x loaded from offset %08x to memory %08x.\n", dol_hdr.sizeText[ii], dol_hdr.offsetText[ii], phys);
		gecko_printf("Memory area starts with %08x and ends with %08x (at address %08x)\n", read32(phys), read32(phys+(dol_hdr.sizeText[ii] - 1) & ~3),(phys+(dol_hdr.sizeText[ii] - 1)) & ~3);
	}

	/* DATA SECTIONS */
	for (ii = 0; ii < 11; ii++)
	{
		if (!dol_hdr.sizeData[ii])
			continue;
		fres = f_lseek(&fd, dol_hdr.offsetData[ii]);
		if (fres != FR_OK)
			return -fres;
		u32 phys = virtualToPhysical(dol_hdr.addressData[ii]);
		fres = f_read(&fd, (void*)phys, dol_hdr.sizeData[ii], &read);
		if (fres != FR_OK)
			return -fres;
		if (phys + dol_hdr.sizeData[ii] > end)
			end = phys + dol_hdr.sizeData[ii];
		gecko_printf("Data section of size %08x loaded from offset %08x to memory %08x.\n", dol_hdr.sizeData[ii], dol_hdr.offsetData[ii], phys);
		gecko_printf("Memory area starts with %08x and ends with %08x (at address %08x)\n", read32(phys), read32(phys+(dol_hdr.sizeData[ii] - 1) & ~3),(phys+(dol_hdr.sizeData[ii] - 1)) & ~3);
	}
  *entry = dol_hdr.entrypt;
	return 0;
}

int powerpc_load_elf(char* path)
{
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
	//powerpc_hang();
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

	gecko_printf("ELF load done. Entry point: %08x\n", elfhdr.e_entry);
	//*entry = elfhdr.e_entry;
	return 0;
}


int powerpc_boot_file(const char *path)
{
	int fres = 0;
	FIL fd;
	u32 decryptionEndAddress, entry;
	
	fres = powerpc_load_dol("/bootmii/00000017.app", &entry);
	gecko_printf("powerpc_load_dol returned %d .\n", fres);
	if(fres) return fres;
	decryptionEndAddress = ( 0x1330100 + read32(0x133008c + read32(0x1330008) ) -1 ) & ~3;
	gecko_printf("0xd8005A0 register value is %08x.\n", read32(0xd8005A0));
	if((read32(0xd8005A0) & 0xFFFF0000) != 0xCAFE0000)
	{	gecko_printf("Not a Wii U. Aborting\n");
		return -1;
	}gecko_printf("Running Wii U code.\n");
	dc_flushall();
	u32 oldValue = read32(0x1330100);
	u32 oldValue2 = read32(decryptionEndAddress);

	gecko_printf("Resetting PPC. End on-screen debug output.\n\n");
	gecko_enable(0);

	//reboot ppc side
	clear32(HW_RESETS, 0x30);
	udelay(100);
	set32(HW_RESETS, 0x20);
	udelay(100);
	set32(HW_RESETS, 0x10);

	// do race attack here
	do dc_invalidaterange((void*)0x1330100,32);
	while(oldValue == read32(0x1330100));
	oldValue = read32(0x1330100);
	write32(0x1330100, 0x48000000); // infinite loop
	dc_flushrange((void*)0x1330100,32);
	sensorbarOn();
	// wait for decryption / validation to finish
	do dc_invalidaterange((void*)decryptionEndAddress,32);
	while(oldValue2 == read32(decryptionEndAddress));
	sensorbarOff();
	//dump decrypted memory area
	u32 writeLength;
	f_open(&fd, "/bootmii/dump.bin", FA_CREATE_ALWAYS|FA_WRITE);
	f_write(&fd, &oldValue, 4, &writeLength);
	f_write(&fd, (void*)0x1330104, decryptionEndAddress-0x1330100, &writeLength);
	f_close(&fd);
	systemReset();
	return fres;
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
	//powerpc_upload_oldstub(ehdr->e_entry);
	powerpc_reset();
	gecko_printf("PPC booted!\n");

	return 0;
}
