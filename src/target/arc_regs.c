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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "arc.h"

/* ----- Supporting functions ---------------------------------------------- */

/* Describe all possible registers. */
static const struct arc32_reg_desc arc32_regs_descriptions[ARC_TOTAL_NUM_REGS] = {
	/* regnum, name, address */
	{ ARC_REG_R0, "r0", 0 },
	{ ARC_REG_R1, "r1", 1 },
	{ ARC_REG_R2, "r2", 2 },
	{ ARC_REG_R3, "r3", 3 },
	{ ARC_REG_R4, "r4", 4 },
	{ ARC_REG_R5, "r5", 5 },
	{ ARC_REG_R6, "r6", 6 },
	{ ARC_REG_R7, "r7", 7 },
	{ ARC_REG_R8, "r8", 8 },
	{ ARC_REG_R9, "r9", 9 },
	{ ARC_REG_R10, "r10", 10 },
	{ ARC_REG_R11, "r11", 11 },
	{ ARC_REG_R12, "r12", 12 },
	{ ARC_REG_R13, "r13", 13 },
	{ ARC_REG_R14, "r14", 14 },
	{ ARC_REG_R15, "r15", 15 },
	{ ARC_REG_R16, "r16", 16 },
	{ ARC_REG_R17, "r17", 17 },
	{ ARC_REG_R18, "r18", 18 },
	{ ARC_REG_R19, "r19", 19 },
	{ ARC_REG_R20, "r20", 20 },
	{ ARC_REG_R21, "r21", 21 },
	{ ARC_REG_R22, "r22", 22 },
	{ ARC_REG_R23, "r23", 23 },
	{ ARC_REG_R24, "r24", 24 },
	{ ARC_REG_R25, "r25", 25 },
	{ ARC_REG_R26, "gp", 26 },
	{ ARC_REG_R27, "fp", 27 },
	{ ARC_REG_R28, "sp", 28 },
	{ ARC_REG_R29, "ilink", 29 },
	{ ARC_REG_R30, "r30", 30 },
	{ ARC_REG_R31, "blink", 31 },
	{ ARC_REG_R32, "r32", 32 },
	{ ARC_REG_R33, "r33", 33 },
	{ ARC_REG_R34, "r34", 34 },
	{ ARC_REG_R35, "r35", 35 },
	{ ARC_REG_R36, "r36", 36 },
	{ ARC_REG_R37, "r37", 37 },
	{ ARC_REG_R38, "r38", 38 },
	{ ARC_REG_R39, "r39", 39 },
	{ ARC_REG_R40, "r40", 40 },
	{ ARC_REG_R41, "r41", 41 },
	{ ARC_REG_R42, "r42", 42 },
	{ ARC_REG_R43, "r43", 43 },
	{ ARC_REG_R44, "r44", 44 },
	{ ARC_REG_R45, "r45", 45 },
	{ ARC_REG_R46, "r46", 46 },
	{ ARC_REG_R47, "r47", 47 },
	{ ARC_REG_R48, "r48", 48 },
	{ ARC_REG_R49, "r49", 49 },
	{ ARC_REG_R50, "r50", 50 },
	{ ARC_REG_R51, "r51", 51 },
	{ ARC_REG_R52, "r52", 52 },
	{ ARC_REG_R53, "r53", 53 },
	{ ARC_REG_R54, "r54", 54 },
	{ ARC_REG_R55, "r55", 55 },
	{ ARC_REG_R56, "r56", 56 },
	{ ARC_REG_R57, "r57", 57 },
	{ ARC_REG_R58, "r58", 58 },
	{ ARC_REG_R59, "r59", 59 },
	{ ARC_REG_R60, "lp_count", 60 },
	{ ARC_REG_R61, "r61", 61 },
	{ ARC_REG_R62, "limm", 62 },
	{ ARC_REG_R63, "pcl", 63 },
	/* AUX */
	{ ARC_REG_PC, "pc", PC_REG_ADDR },
	{ ARC_REG_STATUS32, "status32", STATUS32_REG_ADDR },
	{ ARC_REG_LP_START, "lp_start",  LP_START_REG_ADDR },
	{ ARC_REG_LP_END,   "lp_end",   LP_END_REG_ADDR },
};

