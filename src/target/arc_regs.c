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
	{ R0, "r0", 0 },
	{ R1, "r1", 1 },
	{ R2, "r2", 2 },
	{ R3, "r3", 3 },
	{ R4, "r4", 4 },
	{ R5, "r5", 5 },
	{ R6, "r6", 6 },
	{ R7, "r7", 7 },
	{ R8, "r8", 8 },
	{ R9, "r9", 9 },
	{ R10, "r10", 10 },
	{ R11, "r11", 11 },
	{ R12, "r12", 12 },
	{ R13, "r13", 13 },
	{ R14, "r14", 14 },
	{ R15, "r15", 15 },
	{ R16, "r16", 16 },
	{ R17, "r17", 17 },
	{ R18, "r18", 18 },
	{ R19, "r19", 19 },
	{ R20, "r20", 20 },
	{ R21, "r21", 21 },
	{ R22, "r22", 22 },
	{ R23, "r23", 23 },
	{ R24, "r24", 24 },
	{ R25, "r25", 25 },
	{ R26, "gp", 26 },
	{ R27, "fp", 27 },
	{ R28, "sp", 28 },
	{ R29, "ilink", 29 },
	{ R30, "r30", 30 },
	{ R31, "blink", 31 },
	{ R32, "r32", 32 },
	{ R33, "r33", 33 },
	{ R34, "r34", 34 },
	{ R35, "r35", 35 },
	{ R36, "r36", 36 },
	{ R37, "r37", 37 },
	{ R38, "r38", 38 },
	{ R39, "r39", 39 },
	{ R40, "r40", 40 },
	{ R41, "r41", 41 },
	{ R42, "r42", 42 },
	{ R43, "r43", 43 },
	{ R44, "r44", 44 },
	{ R45, "r45", 45 },
	{ R46, "r46", 46 },
	{ R47, "r47", 47 },
	{ R48, "r48", 48 },
	{ R49, "r49", 49 },
	{ R50, "r50", 50 },
	{ R51, "r51", 51 },
	{ R52, "r52", 52 },
	{ R53, "r53", 53 },
	{ R54, "r54", 54 },
	{ R55, "r55", 55 },
	{ R56, "r56", 56 },
	{ R57, "r57", 57 },
	{ R58, "r58", 58 },
	{ R59, "r59", 59 },
	{ R60, "lp_count", 60 },
	{ R61, "r61", 61 },
	{ R62, "limm", 62 },
	{ R63, "pcl", 63 },
	/* AUX */
	{ ARC_REG_PC, "pc", PC_REG_ADDR },
	{ ARC_REG_STATUS32, "status32", STATUS32_REG_ADDR },
	{ ARC_REG_LP_START, "lp_start",  LP_START_REG_ADDR },
	{ ARC_REG_LP_END,   "lp_end",   LP_END_REG_ADDR },
};

/*static uint32_t aux_regs_addresses[LP_END - PC + 1] = {
	PC_REG_ADDR,
	STATUS32_REG_ADDR,
	LP_START_REG_ADDR,
	LP_END_REG_ADDR,
};*/
#if 0
static char *arc_gdb_reg_names_list[] = {
	/* core regs */
	"r0",   "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
	"r8",   "r9", "r10", "r11", "r12", "r13", "r14", "r15",
	"r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
	"r24", "r25",  "gp",  "fp",  "sp", "ilink", "r30", "blink",
	"r32", "r33", "r34", "r35", "r36", "r37", "r38", "r39",
	"r40", "r41", "r42", "r43", "r44", "r45", "r46", "r47",
	"r48", "r49", "r50", "r51", "r52", "r53", "r54", "r55",
	"r56", "r57", "r58", "r59", "lp_count", "reserved", "limm", "pcl",
	/* aux regs */
	"identity", "pc",  "lp_start",  "lp_end",       "status32", "int_vector_base", "aux_user_sp",
	"status32_p0",  "status32_l2",  "aux_irq_lv12", "aux_irq_lev",
	"aux_irq_hint", "eret",         "erbta",        "erstatus",
	"ecr",          "efa",          "icause1",      "icause2",
	"aux_ienable",  "aux_itrigger", "bta",          "bta_l1",
	"bta_l2",  "aux_irq_pulse_cancel",  "aux_irq_pending", "bcr_ver",
	"bta_link_build", "vecbase_ac_build", "rf_build", "isa_config",
	"dccm_build", "iccm_build",
};

