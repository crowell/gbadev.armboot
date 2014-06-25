/*
	mini - a Free Software replacement for the Nintendo/BroadOn IOS.
	low-level NAND support

Copyright (C) 2008, 2009	Hector Martin "marcan" <marcan@marcansoft.com>
Copyright (C) 2008, 2009	Sven Peter <svenpeter@gmail.com>

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef __NAND_H__
#define __NAND_H__

#include "types.h"
#include "ipc.h"
#include "ff.h"

#define PAGE_SIZE		2048
#define PAGE_SPARE_SIZE		64
#define ECC_BUFFER_SIZE		(PAGE_SPARE_SIZE+16)
#define ECC_BUFFER_ALLOC	(PAGE_SPARE_SIZE+32)
#define BLOCK_SIZE		64
#define NAND_MAX_PAGE		0x40000

void nand_irq(void);

void nand_send_command(u32 command, u32 bitmask, u32 flags, u32 num_bytes);
int nand_reset(void);
void nand_get_id(u8 *);
void nand_get_status(u8 *);
void nand_read_page(u32 pageno, void *data, void *ecc);
void nand_write_page(u32 pageno, void *data, void *ecc);
void nand_erase_block(u32 pageno);
void nand_read_page2(u32 pageno, void *data, void *ecc, u32 addr0);
void nand_write_page2(u32 pageno, void *data, void *ecc, u32 addr0);
void nand_erase_block2(u32 pageno, u32 addr0);
void nand_wait(void);
int dump_NAND_to(char* fileName, FATFS *fatfs);
int dump_NAND_to0(char* fileName, FATFS *fatfs);
int dump_NAND_to1(char* fileName, FATFS *fatfs);
int dump_NAND_to2(char* fileName, FATFS *fatfs, u32 startpage, u32 endpage, u32 addr0);

#define NAND_ECC_OK 0
#define NAND_ECC_CORRECTED 1
#define NAND_ECC_UNCORRECTABLE -1

int nand_correct(u32 pageno, void *data, void *ecc);
void nand_initialize(void);
void nand_ipc(volatile ipc_request *req);

#endif

