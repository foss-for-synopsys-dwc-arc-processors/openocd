/***************************************************************************
 *   Copyright (C) 2013-2015,2019 Synopsys, Inc.                           *
 *   Frank Dols <frank.dols@synopsys.com>                                  *
 *   Mischa Jonker <mischa.jonker@synopsys.com>                            *
 *   Anton Kolesov <anton.kolesov@synopsys.com>                            *
 *   Evgeniy Didin <didin@synopsys.com>
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

void arc_add_reg_data_type(struct target *target,
               struct arc_reg_data_type *data_type)
{
       LOG_DEBUG("-");
       struct arc_common *arc = target_to_arc(target);
       assert(arc);

       list_add_tail(&data_type->list, &arc->reg_data_types);
}


/* Initialize arc_common structure, which passes to openocd target instance */
int arc_init_arch_info(struct target *target, struct arc_common *arc,
	struct jtag_tap *tap)
{
	arc->common_magic = ARC_COMMON_MAGIC;
	target->arch_info = arc;

	arc->fast_data_area = NULL;

	arc->jtag_info.tap = tap;
	arc->jtag_info.scann_size = 4;

	/* has breakpoint/watchpoint unit been scanned */
	arc->bp_scanned = 0;

	/* We don't know how many actionpoints are in the core yet. */
	arc->actionpoints_num_avail = 0;
	arc->actionpoints_num = 0;
	arc->actionpoints_list = NULL;

	/* Flush D$ by default. It is safe to assume that D$ is present,
	 * because if it isn't, there will be no error, just a slight
	 * performance penalty from unnecessary JTAG operations. */
	arc->has_dcache = true;

	/* TODO: uncomment this as this function be introduced */
	//arc_reset_caches_states(target);

        /* Add standard GDB data types */
        INIT_LIST_HEAD(&arc->reg_data_types);
        struct arc_reg_data_type *std_types = calloc(ARRAY_SIZE(standard_gdb_types),
                        sizeof(struct arc_reg_data_type));
        if (!std_types) {
                LOG_ERROR("Cannot allocate memory");
                return ERROR_FAIL;
        }
        for (unsigned int i = 0; i < ARRAY_SIZE(standard_gdb_types); i++) {
                std_types[i].data_type.type = standard_gdb_types[i].type;
                std_types[i].data_type.id = standard_gdb_types[i].id;
                arc_add_reg_data_type(target, &(std_types[i]));
        }


	/* Fields related to target descriptions */
	INIT_LIST_HEAD(&arc->core_reg_descriptions);
	INIT_LIST_HEAD(&arc->aux_reg_descriptions);
	INIT_LIST_HEAD(&arc->bcr_reg_descriptions);
	arc->num_regs = 0;
	arc->num_core_regs = 0;
	arc->num_aux_regs = 0;
	arc->num_bcr_regs = 0;
	arc->last_general_reg = ULONG_MAX;
	arc->pc_index_in_cache = ULONG_MAX;
	arc->debug_index_in_cache = ULONG_MAX;

	return ERROR_OK;
}

int arc_add_reg(struct target *target, struct arc_reg_desc *arc_reg,
		const char * const type_name, const size_t type_name_len)
{
	assert(target);
	assert(arc_reg);

	struct arc_common *arc = target_to_arc(target);
	assert(arc);

	/* Find register type */
	{
		struct arc_reg_data_type *type;
		list_for_each_entry(type, &arc->reg_data_types, list) {
			if (strncmp(type->data_type.id, type_name, type_name_len) == 0) {
				arc_reg->data_type = &(type->data_type);
				break;
			}
		}
		if (!arc_reg->data_type) {
			return ERROR_ARC_REGTYPE_NOT_FOUND;
		}
	}

	if (arc_reg->is_core) {
		list_add_tail(&arc_reg->list, &arc->core_reg_descriptions);
		arc->num_core_regs += 1;
	} else if (arc_reg->is_bcr) {
		list_add_tail(&arc_reg->list, &arc->bcr_reg_descriptions);
		arc->num_bcr_regs += 1;
	} else {
		list_add_tail(&arc_reg->list, &arc->aux_reg_descriptions);
		arc->num_aux_regs += 1;
	}
	arc->num_regs += 1;

	LOG_DEBUG(
			"added register {name=%s, num=0x%x, type=%s%s%s%s}",
			arc_reg->name, arc_reg->arch_num, arc_reg->data_type->id,
			arc_reg->is_core ? ", core" : "",  arc_reg->is_bcr ? ", bcr" : "",
			arc_reg->is_general ? ", general" : ""
		);

	return ERROR_OK;
}


