/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	low-level NAND support

Copyright (C) 2008, 2009	Haxx Enterprises <bushing@gmail.com>
Copyright (C) 2008, 2009	Sven Peter <svenpeter@gmail.com>
Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "hollywood.h"
#include "nand.h"
#include "utils.h"
#include "string.h"
#include "start.h"
#include "memory.h"
#include "crypto.h"
#include "irq.h"
#include "ipc.h"
#include "gecko.h"
#include "types.h"
#include "seeprom.h"
#include "vsprintf.h"
#include "sdhc.h"

// #define	NAND_DEBUG	1
#define NAND_SUPPORT_WRITE 1
#define NAND_SUPPORT_ERASE 1

#ifdef NAND_DEBUG
#	include "gecko.h"
#	define	NAND_debug(f, arg...) gecko_printf("NAND: " f, ##arg);
#else
#	define	NAND_debug(f, arg...)
#endif

#define NAND_RESET      0xff
#define NAND_CHIPID     0x90
#define NAND_GETSTATUS  0x70
#define NAND_ERASE_PRE  0x60
#define NAND_ERASE_POST 0xd0
#define NAND_READ_PRE   0x00
#define NAND_READ_POST  0x30
#define NAND_WRITE_PRE  0x80
#define NAND_WRITE_POST 0x10

#define NAND_BUSY_MASK  0x80000000
#define NAND_ERROR      0x20000000

#define NAND_FLAGS_IRQ 	0x40000000
#define NAND_FLAGS_WAIT 0x8000
#define NAND_FLAGS_WR	0x4000
#define NAND_FLAGS_RD	0x2000
#define NAND_FLAGS_ECC	0x1000

static ipc_request current_request;

static u8 ipc_data[PAGE_SIZE] MEM2_BSS ALIGNED(32);
static u8 ipc_ecc[ECC_BUFFER_ALLOC] MEM2_BSS ALIGNED(128); //128 alignment REQUIRED

static volatile int irq_flag;
static u32 last_page_read = 0;
static u32 nand_min_page = 0x200; // default to protecting boot1+boot2

void nand_irq(void)
{
	int code, tag, err = 0;
	if(read32(NAND_CMD) & NAND_ERROR) {
		gecko_printf("NAND: Error on IRQ\n");
		err = -1;
	}
	ahb_flush_from(AHB_NAND);
	ahb_flush_to(AHB_STARLET);
	if (current_request.code != 0) {
		switch (current_request.req) {
			case IPC_NAND_GETID:
				memcpy32((void*)current_request.args[0], ipc_data, 0x40);
				dc_flushrange((void*)current_request.args[0], 0x40);
				break;
			case IPC_NAND_STATUS:
				memcpy32((void*)current_request.args[0], ipc_data, 0x40);
				dc_flushrange((void*)current_request.args[0], 0x40);
				break;
			case IPC_NAND_READ:
				err = nand_correct(last_page_read, ipc_data, ipc_ecc);

				if (current_request.args[1] != 0xFFFFFFFF) {
					memcpy32((void*)current_request.args[1], ipc_data, PAGE_SIZE);
					dc_flushrange((void*)current_request.args[1], PAGE_SIZE);
				}
				if (current_request.args[2] != 0xFFFFFFFF) {
					memcpy32((void*)current_request.args[2], ipc_ecc, PAGE_SPARE_SIZE);
					dc_flushrange((void*)current_request.args[2], PAGE_SPARE_SIZE);
				}
				break;
			case IPC_NAND_ERASE:
				// no action needed upon erase completion
				break;
			case IPC_NAND_WRITE:
				// no action needed upon write completion
				break;
			default:
				gecko_printf("Got IRQ for unknown NAND req %d\n", current_request.req);
		}
		code = current_request.code;
		tag = current_request.tag;
		current_request.code = 0;
		ipc_post(code, tag, 1, err);
	}
	irq_flag = 1;
}

