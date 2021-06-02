/***************************************************************************
 *   Copyright (C) 2013-2015 Synopsys, Inc.                                *
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

#include "arc32.h"

/* ----- Supporting functions ---------------------------------------------- */

static int arc_ocd_init_arch_info(struct target *target,
	struct arc_common *arc, struct jtag_tap *tap)
{
	struct arc32_common *arc32 = &arc->arc32;

	LOG_DEBUG("Entering");

	arc->common_magic = ARC_COMMON_MAGIC;

	/* initialize arc specific info */
	CHECK_RETVAL(arc32_init_arch_info(target, arc32, tap));
	arc32->arch_info = arc;

	return ERROR_OK;
}

/* ----- Exported functions ------------------------------------------------ */

int arc_ocd_poll(struct target *target)
{
	uint32_t status, value;
	struct arc32_common *arc32 = target_to_arc32(target);

	/* gdb calls continuously through this arc_poll() function  */
	CHECK_RETVAL(arc_jtag_status(&arc32->jtag_info, &status));

	/* check for processor halted */
	if (status & ARC_JTAG_STAT_RU) {
		if (target->state != TARGET_RUNNING){
			LOG_WARNING("target is still running!");
			target->state = TARGET_RUNNING;
		}
	} else {
		/* In some cases JTAG status register indicates that
		*  processor is in halt mode, but processor is still running.
		*  We check halt bit of AUX STATUS32 register for setting correct state
		*/
		if ((target->state == TARGET_RUNNING) || (target->state == TARGET_RESET)) {
			CHECK_RETVAL(arc_jtag_read_aux_reg_one(&arc32->jtag_info, AUX_STATUS32_REG, &value));
			if (value & SET_CORE_HALT_BIT) {
				LOG_DEBUG("ARC core in halt or reset state.");

				target->state = TARGET_HALTED;
				CHECK_RETVAL(arc_dbg_debug_entry(target));
				target_call_event_callbacks(target, TARGET_EVENT_HALTED);
			}
			else {
				 LOG_DEBUG("Discrepancy of STATUS32[0] HALT bit and ARC_JTAG_STAT_RU, target is still running");
			}
		} else if (target->state == TARGET_DEBUG_RUNNING) {

			target->state = TARGET_HALTED;
			LOG_DEBUG("ARC core is in debug running mode");

			CHECK_RETVAL(arc_dbg_debug_entry(target));

			target_call_event_callbacks(target, TARGET_EVENT_DEBUG_HALTED);
		}
	}

	return ERROR_OK;
}

/* ......................................................................... */

int arc_ocd_assert_reset(struct target *target)
{
	struct arc32_common *arc32 = target_to_arc32(target);

	LOG_DEBUG("target->state: %s", target_state_name(target));

	enum reset_types jtag_reset_config = jtag_get_reset_config();

	if (target_has_event_action(target, TARGET_EVENT_RESET_ASSERT)) {
		/* allow scripts to override the reset event */

		target_handle_event(target, TARGET_EVENT_RESET_ASSERT);
		register_cache_invalidate(arc32->core_cache);
		/* An ARC target might be in halt state after reset, so
		 * if script requested processor to resume, then it must
		 * be manually started to ensure that this request
		 * is satisfied. */
		if (target->state == TARGET_HALTED && !target->reset_halt) {
			/* Resume the target and continue from the current
			 * PC register value. */
			LOG_DEBUG("Starting CPU execution after reset");
			CHECK_RETVAL(target_resume(target, 1, 0, 0, 0));
		}
		target->state = TARGET_RESET;

		return ERROR_OK;
	}

	/* some cores support connecting while srst is asserted
	 * use that mode is it has been configured */

	bool srst_asserted = false;

	if (!(jtag_reset_config & RESET_SRST_PULLS_TRST) &&
			(jtag_reset_config & RESET_SRST_NO_GATING)) {
		jtag_add_reset(0, 1);
		srst_asserted = true;
	}

	if (jtag_reset_config & RESET_HAS_SRST) {
		/* should issue a srst only, but we may have to assert trst as well */
		if (jtag_reset_config & RESET_SRST_PULLS_TRST)
			jtag_add_reset(1, 1);
		else if (!srst_asserted)
			jtag_add_reset(0, 1);
	}

	target->state = TARGET_RESET;
	jtag_add_sleep(50000);

	register_cache_invalidate(arc32->core_cache);

	if (target->reset_halt)
		CHECK_RETVAL(target_halt(target));

	return ERROR_OK;
}

int arc_ocd_deassert_reset(struct target *target)
{
	LOG_DEBUG("target->state: %s", target_state_name(target));

	/* deassert reset lines */
	jtag_add_reset(0, 0);

	return ERROR_OK;
}

/* ......................................................................... */

int arc_ocd_target_create(struct target *target, Jim_Interp *interp)
{
	struct arc_common *arc = calloc(1, sizeof(struct arc_common));

	CHECK_RETVAL(arc_ocd_init_arch_info(target, arc, target->tap));

	return ERROR_OK;
}

int arc_ocd_arc600_target_create(struct target *target, Jim_Interp *interp)
{
	struct arc_common *arc = calloc(1, sizeof(struct arc_common));

	arc->arc32.has_debug_ss = true;
	arc->arc32.on_step_reset_debug_ra = true;

	CHECK_RETVAL(arc_ocd_init_arch_info(target, arc, target->tap));

	return ERROR_OK;
}

int arc_ocd_init_target(struct command_context *cmd_ctx, struct target *target)
{
	CHECK_RETVAL(arc32_build_reg_cache(target));
	CHECK_RETVAL(arc32_build_bcr_reg_cache(target));
	return ERROR_OK;
}

int arc_ocd_examine(struct target *target)
{
	uint32_t status;
	struct arc32_common *arc32 = target_to_arc32(target);

	LOG_DEBUG("-");
	CHECK_RETVAL(arc_jtag_startup(&arc32->jtag_info));

	if (!target_was_examined(target)) {
		CHECK_RETVAL(arc_jtag_status(&arc32->jtag_info, &status));
		if (status & ARC_JTAG_STAT_RU) {
			target->state = TARGET_RUNNING;
		} else {
			/* It is first time we examine the target, it is halted
			 * and we don't know why. Let's set debug reason,
			 * otherwise OpenOCD will complain that reason is
			 * unknown. */
			if (target->state == TARGET_UNKNOWN)
				target->debug_reason = DBG_REASON_DBGRQ;
			target->state = TARGET_HALTED;
		}

		/* Read BCRs and configure optional registers. */
		CHECK_RETVAL(arc32_configure(target));

		target_set_examined(target);
	}

	return ERROR_OK;
}
