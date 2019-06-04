/***************************************************************************
 *   Copyright (C) 2013-2015 Synopsys, Inc.                                *
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

#include "arc32.h"

/* Initialize arc32_common structure, which passes to openocd target instance */
int arc32_init_arch_info(struct target *target, struct arc32_common *arc32,
	struct jtag_tap *tap)
{
	arc32->common_magic = ARC32_COMMON_MAGIC;
	target->arch_info = arc32;

	arc32->fast_data_area = NULL;

	/* TODO: uncomment this as jtag functionality as introduced */
/*
	arc32->jtag_info.tap = tap;
	arc32->jtag_info.scann_size = 4;
	arc32->jtag_info.always_check_status_rd = false;
	arc32->jtag_info.check_status_fl = false;
	arc32->jtag_info.wait_until_write_finished = false;
*/
	/* has breakpoint/watchpoint unit been scanned */
	arc32->bp_scanned = 0;

	/* We don't know how many actionpoints are in the core yet. */
	arc32->actionpoints_num_avail = 0;
	arc32->actionpoints_num = 0;
	arc32->actionpoints_list = NULL;

	/* Flush D$ by default. It is safe to assume that D$ is present,
	 * because if it isn't, there will be no error, just a slight
	 * performance penalty from unnecessary JTAG operations. */
	arc32->has_dcache = true;

	/* TODO: uncomment this as this function be introduced */
	//arc32_reset_caches_states(target);

	/* TODO: uncomment this as register functionality as introduced */
	/* Add standard GDB data types */
	/*
	INIT_LIST_HEAD(&arc32->reg_data_types);
	struct arc_reg_data_type *std_types = calloc(ARRAY_SIZE(standard_gdb_types),
			sizeof(struct arc_reg_data_type));
	if (!std_types) {
		LOG_ERROR("Cannot allocate memory");
		return ERROR_FAIL;
	}
	for (unsigned int i = 0; i < ARRAY_SIZE(standard_gdb_types); i++) {
		std_types[i].data_type.type = standard_gdb_types[i].type;
		std_types[i].data_type.id = standard_gdb_types[i].id;
		arc32_add_reg_data_type(target, &(std_types[i]));
	}
	*/

	/* Fields related to target descriptions */
	INIT_LIST_HEAD(&arc32->core_reg_descriptions);
	INIT_LIST_HEAD(&arc32->aux_reg_descriptions);
	INIT_LIST_HEAD(&arc32->bcr_reg_descriptions);
	arc32->num_regs = 0;
	arc32->num_core_regs = 0;
	arc32->num_aux_regs = 0;
	arc32->num_bcr_regs = 0;
	arc32->last_general_reg = ULONG_MAX;
	arc32->pc_index_in_cache = ULONG_MAX;
	arc32->debug_index_in_cache = ULONG_MAX;

	return ERROR_OK;
}