inline void __nand_wait(void) {
	while(read32(NAND_CMD) & NAND_BUSY_MASK);
	if(read32(NAND_CMD) & NAND_ERROR)
		gecko_printf("NAND: Error on wait\n");
	ahb_flush_from(AHB_NAND);
	ahb_flush_to(AHB_STARLET);
}

void nand_send_command(u32 command, u32 bitmask, u32 flags, u32 num_bytes) {
	u32 cmd = NAND_BUSY_MASK | (bitmask << 24) | (command << 16) | flags | num_bytes;

	NAND_debug("nand_send_command(%x, %x, %x, %x) -> %x\n",
		command, bitmask, flags, num_bytes, cmd);

	write32(NAND_CMD, 0x7fffffff);
	write32(NAND_CMD, 0);
	write32(NAND_CMD, cmd);
}

void __nand_set_address(s32 page_off, s32 pageno) {
	if (page_off != -1) write32(NAND_ADDR0, page_off);
	if (pageno != -1)   write32(NAND_ADDR1, pageno);
}

void __nand_setup_dma(u8 *data, u8 *spare) {
	if (((s32)data) != -1) {
		write32(NAND_DATA, dma_addr(data));
	}
	if (((s32)spare) != -1) {
		u32 addr = dma_addr(spare);
		if(addr & 0x7f)
			gecko_printf("NAND: Spare buffer 0x%08x is not aligned, data will be corrupted\n", addr);
		write32(NAND_ECC, addr);
	}
}

int nand_reset(void) {
	NAND_debug("nand_reset()\n");
// IOS actually uses NAND_FLAGS_IRQ | NAND_FLAGS_WAIT here
	nand_send_command(NAND_RESET, 0, NAND_FLAGS_WAIT, 0);
	__nand_wait();
// enable NAND controller
	write32(NAND_CONF, 0x08000000);
// set configuration parameters for 512MB flash chips
	write32(NAND_CONF, 0x4b3e0e7f);
	return 0;
}

void nand_get_id(u8 *idbuf) {
	irq_flag = 0;
	__nand_set_address(0,0);

	dc_invalidaterange(idbuf, 0x40);

	__nand_setup_dma(idbuf, (u8 *)-1);
	nand_send_command(NAND_CHIPID, 1, NAND_FLAGS_IRQ | NAND_FLAGS_RD, 0x40);
}

void nand_get_status(u8 *status_buf) {
	irq_flag = 0;
	status_buf[0]=0;

	dc_invalidaterange(status_buf, 0x40);

	__nand_setup_dma(status_buf, (u8 *)-1);
	nand_send_command(NAND_GETSTATUS, 0, NAND_FLAGS_IRQ | NAND_FLAGS_RD, 0x40);
}

void nand_read_page(u32 pageno, void *data, void *ecc)
{nand_read_page2(pageno, data, ecc, 0);}

void nand_read_page2(u32 pageno, void *data, void *ecc, u32 addr0) {
	irq_flag = 0;
	last_page_read = pageno;  // needed for error reporting
	__nand_set_address(addr0, pageno);
	nand_send_command(NAND_READ_PRE, 0x1f, 0, 0);

	if (((s32)data) != -1) dc_invalidaterange(data, PAGE_SIZE);
	if (((s32)ecc) != -1)  dc_invalidaterange(ecc, ECC_BUFFER_SIZE);

	__nand_wait();
	__nand_setup_dma(data, ecc);
	nand_send_command(NAND_READ_POST, 0, NAND_FLAGS_IRQ | NAND_FLAGS_WAIT | NAND_FLAGS_RD | NAND_FLAGS_ECC, 0x840);
}

void nand_wait(void) {
// power-saving IRQ wait
	while(!irq_flag) {
		u32 cookie = irq_kill();
		if(!irq_flag)
			irq_wait();
		irq_restore(cookie);
	}
}

#ifdef NAND_SUPPORT_WRITE
void nand_write_page(u32 pageno, void *data, void *ecc)
{nand_write_page2(pageno, data, ecc, 0);}

