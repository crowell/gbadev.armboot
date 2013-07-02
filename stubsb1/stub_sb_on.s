/*
	TinyLoad - a simple region free (original) game launcher in 4k

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

# This code comes from the Twilight Hack
# Copyright 2008-2009  Segher Boessenkool  <segher@kernel.crashing.org>

# Slightly modified to imitate the homebrew channel stub

*/
.set r0,0;		.set r1,1;		.set r2,2;		.set r3,3;		.set r4,4;
.set r5,5;		.set r6,6;		.set r7,7;		.set r8,8;		.set r9,9;
.set r10,10;	.set r11,11;	.set r12,12;	.set r13,13;	.set r14,14;
.set r15,15;	.set r16,16;	.set r17,17;	.set r18,18;	.set r19,19;
.set r20,20;	.set r21,21;	.set r22,22;	.set r23,23;	.set r24,24;
.set r25,25;	.set r26,26;	.set r27,27;	.set r28,28;	.set r29,29;
.set r30,30;	.set r31,31;

#include "hw.h" 	 

	.globl _start  
_start:

	lis		r3,0x0d80		#HW_REG_BASE physical address
	lwz 	r4,0xc4(r3)		#HW_GPIO1BDIR
	ori		r4,r4,0x100
	stw		r4,0xc4(r3)

	lis		r3,0x0d80		#HW_REG_BASE physical address
	lwz 	r4,0xc0(r3)		#HW_GPIO1BOUT
	ori		r4,r4,0x100
	stw		r4,0xc0(r3)
	b		.
	b		.
