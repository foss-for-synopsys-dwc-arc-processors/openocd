/***************************************************************************
 *   Copyright (C) 2013-2014,2019 Synopsys, Inc.                           *
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

/* ----- Supporting functions ---------------------------------------------- */

typedef enum arc_jtag_reg_type {
	ARC_JTAG_CORE_REG,
	ARC_JTAG_AUX_REG
} reg_type_t;

/* Declare functions */
static void arc_jtag_write_ir(struct arc_jtag *jtag_info, uint32_t
	new_instr);
static void arc_jtag_read_dr(struct arc_jtag *jtag_info, uint8_t *data,
	tap_state_t end_state);
static void arc_jtag_write_dr(struct arc_jtag *jtag_info, uint32_t data,
	tap_state_t end_state);

static void arc_jtag_set_transaction(struct arc_jtag *jtag_info,
	arc_jtag_transaction_t new_trans, tap_state_t end_state);
static void arc_jtag_reset_transaction(struct arc_jtag *jtag_info);

/**
 * This functions sets instruction register in TAP. TAP end state is always
 * IRPAUSE.
 *
 * @param jtag_info
 * @param new_instr	Instruction to write to instruction register.
 */
static void arc_jtag_write_ir(struct arc_jtag *jtag_info, uint32_t
		new_instr)
{
	assert(jtag_info != NULL);
	assert(jtag_info->tap != NULL);

	struct jtag_tap *tap = jtag_info->tap;

	/* Set end state */
	jtag_info->tap_end_state = TAP_IRPAUSE;

	/* Do not set instruction if it is the same as current. */
	uint32_t current_instr = buf_get_u32(tap->cur_instr, 0, tap->ir_length);
	if (current_instr == new_instr)
		return;

	/* Create scan field to output new instruction. */
	struct scan_field field;
	uint8_t instr_buffer[4];
	field.num_bits = tap->ir_length;
	field.in_value = NULL;
	buf_set_u32(instr_buffer, 0, field.num_bits, new_instr);
	field.out_value = instr_buffer;

	/* From code in src/jtag/drivers/driver.c it look like that fields are
	 * copied so it is OK that field in this function is allocated in stack and
	 * thus this memory will be repurposed before jtag_execute_queue() will be
	 * invoked. */
	jtag_add_ir_scan(tap, &field, jtag_info->tap_end_state);
	exit(1);

}

/**
 * Read 4-byte word from data register.
 *
 * Unlike arc_jtag_write_data, this function returns byte-buffer, caller must
 * convert this data to required format himself. This is done, because it is
 * impossible to convert data before jtag_execute_queue() is invoked, so it
 * cannot be done inside this function, so it has to operate with
 * byte-buffers. Write function on the other hand can "write-and-forget", data
 * is converted to byte-buffer before jtag_execute_queue().
 *
 * @param jtag_info
 * @param data		Array of bytes to read into.
 * @param end_state	End state after reading.
 */
static void arc_jtag_read_dr(struct arc_jtag *jtag_info, uint8_t *data,
		tap_state_t end_state)
{
	assert(jtag_info != NULL);
	assert(jtag_info->tap != NULL);

	jtag_info->tap_end_state = end_state;
	struct scan_field field;
	field.num_bits = 32;
	field.in_value = data;
	field.out_value = NULL;
	jtag_add_dr_scan(jtag_info->tap, 1, &field, jtag_info->tap_end_state);
}

/**
 * Write 4-byte word to data register.
 *
 * @param jtag_info
 * @param data		4-byte word to write into data register.
 * @param end_state	End state after writing.
 */
static void arc_jtag_write_dr(struct arc_jtag *jtag_info, uint32_t data,
		tap_state_t end_state)
{
	assert(jtag_info != NULL);
	assert(jtag_info->tap != NULL);

	jtag_info->tap_end_state = end_state;

	uint8_t out_value[4];
	buf_set_u32(out_value, 0, 32, data);

	struct scan_field field;
	field.num_bits = 32;
	field.out_value = out_value;
	field.in_value = NULL;
	jtag_add_dr_scan(jtag_info->tap, 1, &field, jtag_info->tap_end_state);
}


/**
 * Set transaction in command register. This function sets instruction register
 * and then transaction register, there is no need to invoke write_ir before
 * invoking this function.
 *
 * @param jtag_info
 * @param new_trans	Transaction to write to transaction command register.
 * @param end_state	End state after writing.
 */
static void arc_jtag_set_transaction(struct arc_jtag *jtag_info,
		arc_jtag_transaction_t new_trans, tap_state_t end_state)
{
	assert(jtag_info != NULL);
	assert(jtag_info->tap != NULL);

	/* No need to do anything. */
	if (jtag_info->cur_trans == new_trans)
		return;

	/* Set instruction. We used to call write_ir at upper levels, however
	 * write_ir-write_transaction were constantly in pair, so to avoid code
	 * duplication this function does it self. For this reasons it is "set"
	 * instead of "write". */
	arc_jtag_write_ir(jtag_info, ARC_TRANSACTION_CMD_REG);

	jtag_info->tap_end_state = end_state;

	uint8_t out_value[4];
	buf_set_u32(out_value, 0, ARC_TRANSACTION_CMD_REG_LENGTH, new_trans);

	struct scan_field field;
	field.num_bits = ARC_TRANSACTION_CMD_REG_LENGTH;
	field.out_value = out_value;
	field.in_value = NULL;

	jtag_add_dr_scan(jtag_info->tap, 1, &field, jtag_info->tap_end_state);
	jtag_info->cur_trans = new_trans;
}


/**
 * Run reset through transaction set. None of the previous
 * settings/commands/etc. are used anymore (or no influence).
 */
static void arc_jtag_reset_transaction(struct arc_jtag *jtag_info)
{
	arc_jtag_set_transaction(jtag_info, ARC_JTAG_CMD_NOP, TAP_IDLE);
}