void nand_write_page2(u32 pageno, void *data, void *ecc, u32 addr0) {
	irq_flag = 0;
	NAND_debug("nand_write_page(%u, %p, %p)\n", pageno, data, ecc);

// this is a safety check to prevent you from accidentally wiping out boot1 or boot2.
	if ((pageno < nand_min_page) || (pageno >= NAND_MAX_PAGE)) {
		gecko_printf("Error: nand_write to page %d forbidden\n", pageno);
		return;
	}
	if (((s32)data) != -1) dc_flushrange(data, PAGE_SIZE);
	if (((s32)ecc) != -1)  dc_flushrange(ecc, PAGE_SPARE_SIZE);
	ahb_flush_to(AHB_NAND);
	__nand_set_address(addr0, pageno);
	__nand_setup_dma(data, ecc);
	nand_send_command(NAND_WRITE_PRE, 0x1f, NAND_FLAGS_WR, 0x840);
	__nand_wait();
	nand_send_command(NAND_WRITE_POST, 0, NAND_FLAGS_IRQ | NAND_FLAGS_WAIT, 0);
}
#endif

#ifdef NAND_SUPPORT_ERASE
void nand_erase_block(u32 pageno)
{nand_erase_block2(pageno, 0);}

void nand_erase_block2(u32 pageno, u32 addr0) {
	irq_flag = 0;
	NAND_debug("nand_erase_block(%d)\n", pageno);

// this is a safety check to prevent you from accidentally wiping out boot1 or boot2.
	if ((pageno < nand_min_page) || (pageno >= NAND_MAX_PAGE)) {
		gecko_printf("Error: nand_erase to page %d forbidden\n", pageno);
		return;
	}
	__nand_set_address(addr0, pageno);
	nand_send_command(NAND_ERASE_PRE, 0x1c, 0, 0);
	__nand_wait();
	nand_send_command(NAND_ERASE_POST, 0, NAND_FLAGS_IRQ | NAND_FLAGS_WAIT, 0);
	NAND_debug("nand_erase_block(%d) done\n", pageno);
}
#endif

void nand_initialize(void)
{
	current_request.code = 0;
	nand_reset();
	irq_enable(IRQ_NAND);
}

int nand_correct(u32 pageno, void *data, void *ecc)
{
	(void) pageno;

	u8 *dp = (u8*)data;
	u32 *ecc_read = (u32*)((u8*)ecc+0x30);
	u32 *ecc_calc = (u32*)((u8*)ecc+0x40);
	int i;
	int uncorrectable = 0;
	int corrected = 0;
	
	for(i=0;i<4;i++) {
		u32 syndrome = *ecc_read ^ *ecc_calc; //calculate ECC syncrome
		// don't try to correct unformatted pages (all FF)
		if ((*ecc_read != 0xFFFFFFFF) && syndrome) {
			if(!((syndrome-1)&syndrome)) {
				// single-bit error in ECC
				corrected++;
			} else {
				// byteswap and extract odd and even halves
				u16 even = (syndrome >> 24) | ((syndrome >> 8) & 0xf00);
				u16 odd = ((syndrome << 8) & 0xf00) | ((syndrome >> 8) & 0x0ff);
				if((even ^ odd) != 0xfff) {
					// oops, can't fix this one
					uncorrectable++;
				} else {
					// fix the bad bit
					dp[odd >> 3] ^= 1<<(odd&7);
					corrected++;
				}
			}
		}
		dp += 0x200;
		ecc_read++;
		ecc_calc++;
	}
	if(uncorrectable || corrected)
		gecko_printf("ECC stats for NAND page 0x%x: %d uncorrectable, %d corrected\n", pageno, uncorrectable, corrected);
	if(uncorrectable)
		return NAND_ECC_UNCORRECTABLE;
	if(corrected)
		return NAND_ECC_CORRECTED;
	return NAND_ECC_OK;
}