static struct arc32_core_reg
	arc_gdb_reg_list_arch_info[ARC32_NUM_GDB_REGS] = { // + 3
	/* ARC core regs R0 - R26 */
	 {0, NULL, NULL},  {1, NULL, NULL},  {2, NULL, NULL},  {3, NULL, NULL},
	 {4, NULL, NULL},  {5, NULL, NULL},  {6, NULL, NULL},  {7, NULL, NULL},
	 {8, NULL, NULL},  {9, NULL, NULL}, {10, NULL, NULL}, {11, NULL, NULL},
	{12, NULL, NULL}, {13, NULL, NULL}, {14, NULL, NULL}, {15, NULL, NULL},
	{16, NULL, NULL}, {17, NULL, NULL}, {18, NULL, NULL}, {19, NULL, NULL},
	{20, NULL, NULL}, {21, NULL, NULL}, {22, NULL, NULL}, {23, NULL, NULL},
	{24, NULL, NULL}, {25, NULL, NULL}, {26, NULL, NULL}, {27, NULL, NULL},
	{28, NULL, NULL}, {29, NULL, NULL}, {30, NULL, NULL}, {31, NULL, NULL},
	{32, NULL, NULL}, {33, NULL, NULL}, {34, NULL, NULL}, {35, NULL, NULL},
	{36, NULL, NULL}, {37, NULL, NULL}, {38, NULL, NULL}, {39, NULL, NULL},
	{40, NULL, NULL}, {41, NULL, NULL}, {42, NULL, NULL}, {43, NULL, NULL},
	{44, NULL, NULL}, {45, NULL, NULL}, {46, NULL, NULL}, {47, NULL, NULL},
	{48, NULL, NULL}, {49, NULL, NULL}, {50, NULL, NULL}, {51, NULL, NULL},
	{52, NULL, NULL}, {53, NULL, NULL}, {54, NULL, NULL}, {55, NULL, NULL},
	{56, NULL, NULL}, {57, NULL, NULL}, {58, NULL, NULL}, {59, NULL, NULL},
	{60, NULL, NULL}, {61, NULL, NULL}, {62, NULL, NULL}, {63, NULL, NULL},
	/* selection of ARC aux regs */
	{64, NULL, NULL}, {65, NULL, NULL}, {66, NULL, NULL}, {67, NULL, NULL},
	{68, NULL, NULL}, {69, NULL, NULL}, {70, NULL, NULL}, {71, NULL, NULL},
	{72, NULL, NULL}, {73, NULL, NULL}, {74, NULL, NULL}, {75, NULL, NULL},
	{76, NULL, NULL}, {77, NULL, NULL}, {78, NULL, NULL}, {79, NULL, NULL},
	{80, NULL, NULL}, {81, NULL, NULL}, {82, NULL, NULL}, {83, NULL, NULL},
	{84, NULL, NULL}, {85, NULL, NULL}, {86, NULL, NULL}, {87, NULL, NULL},
	{88, NULL, NULL}, {89, NULL, NULL}, {90, NULL, NULL}, {91, NULL, NULL},
	{92, NULL, NULL}, {93, NULL, NULL}, {94, NULL, NULL}, {95, NULL, NULL},
	{96, NULL, NULL},
};

/* arc_regs_(read|write)_registers use this array as a list of AUX registers to
 * perform action on. */
