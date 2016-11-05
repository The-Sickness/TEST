/*
<<<<<<< HEAD
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
=======
 * Copy from user space to user space
 *
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
>>>>>>> 512ca3c... stock
 */

#include <linux/linkage.h>
#include <asm/assembler.h>

<<<<<<< HEAD
/* Parameters */
dstin	.req	x0
src	.req	x1
count	.req	x2

/* Return value */
ret_val	.req	x0	/* Aliased with dstin */

/* Local version of dstin */
dst	.req	x3
last_dst .req	x4
last_src .req	x5


#define LOAD_USER( offset, x...)		\
9999:	x;					\
	.section __ex_table,"a";		\
	.align	3;				\
	.quad	9999b, load_fixup ## offset;	\
	.previous

#define STORE_USER(offset, x...)		\
9999:	x;					\
	.section __ex_table,"a";		\
	.align	3;				\
	.quad	9999b, store_fixup ## offset;	\
	.previous

ENTRY(__copy_in_user)
	mov	dst, dstin
	add	last_src, src, count
	add	last_dst, dst, count
	mov	ret_val, #0
#include "memcpy_base.h"
ENDPROC(__copy_in_user)


/* Create a fixup target for each load and store offset. Calculate the number
   of not copied bytes with (last_src - src - offset). There may be more bytes
   copied. But, this ensures that all of the bytes before it have been copied.
*/

#define FIXUP_LOAD( offset)			\
load_fixup ## offset ## :			\
.globl load_fixup ## offset ## ;		\
	sub	ret_val, last_src, src;		\
	sub	ret_val, ret_val, offset;	\
	ret;

#define FIXUP_STORE(offset)			\
store_fixup ## offset ## :			\
.globl store_fixup ## offset ##	;		\
	sub	ret_val, last_dst, dst;		\
	sub	ret_val, ret_val, offset;	\
	ret;

	.section .fixup,"ax"
	.align 2
	FIXUP_LOAD( 0x00)

	FIXUP_STORE(0x00)
	FIXUP_STORE(0x01)
	FIXUP_STORE(0x02)
	FIXUP_STORE(0x03)
	FIXUP_STORE(0x04)
	FIXUP_STORE(0x05)
	FIXUP_STORE(0x06)
	FIXUP_STORE(0x07)
	FIXUP_STORE(0x08)
	FIXUP_STORE(0x10)
	FIXUP_STORE(0x18)
	FIXUP_STORE(0x20)
	FIXUP_STORE(0x28)
	FIXUP_STORE(0x30)
	FIXUP_STORE(0x38)
	FIXUP_STORE(0x40)
=======
/*
 * Copy from user space to user space (alignment handled by the hardware)
 *
 * Parameters:
 *	x0 - to
 *	x1 - from
 *	x2 - n
 * Returns:
 *	x0 - bytes not copied
 */
ENTRY(__copy_in_user)
	add	x4, x0, x2			// upper user buffer boundary
	subs	x2, x2, #8
	b.mi	2f
1:
USER(9f, ldr	x3, [x1], #8	)
	subs	x2, x2, #8
USER(9f, str	x3, [x0], #8	)
	b.pl	1b
2:	adds	x2, x2, #4
	b.mi	3f
USER(9f, ldr	w3, [x1], #4	)
	sub	x2, x2, #4
USER(9f, str	w3, [x0], #4	)
3:	adds	x2, x2, #2
	b.mi	4f
USER(9f, ldrh	w3, [x1], #2	)
	sub	x2, x2, #2
USER(9f, strh	w3, [x0], #2	)
4:	adds	x2, x2, #1
	b.mi	5f
USER(9f, ldrb	w3, [x1]	)
USER(9f, strb	w3, [x0]	)
5:	mov	x0, #0
	ret
ENDPROC(__copy_in_user)

	.section .fixup,"ax"
	.align	2
9:	sub	x0, x4, x0			// bytes not copied
	ret
>>>>>>> 512ca3c... stock
	.previous