void nand_ipc(volatile ipc_request *req)
{
	u32 new_min_page = 0x200;
	if (current_request.code != 0) {
		gecko_printf("NAND: previous IPC request is not done yet.");
		ipc_post(req->code, req->tag, 1, -1);
		return;
	}
	switch (req->req) {
		case IPC_NAND_RESET:
			nand_reset();
			ipc_post(req->code, req->tag, 0);
			break;

		case IPC_NAND_GETID:
			current_request = *req;
			nand_get_id(ipc_data);
			break;

		case IPC_NAND_STATUS:
			current_request = *req;
			nand_get_status(ipc_data);
			break;

		case IPC_NAND_READ:
			current_request = *req;
			nand_read_page(req->args[0], ipc_data, ipc_ecc);
			break;
#ifdef NAND_SUPPORT_WRITE
		case IPC_NAND_WRITE:
			current_request = *req;
			dc_invalidaterange((void*)req->args[1], PAGE_SIZE);
			dc_invalidaterange((void*)req->args[2], PAGE_SPARE_SIZE);
			memcpy(ipc_data, (void*)req->args[1], PAGE_SIZE);
			memcpy(ipc_ecc, (void*)req->args[2], PAGE_SPARE_SIZE);
			nand_write_page(req->args[0], ipc_data, ipc_ecc);
			break;
#endif
#ifdef NAND_SUPPORT_ERASE
		case IPC_NAND_ERASE:
			current_request = *req;
			nand_erase_block(req->args[0]);
			break;
#endif
/* This is only here to support the truly brave or stupid who are using hardware hacks to reflash
   boot1/boot2 onto blank or corrupted NAND flash chips.  Best practices dictate that you should
   query minpage (and make sure it is the value you expect -- usually 0x200) before writing to NAND.
   If you call SETMINPAGE, you MUST then call GETMINPAGE to check that it actually succeeded, do your
   writes, and then as soon as possible call SETMINPAGE(0x200) to restore the default minimum page. */
		case IPC_NAND_SETMINPAGE:
			new_min_page = req->args[0];
			if (new_min_page > 0x200) {
				gecko_printf("Ignoring strange NAND_SETMINPAGE request: %u\n", new_min_page);
				break;
			}
			gecko_printf("WARNING: setting minimum allowed NAND page to %u\n", new_min_page);
			nand_min_page = new_min_page;
			ipc_post(req->code, req->tag, 0);
			break;
		case IPC_NAND_GETMINPAGE:
			ipc_post(req->code, req->tag, 1, nand_min_page);
			break;
		default:
			gecko_printf("IPC: unknown SLOW NAND request %04x\n",
					req->req);
	}
}

int s_printf(char *buffer, const char *fmt, ...)
{	int i;
	va_list args;
	va_start(args, fmt);
	i = vsprintf(buffer, fmt, args);
	va_end(args);
	return i;
}

void safe_read(FIL *fp, const char *filename, FATFS *fatfs, void *buff, UINT btr)
{	FRESULT fres;
	UINT br, startingPoint = fp->fsize;
	fres = f_read(fp, buff, btr, &br);
	while(fres!=FR_OK || btr!=br)
	{	sdhc_exit();
		sdhc_init();
		if(f_mount(0, fatfs) != FR_OK)
			continue;
		if(f_open(fp, filename, FA_READ) != FR_OK)
			continue;
		if(f_lseek(fp, startingPoint) != FR_OK)
			continue;
		fres = f_read(fp, buff, btr, &br);
	}
}