static uint32_t aux_regs_addresses[] = { 
	AUX_IDENTITY_REG,
	AUX_PC_REG,
	AUX_LP_START_REG,
	AUX_LP_END_REG,
	AUX_STATUS32_REG,
	AUX_INT_VECTOR_BASE_REG,
	AUX_USER_SP_REG,
	AUX_STATUS32_L1_REG,
	AUX_STATUS32_L2_REG,
	AUX_IRQ_LV12_REG,
	AUX_IRQ_LEV_REG,
	AUX_IRQ_HINT_REG,
	AUX_ERET_REG,
	AUX_ERBTA_REG,
	AUX_ERSTATUS_REG,
	AUX_ECR_REG,
	AUX_EFA_REG,
	AUX_ICAUSE1_REG,
	AUX_ICAUSE2_REG,
	AUX_IENABLE_REG,
	AUX_ITRIGGER_REG,
	AUX_BTA_REG,
	AUX_BTA_L1_REG,
	AUX_BTA_L2_REG,
	AUX_IRQ_PULSE_CAN_REG,
	AUX_IRQ_PENDING_REG,
	AUX_BCR_VER_REG,
	AUX_BTA_LINK_BUILD_REG,
	AUX_VECBASE_AC_BUILD_REG,
	AUX_RF_BUILD_REG,
	AUX_ISA_CONFIG_REG,
	AUX_DCCM_BUILD_REG,
	AUX_ICCM_BUILD_REG,
};
static unsigned int aux_regs_addresses_count = (sizeof(aux_regs_addresses) / sizeof(uint32_t));
#endif

