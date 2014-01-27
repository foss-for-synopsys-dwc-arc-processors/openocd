/***************************************************************************
 *   Copyright (C) 2013-2014 Synopsys, Inc.                                *
 *   Frank Dols <frank.dols@synopsys.com>                                  *
 *   Mischa Jonker <mischa.jonker@synopsys.com>                            *
 *   Anton Kolesov <anton.kolesov@synopsys.com>                            *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
 ***************************************************************************/

#ifndef ARC_REGS_H
#define ARC_REGS_H

#include "target.h"
#if 0
/* ARCompatISA-v2 register set definition */
#define ARC32_NUM_CORE_REGS			32
#define ARC32_NUM_EXTENSION_REGS	59 - 32 + 1	/* R59 - R32 + 1 */
#define ARC32_LAST_EXTENSION_REG	59			/* R59 */

/* Particular ARCompatISA-v2 core registers */
#define LP_COUNT_REG				60	/* loop count */
#define LIDI_REG					62	/* long imediate data indicator */
#define PCL_REG						63	/* loaded instruction PC */

/*
 * OpenOCD register cache registers
 *   check the arc32-read/write_registers functions
 */
#define PC_REG						65	/* program counter */

/* GNU GDB register set (expected to receive) */
#define ARC32_NUM_AUX_REGS			47	/* expected by GDB */
#define ARC32_NUM_GDB_REGS			97
#endif


/* --------------------------------------------------------------------------
 * ARC core Auxiliary register set
 *      name:					id:		bitfield:	comment:
 *      ------                  ----    ----------  ---------
 */
#define AUX_STATUS_REG			0x0					/* LEGACY, IS OBSOLETE */
#define STAT_HALT_BIT					(1 << 25)

#define AUX_SEMAPHORE_REG	 	0x1
#define AUX_LP_START_REG		0x2
#define AUX_LP_END_REG			0x3
#define AUX_IDENTITY_REG		0x4

#define AUX_DEBUG_REG			0x5
#define SET_CORE_SINGLE_STEP			(1)
#define SET_CORE_FORCE_HALT				(1 << 1)
#define SET_CORE_SINGLE_INSTR_STEP		(1 << 11)
#define SET_CORE_RESET_APPLIED			(1 << 22)
#define SET_CORE_SLEEP_MODE				(1 << 23)
#define SET_CORE_USER_BREAKPOINT		(1 << 28)
#define SET_CORE_BREAKPOINT_HALT		(1 << 29)
#define SET_CORE_SELF_HALT				(1 << 30)
#define SET_CORE_LOAD_PENDING			(1 << 31)

#define AUX_PC_REG				0x6

#define AUX_STATUS32_REG		0xA
#define SET_CORE_HALT_BIT				(1)
#define SET_CORE_INTRP_MASK_E1			(1 << 1)
#define SET_CORE_INTRP_MASK_E2			(1 << 2)
#define SET_CORE_AE_BIT					(1 << 5)

#define AUX_STATUS32_L1_REG		0xB
#define AUX_STATUS32_L2_REG		0xC
#define AUX_USER_SP_REG         0xD

#define AUX_IC_IVIC_REG			0X10
#define IC_IVIC_INVALIDATE		0XFFFFFFFF

#define AUX_COUNT0_REG			0x21
#define AUX_CONTROL0_REG		0x22
#define AUX_LIMIT0_REG			0x23
#define AUX_INT_VECTOR_BASE_REG	0x25
#define AUX_MACMODE_REG			0x41
#define AUX_IRQ_LV12_REG		0x43

#define AUX_DC_IVDC_REG			0X47
#define DC_IVDC_INVALIDATE		(1)
#define AUX_DC_CTRL_REG			0X48
#define DC_CTRL_IM			(1 << 6)

#define AUX_COUNT1_REG			0x100
#define AUX_CONTROL1_REG		0x101
#define AUX_LIMIT1_REG			0x102
#define AUX_IRQ_LEV_REG			0x200
#define AUX_IRQ_HINT_REG		0x201
#define AUX_ERET_REG			0x400
#define AUX_ERBTA_REG			0x401
#define AUX_ERSTATUS_REG		0x402
#define AUX_ECR_REG				0x403
#define AUX_EFA_REG				0x404
#define AUX_ICAUSE1_REG			0x40A
#define AUX_ICAUSE2_REG			0x40B