void safe_write(FIL *fp, const char *filename, FATFS *fatfs, const void *buff, UINT btw)
{	FRESULT fres;
	UINT bw, startingPoint = fp->fsize;
	fres = f_write(fp, buff, btw, &bw);
	if(fres==FR_OK && btw==bw)
		fres = f_sync(fp);
	while(fres!=FR_OK || btw!=bw)
	{	sdhc_exit();
		sdhc_init();
		if(f_mount(0, fatfs) != FR_OK)
			continue;
		if(f_open(fp, filename, FA_OPEN_ALWAYS|FA_WRITE) != FR_OK)
			continue;
		if(f_lseek(fp, startingPoint) != FR_OK)
			continue;
		fres = f_write(fp, buff, btw, &bw);
		if(fres==FR_OK && btw==bw)
			fres = f_sync(fp);
	}
}

inline u32 pageToOffset(u32 page) {return (PAGE_SIZE + PAGE_SPARE_SIZE) * page;}

void writeKeys(FIL *fd, char* filename, FATFS *fatfs)
{	u32 i;
		// 256 human readable
	const char* humanReadable = "BackupMii v1, ConsoleID: %08x\n";
	u32 *tempBuffer = (u32*)ipc_data;
	for(i = 0; i < 0x40; i++)
		tempBuffer[i] = 0;
	write32(HW_OTPCMD, 9 | 0x80000000); // gets the console ID from OTP
	s_printf((char*)ipc_data, humanReadable, read32(HW_OTPDATA));
	safe_write(fd, filename, fatfs, ipc_data, 0x100);
	screen_printf(" .");
	
		// 128 OTP
	for(i = 0; i <= 0x1F; i++)
	{	write32(HW_OTPCMD, i | 0x80000000);
		tempBuffer[i] = read32(HW_OTPDATA);
	}safe_write(fd, filename, fatfs, ipc_data, 0x80);
	
		// 128 padding
	for(i = 0; i < 0x20; i++)
		tempBuffer[i] = 0;
	safe_write(fd, filename, fatfs, ipc_data, 0x80);
	screen_printf(" .");
	
		// 256 SEEPROM
	seeprom_read(ipc_data, 0, sizeof(seeprom_t) / 2);
	safe_write(fd, filename, fatfs, ipc_data, sizeof(seeprom_t));
	
		// 256 padding
	for(i = 0; i < 0x40; i++)
		tempBuffer[i] = 0;
	safe_write(fd, filename, fatfs, ipc_data, 0x100);
}

int write_keys_bin(char* filename, FATFS *fatfs)
{	int fres = 0;
	char*pathEnd = filename, *temp = pathEnd;
	FIL fd;
	while( (temp = strchr(pathEnd, '/')) )
		pathEnd = temp+1;
	pathEnd[0] = 'k';
	pathEnd[1] = 'e';
	pathEnd[2] = 'y';
	pathEnd[3] = 's';
	pathEnd[4] = '.';
	pathEnd[5] = 'b';
	pathEnd[6] = 'i';
	pathEnd[7] = 'n';
	pathEnd[8] = '\0';
	screen_printf("Writing file %s.", filename);
	fres = f_open(&fd, filename, FA_CREATE_ALWAYS|FA_WRITE);
	if(fres) return fres;
	writeKeys(&fd, filename, fatfs);
	return f_close(&fd);
}

int dump_NAND_to(char* filename, FATFS *fatfs)
{return dump_NAND_to2(filename, fatfs, 0, NAND_MAX_PAGE, 0);}

int dump_NAND_to0(char* filename, FATFS *fatfs)
{return dump_NAND_to2(filename, fatfs, NAND_MAX_PAGE, NAND_MAX_PAGE*2, 0);}

int dump_NAND_to1(char* filename, FATFS *fatfs)
{return dump_NAND_to2(filename, fatfs, 0, NAND_MAX_PAGE, 1);}