#if 0
static int arc_regs_get_core_reg(struct reg *reg)
{
	int retval = ERROR_OK;
LOG_DEBUG("Entering get core reg...");
	struct arc32_reg_arch_info *arc32_reg = reg->arch_info;
	struct target *target = arc32_reg->target;
	struct arc32_common *arc32_target = target_to_arc32(target);

	if (target->state != TARGET_HALTED)
		return ERROR_TARGET_NOT_HALTED;

	retval = arc32_target->read_core_reg(target, arc32_reg->desc->regnum);

	return retval;
}
#else
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

	if (regnum == LIMM || regnum == R61) {
		arc_reg->value = 0;
	} else	if (regnum <= LAST_CORE_EXT_REG) {
		arc_jtag_read_core_reg(&arc32->jtag_info, arc_reg->desc->addr, 1, &arc_reg->value);
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
#endif

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
//	reg->dirty = 1;
//	reg->valid = 1;

//	uint32_t reg_value;
//	LOG_DEBUG("Entering write core reg");

	/* get pointers to arch-specific information */

//	if ((num < 0) || (num >= TOTAL_NUM_REGS))
//		return ERROR_COMMAND_SYNTAX_ERROR;

	//reg_value = buf_get_u32(arc32->core_cache->reg_list[num].value, 0, 32);
	//arc32->core_regs[num] = reg_value;
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
#if 0
	int num_regs = ARC32_NUM_GDB_REGS;
	int i;
#else
	uint32_t i;
#endif

	/* get pointers to arch-specific information */
	struct arc32_common *arc32 = target_to_arc32(target);
	struct reg_cache **cache_p = register_get_last_cache_p(&target->reg_cache);
	struct reg_cache *cache = malloc(sizeof(struct reg_cache));
	struct reg *reg_list = calloc(ARC_TOTAL_NUM_REGS, sizeof(struct reg));
#if 0
	struct arc32_core_reg *arch_info =
		malloc(sizeof(struct arc32_core_reg) * num_regs);
#else
	struct arc_reg_t *arch_info = calloc(ARC_TOTAL_NUM_REGS, sizeof(struct arc_reg_t));
#endif

	/* Build the process context cache */
	cache->name = "arc32 registers";
	cache->next = NULL;
	cache->reg_list = reg_list;
#if 0
	cache->num_regs = num_regs;
#else
	cache->num_regs = ARC_TOTAL_NUM_REGS;
#endif
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
#if 0
		arch_info[i] = arc_gdb_reg_list_arch_info[i];
#else
		arch_info[i].desc = arc32_regs_descriptions + i;
#endif
		arch_info[i].target = target;
		arch_info[i].arc32_common = arc32;
#if 0
		reg_list[i].name = arc_gdb_reg_names_list[i];
#else
		reg_list[i].name = arc32_regs_descriptions[i].name;
#endif
		reg_list[i].size = 32;
		reg_list[i].value = calloc(1, 4);
		reg_list[i].dirty = 0;
		reg_list[i].valid = 0;
		reg_list[i].type = &arc32_reg_type;
		reg_list[i].arch_info = &arch_info[i];
		// XML
#if 0
		reg_list[i].number = i;
		reg_list[i].exist = 1;
		if (i < 96)
			reg_list[i].group = general_group_name;
		else
			reg_list[i].group = float_group_name;
#else
		reg_list[i].number = arc32_regs_descriptions[i].regnum;
		reg_list[i].exist = 1;
		reg_list[i].group = general_group_name;
		reg_list[i].caller_save = true;
#endif

#if 0
		if (i < 26) {
			reg_list[i].feature = core_basecase;
		} else if (i < 29) {
			reg_list[i].feature = core_pointers;
		} else if (i < 32) {
			reg_list[i].feature = core_link;
		} else if (i < 60) {
			reg_list[i].feature = core_extension;
		} else if (i == 60 || i == 62 || i == 63) {
			reg_list[i].feature = core_other;
		} else {
			reg_list[i].feature = aux_baseline;
		}
#else
		if (i < GP)
			reg_list[i].feature = core_basecase;
		else if (GP <= i && i < ILINK)
			reg_list[i].feature = core_pointers;
		else if (ILINK <= i && i < R32)
			reg_list[i].feature = core_link;
		else if ((R32 <= i && i < R60) || i == R61 || i == R62) {
			reg_list[i].feature = core_extension;
			reg_list[i].exist = false;
		} else if (R60 <= i && i <= R63)
			reg_list[i].feature = core_other;
		else if (ARC_REG_PC <= i && i <= ARC_REG_LP_END)
			reg_list[i].feature = aux_baseline;
		else {
			LOG_WARNING("Unknown register with number %" PRIu32, i);
			reg_list[i].feature = NULL;
		}

		/* Temporary hack until proper detection of registers is implemented. */
		if (i > ARC_REG_LAST_GDB_GENERAL) {
			reg_list[i].exist = false;
		} else {
			LOG_DEBUG("reg n=%3i name=%3s group=%s feature=%s", i,
					reg_list[i].name, reg_list[i].group,
					reg_list[i].feature->name); }

#endif
		// end XML
	}

	return cache;
}

#if 0
int arc_regs_read_core_reg(struct target *target, int num)
{
	int retval = ERROR_OK;
	uint32_t reg_value;
LOG_DEBUG("Entering read core reg");
	/* get pointers to arch-specific information */
	struct arc32_common *arc32 = target_to_arc32(target);

#if 0
	if ((num < 0) || (num >= ARC32_NUM_GDB_REGS))
#else
	if ((num < 0) || (num >= TOTAL_NUM_REGS))
#endif
		return ERROR_COMMAND_SYNTAX_ERROR;

	reg_value = arc32->core_regs[num];
	buf_set_u32(arc32->core_cache->reg_list[num].value, 0, 32, reg_value);
	LOG_DEBUG("read core reg %02i value 0x%08" PRIx32, num , reg_value);
	arc32->core_cache->reg_list[num].valid = 1;
	arc32->core_cache->reg_list[num].dirty = 0;

	return retval;
}