static int arc_regs_get_core_reg(struct reg *reg) {
	int retval = ERROR_OK;
	LOG_DEBUG("Getting register");
	assert(reg != NULL);

	struct arc_reg_t *arc_reg = reg->arch_info;
	struct target *target = arc_reg->target;
	struct arc32_common *arc32 = target_to_arc32(target);
	const uint32_t regnum = arc_reg->desc->regnum;

	if (target->state != TARGET_HALTED)
		return ERROR_TARGET_NOT_HALTED;

	if (regnum >= ARC_TOTAL_NUM_REGS)
		return ERROR_COMMAND_SYNTAX_ERROR;

	if (reg->valid) {
		LOG_DEBUG("Get register (cached) regnum=%" PRIu32 ", name=%s, value=0x%08" PRIx32,
				regnum, arc_reg->desc->name, arc_reg->value);
		return ERROR_OK;
	}

	if (regnum == ARC_REG_LIMM || regnum == ARC_REG_RESERVED) {
		arc_reg->value = 0;
	} else	if (regnum < ARC_REG_AFTER_CORE_EXT) {
		arc_jtag_read_core_reg_one(&arc32->jtag_info, arc_reg->desc->addr, &arc_reg->value);
	} else {
		arc_jtag_read_aux_reg_one(&arc32->jtag_info, arc_reg->desc->addr, &arc_reg->value);
	}

	// retval = arc32_target->read_core_reg(target, arc32_reg->desc->regnum);
	//uint32_t reg_value;
	/* get pointers to arch-specific information */
	//struct arc32_common *arc32 = target_to_arc32(target);

	//reg_value = arc32->core_regs[regnum];
	buf_set_u32(arc32->core_cache->reg_list[regnum].value, 0, 32, arc_reg->value);
	arc32->core_cache->reg_list[regnum].valid = true;
	arc32->core_cache->reg_list[regnum].dirty = false;
	LOG_DEBUG("Get register regnum=%" PRIu32 ", name=%s, value=0x%08" PRIx32,
			regnum , arc_reg->desc->name, arc_reg->value);

	return retval;
}

static int arc_regs_set_core_reg(struct reg *reg, uint8_t *buf)
{
	int retval = ERROR_OK;

	LOG_DEBUG("Entering set core reg...");
	struct arc_reg_t *arc_reg = reg->arch_info;
	struct target *target = arc_reg->target;
	struct arc32_common *arc32 = target_to_arc32(target);
	uint32_t value = buf_get_u32(buf, 0, 32);
	uint32_t regnum = arc_reg->desc->regnum;

	if (target->state != TARGET_HALTED)
		return ERROR_TARGET_NOT_HALTED;

	if (regnum >= ARC_TOTAL_NUM_REGS)
		return ERROR_COMMAND_SYNTAX_ERROR;

	buf_set_u32(reg->value, 0, 32, value);

	arc_reg->value = value;

	LOG_DEBUG("Set register regnum=%" PRIu32 ", name=%s, value=0x%08" PRIx32, regnum, arc_reg->desc->name, value);
	arc32->core_cache->reg_list[regnum].valid = true;
	arc32->core_cache->reg_list[regnum].dirty = true;

	return retval;
}

static const struct reg_arch_type arc32_reg_type = {
	.get = arc_regs_get_core_reg,
	.set = arc_regs_set_core_reg,
};

/* ----- Exported functions ------------------------------------------------ */

static const char * const general_group_name = "general";
static const char * const float_group_name = "float";
static const char * const feature_core_basecase_name = "org.gnu.gdb.arc.core-basecase";
static const char * const feature_core_extension_name = "org.gnu.gdb.arc.core-extension";
static const char * const feature_core_pointers_name = "org.gnu.gdb.arc.core-pointers";
static const char * const feature_core_link_name = "org.gnu.gdb.arc.core-linkregs.v2";
static const char * const feature_core_other_name = "org.gnu.gdb.arc.core-other";
static const char * const feature_aux_baseline_name = "org.gnu.gdb.arc.aux-baseline.v2";

struct reg_cache *arc_regs_build_reg_cache(struct target *target)
{
	uint32_t i;

	/* get pointers to arch-specific information */
	struct arc32_common *arc32 = target_to_arc32(target);
	struct reg_cache **cache_p = register_get_last_cache_p(&target->reg_cache);
	struct reg_cache *cache = malloc(sizeof(struct reg_cache));
	struct reg *reg_list = calloc(ARC_TOTAL_NUM_REGS, sizeof(struct reg));
	struct arc_reg_t *arch_info = calloc(ARC_TOTAL_NUM_REGS, sizeof(struct arc_reg_t));

	/* Build the process context cache */
	cache->name = "arc32 registers";
	cache->next = NULL;
	cache->reg_list = reg_list;
	cache->num_regs = ARC_TOTAL_NUM_REGS;
	(*cache_p) = cache;
	arc32->core_cache = cache;