int dump_NAND_to2(char* filename, FATFS *fatfs, u32 startpage, u32 endpage, u32 addr0)
{	u32 page, temp, bw;
	int ret, fres = 0;
	FIL fd;
	fres = f_open(&fd, filename, FA_CREATE_ALWAYS|FA_WRITE);
	if(fres) return fres;
	screen_printf("\nNAND dump process started. Do NOT remove the SD card.\n\n - blocks dumped:\n0    / %d.\r", endpage/64);
	for (page = startpage; page < endpage; screen_printf("%d\r", page/64))
	{	for (temp = page; page < temp+64; page++)
		{	nand_read_page2(page, ipc_data, ipc_ecc, addr0);
			nand_wait();
			
			ret = nand_correct(page, ipc_data, ipc_ecc);
			if (ret < 0)
				screen_printf(" - bad NAND page found: 0x%x (from block %d).\n     / %d.\r", page, page/64, endpage/64);
			
			/* Save the normal 2048 bytes from the current page */
			fres = f_write(&fd, ipc_data, PAGE_SIZE, &bw);
			if(fres!=FR_OK || PAGE_SIZE!=bw)
				break;
			
			/* Save the additional 64 bytes with spare / ECC data */
			fres = f_write(&fd, ipc_ecc, PAGE_SPARE_SIZE, &bw);
			if(fres!=FR_OK || PAGE_SPARE_SIZE!=bw)
				break;
		}
		if(page == temp+64)
			fres = f_sync(&fd);
		if(fres != FR_OK || page < temp+64)while(1)
		{	sdhc_exit();
			sdhc_init();
			if(f_mount(0, fatfs) != FR_OK)
				continue;
			if(f_open(&fd, filename, FA_OPEN_ALWAYS|FA_WRITE) != FR_OK)
				continue;
			if(f_lseek(&fd, pageToOffset(temp)) != FR_OK)
				continue;
			page = temp;
			break;
		}
	}
	screen_printf("\nWriting footer.");
	writeKeys(&fd, filename, fatfs);
	screen_printf(" Done.\n");
	fres = f_close(&fd);
	if( (write_keys_bin(filename, fatfs) == FR_OK) )
		screen_printf(" Done.\n");
	else screen_printf(" Failed.\n");
	return fres;
}

int write_NAND_from(char* filename, FATFS *fatfs)
{	u32 page;
	int fres = 0;
	FIL fd;
	u32 *tempBuffer = (u32*)ipc_data;
	fres = f_open(&fd, filename, FA_READ);
	if(fres) return fres;

	screen_printf("Checking console file against this console's OTP.");
	fres = f_lseek(&fd, pageToOffset(NAND_MAX_PAGE)+256);	// dump+256 human readable
	if(fres) return fres;

		// 128 OTP
	for(page = 0; page <= 0x1F; page++)
	{	write32(HW_OTPCMD, page | 0x80000000);
		tempBuffer[page] = read32(HW_OTPDATA);
	}safe_read(&fd, filename, fatfs, ipc_data+128, 0x80);
	if(memcmp(ipc_data, ipc_data+128, 128))
	{	screen_printf(" Failed.\n");
		return -1000; // NOT my NAND dump ... NOT happening!!!
	}else screen_printf("\n");
	
	fres = f_lseek(&fd, pageToOffset(nand_min_page));
	if(fres) return fres;
	
	screen_printf("\nNAND write process started. Do NOT remove/change the SD card, power down,\nlet the power go out, touch any buttons or do ANYTHING with your Wii until the process has completed.\nDoing so  WILL  BRICK  YOUR  WII !!!!\n\n - blocks dumped:\n0    / %d.\r", NAND_MAX_PAGE/64);
	for (page = nand_min_page; page < NAND_MAX_PAGE; page++)
	{	/* Read the normal 2048 bytes for the current page */
		safe_read(&fd, filename, fatfs, ipc_data, PAGE_SIZE);

		/* Read the additional 64 bytes of spare / ECC data */
		safe_read(&fd, filename, fatfs, ipc_ecc, PAGE_SPARE_SIZE);
 		
		nand_write_page(page, ipc_data, ipc_ecc);
		nand_wait();
		
		if((page+1)%64 == 0)
			screen_printf("%d\r", (page+1)/64);
	}
	screen_printf("\nDone.\n");
	return f_close(&fd);
}