int arc_regs_write_core_reg(struct target *target, int num)
{
	int retval = ERROR_OK;
	uint32_t reg_value;
LOG_DEBUG("Entering write core reg");

	/* get pointers to arch-specific information */
	struct arc32_common *arc32 = target_to_arc32(target);

	if ((num < 0) || (num >= TOTAL_NUM_REGS))
		return ERROR_COMMAND_SYNTAX_ERROR;

	reg_value = buf_get_u32(arc32->core_cache->reg_list[num].value, 0, 32);
	arc32->core_regs[num] = reg_value;
	LOG_DEBUG("write core reg %02i value 0x%08" PRIx32, num , reg_value);
	arc32->core_cache->reg_list[num].valid = 1;
	arc32->core_cache->reg_list[num].dirty = 0;

	return retval;
}

int arc_regs_read_registers(struct target *target, uint32_t *regs)
{
	int retval = ERROR_OK;
	struct arc32_common *arc32 = target_to_arc32(target);
	uint32_t reg_value = 0xdeadbeef;

	/* This will take a while, so calsl to keep_alive() are required. */
	keep_alive();

	/*
	 * read core registers:
	 *	R0-R31 for low-end cores
	 *	R0-R60 for high-end cores
	 * gdb requests:
	 *       regmap[0]  = (&(pt_buf.scratch.r0)
	 *       ...
	 *       regmap[60] = (&(pt_buf.scratch.r60)
	 */
	if (arc32->processor_type == ARCEM_NUM) {
		arc_jtag_read_core_reg(&arc32->jtag_info, 0, CORE_NUM_REGS, regs);
	} else {
#if 0
		/* Note that NUM_CORE_REGS is a number of registers but LAST_EXTENSION_REG
		 * is a _register number_ so it have to be incremented. */
		arc_jtag_read_core_reg(&arc32->jtag_info, 0, ARC32_LAST_EXTENSION_REG + 1, regs);
		arc_jtag_read_core_reg(&arc32->jtag_info, LP_COUNT_REG, 1, regs + LP_COUNT_REG);
		arc_jtag_read_core_reg(&arc32->jtag_info, LIDI_REG, 1, regs + LIDI_REG);
#else
		LOG_ERROR("we shouldn't be here");
		assert(false);
#endif
	}
	arc_jtag_read_core_reg(&arc32->jtag_info, arc32_regs_descriptions[LP_COUNT].addr, 1, regs + LP_COUNT);
	arc_jtag_read_core_reg(&arc32->jtag_info, arc32_regs_descriptions[PCL].addr, 1, regs + PCL);

	/*
	 * we have here a hole of 2 regs, they are 0 on the other end (GDB)
	 *   aux-status & aux-semaphore are not transfered
	 */

	/* read auxiliary registers */
	keep_alive();
	//arc_jtag_read_aux_reg(&arc32->jtag_info, aux_regs_addresses, ARRAY_SIZE(aux_regs_addresses), regs + FIRST_AUX_REG);

	keep_alive();
	return retval;
}