#define AUX_IENABLE_REG			0x40C
#define SET_CORE_DISABLE_INTERRUPTS		0x00000000
#define SET_CORE_ENABLE_INTERRUPTS		0xFFFFFFFF

#define AUX_ITRIGGER_REG		0x40D
#define AUX_XPU_REG				0x410
#define AUX_BTA_REG				0x412
#define AUX_BTA_L1_REG			0x413
#define AUX_BTA_L2_REG			0x414
#define AUX_IRQ_PULSE_CAN_REG	0x415
#define AUX_IRQ_PENDING_REG		0x416
#define AUX_XFLAGS_REG			0x44F

#define AUX_BCR_VER_REG			0x60
#define AUX_BTA_LINK_BUILD_REG	0x63
#define AUX_VECBASE_AC_BUILD_REG	0x68
#define AUX_RF_BUILD_REG		0x6E
#define AUX_ISA_CONFIG_REG		0xC1
#define AUX_DCCM_BUILD_REG		0x74

#define AUX_ICCM_BUILD_REG		0x78

#if 0
/* OpenOCD ARC core & aux registers hook structure */
struct arc32_core_reg {
	uint32_t num;
	struct target *target;
	struct arc32_common *arc32_common;
};

struct arc32_aux_reg {
	uint32_t num;
	struct target *target;
	struct arc32_common *arc32_common;
};
#endif
struct arc32_reg_desc {
	uint32_t regnum;
	char * const name;
	uint32_t addr;
};

struct arc32_reg_arch_info {
	const struct arc32_reg_desc *desc;
	struct target *target;
	struct arc32_common *arc32_common;
};

enum arc32_reg_number {
	/* Core registers */
	R0 = 0,
	R1,
	R2,
	R3,
	R4,
	R5,
	R6,
	R7,
	R8,
	R9,
	R10,
	R11,
	R12,
	R13,
	R14,
	R15,
	R16,
	R17,
	R18,
	R19,
	R20,
	R21,
	R22,
	R23,
	R24,
	R25,
	R26 = 26,
	GP = 26,
	R27 = 27,
	FP = 27,
	R28 = 28,
	SP = 28,
	R29 = 29,
	ILINK = 29,
	R30 = 30,
	R31 = 31,
	BLINK = 31,
	/* Core extension registers */
	R32 = 32,
	R33,
	R34,
	R35,
	R36,
	R37,
	R38,
	R39,
	R40,
	R41,
	R42,
	R43,
	R44,
	R45,
	R46,
	R47,
	R48,
	R49,
	R50,
	R51,
	R52,
	R53,
	R54,
	R55,
	R56,
	R57,
	/* In HS R58 and R59 are ACCL and ACCH. */
	R58,
	R59,
	R60 = 60,
	LP_COUNT = 60,
	/* Reserved */
	R61 = 61,
	/* LIMM is not a realk registers. */
	R62 = 62,
	LIMM = 62,
	R63 = 63,
	PCL = 63,
	CORE_NUM_REGS = PCL,
	/* AUX registers */
	PC = 64,
	FIRST_AUX_REG = PC,
	STATUS32,
	LP_START,
	LP_END,

	TOTAL_NUM_REGS,
};

/* ----- Exported functions ------------------------------------------------ */

struct reg_cache *arc_regs_build_reg_cache(struct target *target);

int arc_regs_read_core_reg(struct target *target, int num);
int arc_regs_write_core_reg(struct target *target, int num);
int arc_regs_read_registers(struct target *target, uint32_t *regs);
int arc_regs_write_registers(struct target *target, uint32_t *regs);

int arc_regs_get_gdb_reg_list(struct target *target, struct reg **reg_list[],
	int *reg_list_size, enum target_register_class reg_class);

int arc_regs_print_core_registers(struct target *target);
int arc_regs_print_aux_registers(struct arc_jtag *jtag_info);

#endif /* ARC_REGS_H */