/* Common code to initialize `struct reg` for different registers: core, aux, bcr. */
static void arc_init_reg(
		struct target *target,
		struct reg *reg,
		struct arc_reg_t *arc_reg,
		struct arc_reg_desc *reg_desc,
		unsigned long number)
{
	assert(target);
	assert(reg);
	assert(arc_reg);
	assert(reg_desc);

	struct arc_common *arc = target_to_arc(target);

	/* Initialize struct arc_reg_t */
	arc_reg->desc = reg_desc;
	arc_reg->target = target;
	arc_reg->arc_common = arc;

	/* Initialize struct reg */
	reg->name = reg_desc->name;
	reg->size = 32; /* All register in ARC are 32-bit */
	reg->value = calloc(1, 4);
	reg->dirty = 0;
	reg->valid = 0;
	reg->type = &arc_reg_type;
	reg->arch_info = arc_reg;
	reg->exist = false;
	reg->caller_save = true; /* @todo should be configurable. */
	reg->reg_data_type = reg_desc->data_type;

	reg->feature = calloc(1, sizeof(struct reg_feature));
	reg->feature->name = reg_desc->gdb_xml_feature;

	/* reg->number is used by OpenOCD as value for @regnum. Thus when setting
	 * value of a register GDB will use it as a number of register in
	 * P-packet. OpenOCD gdbserver will then use number of register in
	 * P-packet as an array index in the reg_list returned by
	 * arc_regs_get_gdb_reg_list. So to ensure that registers are assigned
	 * correctly it would be required to either sort registers in
	 * arc_regs_get_gdb_reg_list or to assign numbers sequentially here and
	 * according to how registers will be sorted in
	 * arc_regs_get_gdb_reg_list. Second options is much more simpler. */
	reg->number = number;

	if (reg_desc->is_general) {
		arc->last_general_reg = reg->number;
		reg->group = reg_group_general;
	} else {
		reg->group = reg_group_other;
	}
}

int arc_build_reg_cache(struct target *target)
{
	/* get pointers to arch-specific information */
	struct arc_common *arc = target_to_arc(target);
	const unsigned long num_regs = arc->num_core_regs + arc->num_aux_regs;
	struct reg_cache **cache_p = register_get_last_cache_p(&target->reg_cache);
	struct reg_cache *cache = calloc(1, sizeof(struct reg_cache));
	struct reg *reg_list = calloc(num_regs, sizeof(struct reg));
	struct arc_reg_t *reg_arch_info = calloc(num_regs, sizeof(struct arc_reg_t));

	/* Build the process context cache */
	cache->name = "arc registers";
	cache->next = NULL;
	cache->reg_list = reg_list;
	cache->num_regs = num_regs;
	arc->core_cache = cache;
	(*cache_p) = cache;

	struct arc_reg_desc *reg_desc;
	unsigned long i = 0;
	list_for_each_entry(reg_desc, &arc->core_reg_descriptions, list) {
		arc_init_reg(target, &reg_list[i], &reg_arch_info[i], reg_desc, i);

		LOG_DEBUG("reg n=%3li name=%3s group=%s feature=%s", i,
			reg_list[i].name, reg_list[i].group,
			reg_list[i].feature->name);

		i += 1;
	}

	list_for_each_entry(reg_desc, &arc->aux_reg_descriptions, list) {
		arc_init_reg(target, &reg_list[i], &reg_arch_info[i], reg_desc, i);

		LOG_DEBUG("reg n=%3li name=%3s group=%s feature=%s", i,
			reg_list[i].name, reg_list[i].group,
			reg_list[i].feature->name);

		/* PC and DEBUG are essential so we search for them. */
		if (arc->pc_index_in_cache == ULONG_MAX && strcmp("pc", reg_desc->name) == 0)
			arc->pc_index_in_cache = i;
		else if (arc->debug_index_in_cache == ULONG_MAX
				&& strcmp("debug", reg_desc->name) == 0)
			arc->debug_index_in_cache = i;

		i += 1;
	}

	if (arc->pc_index_in_cache == ULONG_MAX
			|| arc->debug_index_in_cache == ULONG_MAX) {
		LOG_ERROR("`pc' and `debug' registers must be present in target description.");
		return ERROR_FAIL;
	}

	assert(i == (arc->num_core_regs + arc->num_aux_regs));

	return ERROR_OK;
}

/* This function must be called only after arc_build_reg_cache */
int arc_build_bcr_reg_cache(struct target *target)
{
	/* get pointers to arch-specific information */
	struct arc_common *arc = target_to_arc(target);
	const unsigned long num_regs = arc->num_bcr_regs;
	struct reg_cache **cache_p = register_get_last_cache_p(&target->reg_cache);
	struct reg_cache *cache = malloc(sizeof(struct reg_cache));
	struct reg *reg_list = calloc(num_regs, sizeof(struct reg));
	struct arc_reg_t *reg_arch_info = calloc(num_regs, sizeof(struct arc_reg_t));


	/* Build the process context cache */
	cache->name = "arc.bcr";
	cache->next = NULL;
	cache->reg_list = reg_list;
	cache->num_regs = num_regs;
	(*cache_p) = cache;


	struct arc_reg_desc *reg_desc;
	unsigned long i = 0;
	unsigned long gdb_regnum = arc->core_cache->num_regs;

	list_for_each_entry(reg_desc, &arc->bcr_reg_descriptions, list) {
		arc_init_reg(target, &reg_list[i], &reg_arch_info[i], reg_desc, gdb_regnum);
		/* BCRs always semantically, they are just read-as-zero, if there is
		 * not real register. */
		reg_list[i].exist = true;

		LOG_DEBUG("reg n=%3li name=%3s group=%s feature=%s", i,
			reg_list[i].name, reg_list[i].group,
			reg_list[i].feature->name);
		i += 1;
		gdb_regnum += 1;
	}

	assert(i == arc->num_bcr_regs);

	return ERROR_OK;
}