int arc_regs_write_registers(struct target *target, uint32_t *regs)
{
	/* This function takes around 2 seconds to finish in wall clock time.
	 * To avoid annoing OpenOCD warnings about uncalled keep_alive() we need
	 * to insert calls to it. I'm wondering whether there is no problems
	 * with JTAG implementation here, because really it would be much better
	 * to write registers faster, instead of adding calls to keep_alive().
	 * Writing one register takes around 40-50ms. May be they can be written
	 * in a batch?
	 *
	 * I would like to see real resoultion of this problem instead of
	 * inserted calls to keep_alive just to keep OpenOCD happy.  */
	int retval = ERROR_OK;
	struct arc32_common *arc32 = target_to_arc32(target);

	/* This will take a while, so calsl to keep_alive() are required. */
	keep_alive();

	/*
	 * write core registers R0-Rx (see above read_registers() !)
	 */
	if (arc32->processor_type == ARCEM_NUM) {
		arc_jtag_write_core_reg(&arc32->jtag_info, 0, CORE_NUM_REGS, regs);
	} else {
#if 0
		arc_jtag_write_core_reg(&arc32->jtag_info, 0, ARC32_LAST_EXTENSION_REG + 1, regs);
		arc_jtag_write_core_reg(&arc32->jtag_info, LP_COUNT_REG, 1, regs + LP_COUNT_REG);
		arc_jtag_write_core_reg(&arc32->jtag_info, LIDI_REG,     1, regs + LIDI_REG);
#else
		LOG_ERROR("We shouldn't be here");
		assert(false);
#endif
	}
	arc_jtag_write_core_reg(&arc32->jtag_info, arc32_regs_descriptions[LP_COUNT].addr, 1, regs + LP_COUNT);
	arc_jtag_write_core_reg(&arc32->jtag_info, arc32_regs_descriptions[PCL].addr, 1, regs + PCL);

	/* Write auxiliary registers */
	keep_alive();
	arc_jtag_write_aux_reg(&arc32->jtag_info, aux_regs_addresses, ARRAY_SIZE(aux_regs_addresses), regs + 64);

	keep_alive();
	return retval;
}
#endif

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
			if (i <= ARC_REG_LAST_GDB_GENERAL && arc32->core_cache->reg_list[i].exist){
				(*reg_list)[cur_index] = &arc32->core_cache->reg_list[i];
				cur_index += 1;
			}
		}
		*reg_list_size = cur_index;
		LOG_DEBUG("REG_CLASS_GENERAL: number of regs=%i", *reg_list_size);
	}

	return retval;
}

int arc_regs_print_core_registers(struct target *target)
{
	int retval = ERROR_OK;
#if 0
	int reg_nbr;
	uint32_t value;

	struct arc32_common *arc32 = target_to_arc32(target);

	LOG_USER("ARC core register display.");

	for(reg_nbr = 0; reg_nbr < ARC32_NUM_CORE_REGS; reg_nbr++) {
		arc_jtag_read_core_reg(&arc32->jtag_info, reg_nbr, 1, &value);
		LOG_USER_N(" R%02i: 0x%08" PRIx32, reg_nbr, value);

		if(reg_nbr ==  3 || reg_nbr ==  7 || reg_nbr == 11 || reg_nbr == 15 ||
		   reg_nbr == 19 || reg_nbr == 23 || reg_nbr == 27 || reg_nbr == 31 )
			LOG_USER(" "); /* give newline */
	}

	/* do not read extension+ registers for low-end cores */
	if (arc32->processor_type != ARCEM_NUM) {
		for(reg_nbr = ARC32_NUM_CORE_REGS; reg_nbr <= PCL_REG; reg_nbr++) {
			arc_jtag_read_core_reg(&arc32->jtag_info, reg_nbr, 1, &value);
			LOG_USER_N(" R%02i: 0x%08" PRIx32, reg_nbr, value);

			if(reg_nbr == 35 || reg_nbr == 39 || reg_nbr == 43 || reg_nbr == 47 ||
			   reg_nbr == 51 || reg_nbr == 55 || reg_nbr == 59 || reg_nbr == 63 )
				LOG_USER(" "); /* give newline */
		}
	}
#endif
	LOG_USER(" "); /* give newline */

	return retval;
}