	// XML feature
	struct reg_feature *core_basecase = calloc(1, sizeof(struct reg_feature));
	core_basecase->name = feature_core_basecase_name;
	struct reg_feature *core_extension = calloc(1, sizeof(struct reg_feature));
	core_extension->name = feature_core_extension_name;
	struct reg_feature *core_pointers = calloc(1, sizeof(struct reg_feature));
	core_pointers->name = feature_core_pointers_name;
	struct reg_feature *core_link = calloc(1, sizeof(struct reg_feature));
	core_link->name = feature_core_link_name;
	struct reg_feature *core_other = calloc(1, sizeof(struct reg_feature));
	core_other->name = feature_core_other_name;
	struct reg_feature *aux_baseline = calloc(1, sizeof(struct reg_feature));
	aux_baseline->name = feature_aux_baseline_name;

	for (i = 0; i < ARC_TOTAL_NUM_REGS; i++) {
		arch_info[i].desc = arc32_regs_descriptions + i;
		arch_info[i].target = target;
		arch_info[i].arc32_common = arc32;
		reg_list[i].name = arc32_regs_descriptions[i].name;
		reg_list[i].size = 32;
		reg_list[i].value = calloc(1, 4);
		reg_list[i].dirty = 0;
		reg_list[i].valid = 0;
		reg_list[i].type = &arc32_reg_type;
		reg_list[i].arch_info = &arch_info[i];
		// XML
		reg_list[i].number = arc32_regs_descriptions[i].regnum;
		reg_list[i].exist = 1;
		reg_list[i].group = general_group_name;
		reg_list[i].caller_save = true;

		if (i < ARC_REG_GP)
			reg_list[i].feature = core_basecase;
		else if (ARC_REG_GP <= i && i < ARC_REG_ILINK)
			reg_list[i].feature = core_pointers;
		else if (ARC_REG_ILINK <= i && i < ARC_REG_AFTER_CORE)
			reg_list[i].feature = core_link;
		else if ((ARC_REG_FIRST_CORE_EXT <= i && i < ARC_REG_AFTER_CORE_EXT) || i == ARC_REG_LIMM || i == ARC_REG_RESERVED) {
			reg_list[i].feature = core_extension;
			reg_list[i].exist = false;
		} else if (ARC_REG_AFTER_CORE_EXT <= i && i < ARC_REG_FIRST_AUX)
			reg_list[i].feature = core_other;
		else if (ARC_REG_PC <= i && i <= ARC_REG_LP_END)
			reg_list[i].feature = aux_baseline;
		else {
			LOG_WARNING("Unknown register with number %" PRIu32, i);
			reg_list[i].feature = NULL;
		}

		/* Temporary hack until proper detection of registers is implemented. */
		if (i >= ARC_REG_AFTER_GDB_GENERAL) {
			reg_list[i].exist = false;
		} else {
			LOG_DEBUG("reg n=%3i name=%3s group=%s feature=%s", i,
					reg_list[i].name, reg_list[i].group,
					reg_list[i].feature->name); }

		// end XML
	}

	return cache;
}

int arc_regs_get_gdb_reg_list(struct target *target, struct reg **reg_list[],
	int *reg_list_size, enum target_register_class reg_class)
{
	int retval = ERROR_OK;
	int i;

	struct arc32_common *arc32 = target_to_arc32(target);

	/* get pointers to arch-specific information storage */
	*reg_list_size = ARC_TOTAL_NUM_REGS;
	*reg_list = malloc(sizeof(struct reg *) * (*reg_list_size));

	/* OpenOCD gdb_server API seems to be inconsistent here: when it generates
	 * XML tdesc it filters out !exist registers, however when creating a
	 * g-packet it doesn't do so. REG_CLASS_ALL is used in first case, and
	 * REG_CLASS_GENERAL used in the latter one. Due to this we had to filter
	 * out !exist register for "general", but not for "all". Attempts to filter out
	 * !exist for "all" as well will cause a failed check in OpenOCD GDB
	 * server. */
	if (reg_class == REG_CLASS_ALL) {
		for (i = 0; i < ARC_TOTAL_NUM_REGS; i++) {
			(*reg_list)[i] = &arc32->core_cache->reg_list[i];
		}
		LOG_DEBUG("REG_CLASS_ALL: number of regs=%i", *reg_list_size);
	} else {
		int cur_index = 0;
		for (i = 0; i < ARC_TOTAL_NUM_REGS; i++) {
			if (i < ARC_REG_AFTER_GDB_GENERAL && arc32->core_cache->reg_list[i].exist){
				(*reg_list)[cur_index] = &arc32->core_cache->reg_list[i];
				cur_index += 1;
			}
		}
		*reg_list_size = cur_index;
		LOG_DEBUG("REG_CLASS_GENERAL: number of regs=%i", *reg_list_size);
	}

	return retval;
}