int arc_regs_print_aux_registers(struct arc_jtag *jtag_info)
{
	int retval = ERROR_OK;
	uint32_t value;

	LOG_USER("ARC auxiliary register display.");

	arc_jtag_read_aux_reg_one(jtag_info, AUX_STATUS_REG, &value);
	LOG_USER(" STATUS:          0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_STATUS_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_SEMAPHORE_REG, &value);
	LOG_USER(" SEMAPHORE:       0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_SEMAPHORE_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_LP_START_REG, &value);
	LOG_USER(" LP START:        0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_LP_START_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_LP_END_REG, &value);
	LOG_USER(" LP END:          0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_LP_END_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_IDENTITY_REG, &value);
	LOG_USER(" IDENTITY:        0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_IDENTITY_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_DEBUG_REG, &value);
	LOG_USER(" DEBUG:           0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_DEBUG_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_PC_REG, &value);
	LOG_USER(" PC:              0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_PC_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_STATUS32_REG, &value);
	LOG_USER(" STATUS32:        0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_STATUS32_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_STATUS32_L1_REG, &value);
	LOG_USER(" STATUS32 L1:     0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_STATUS32_L1_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_STATUS32_L2_REG, &value);
	LOG_USER(" STATUS32 L2:     0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_STATUS32_L2_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_COUNT0_REG, &value);
	LOG_USER(" COUNT0:          0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_COUNT0_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_CONTROL0_REG, &value);
	LOG_USER(" CONTROL0:        0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_CONTROL0_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_LIMIT0_REG, &value);
	LOG_USER(" LIMIT0:          0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_LIMIT0_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_INT_VECTOR_BASE_REG, &value);
	LOG_USER(" INT VECTOR BASE: 0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_INT_VECTOR_BASE_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_MACMODE_REG, &value);
	LOG_USER(" MACMODE:         0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_MACMODE_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_IRQ_LV12_REG, &value);
	LOG_USER(" IRQ LV12:        0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_IRQ_LV12_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_COUNT1_REG, &value);
	LOG_USER(" COUNT1:          0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_COUNT1_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_CONTROL1_REG, &value);
	LOG_USER(" CONTROL1:        0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_CONTROL1_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_LIMIT1_REG, &value);
	LOG_USER(" LIMIT1:          0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_LIMIT1_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_IRQ_LEV_REG, &value);
	LOG_USER(" IRQ LEV:         0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_IRQ_LEV_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_IRQ_HINT_REG, &value);
	LOG_USER(" IRQ HINT:        0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_IRQ_HINT_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_ERET_REG, &value);
	LOG_USER(" ERET:            0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_ERET_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_ERBTA_REG, &value);
	LOG_USER(" ERBTA:           0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_ERBTA_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_ERSTATUS_REG, &value);
	LOG_USER(" ERSTATUS:        0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_ERSTATUS_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_ECR_REG, &value);
	LOG_USER(" ECR:             0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_ECR_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_EFA_REG, &value);
	LOG_USER(" EFA:             0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_EFA_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_ICAUSE1_REG, &value);
	LOG_USER(" ICAUSE1:         0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_ICAUSE1_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_ICAUSE2_REG, &value);
	LOG_USER(" ICAUSE2:         0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_ICAUSE2_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_IENABLE_REG, &value);
	LOG_USER(" IENABLE:         0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_IENABLE_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_ITRIGGER_REG, &value);
	LOG_USER(" ITRIGGER:        0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_ITRIGGER_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_XPU_REG, &value);
	LOG_USER(" XPU:             0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_XPU_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_BTA_REG, &value);
	LOG_USER(" BTA:             0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_BTA_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_BTA_L1_REG, &value);
	LOG_USER(" BTA L1:          0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_BTA_L1_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_BTA_L2_REG, &value);
	LOG_USER(" BTA L2:          0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_BTA_L2_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_IRQ_PULSE_CAN_REG, &value);
	LOG_USER(" IRQ PULSE CAN:   0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_IRQ_PULSE_CAN_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_IRQ_PENDING_REG, &value);
	LOG_USER(" IRQ PENDING:     0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_IRQ_PENDING_REG);
	arc_jtag_read_aux_reg_one(jtag_info, AUX_XFLAGS_REG, &value);
	LOG_USER(" XFLAGS:          0x%08" PRIx32 "\t(@:0x%08" PRIx32 ")", value, AUX_XFLAGS_REG);
	LOG_USER(" ");

	return retval;
}
