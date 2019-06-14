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


 #ifndef ARC_JTAG_H
 #define ARC_JTAG_H

#define ARC_TRANSACTION_CMD_REG 	0x9 /* Command to perform */
#define ARC_TRANSACTION_CMD_REG_LENGTH 4

typedef enum arc_jtag_transaction {
	ARC_JTAG_WRITE_TO_MEMORY = 0x0,
	ARC_JTAG_WRITE_TO_CORE_REG = 0x1,
	ARC_JTAG_WRITE_TO_AUX_REG = 0x2,
	ARC_JTAG_CMD_NOP = 0x3,
	ARC_JTAG_READ_FROM_MEMORY = 0x4,
	ARC_JTAG_READ_FROM_CORE_REG = 0x5,
	ARC_JTAG_READ_FROM_AUX_REG = 0x6,
} arc_jtag_transaction_t;

struct arc_jtag {
	struct jtag_tap *tap;
	uint32_t tap_end_state;
	uint32_t intest_instr;
	uint32_t cur_trans;

	uint32_t scann_size;
	uint32_t scann_instr;
	uint32_t cur_scan_chain;

	uint32_t dpc; /* Debug PC value */

	int fast_access_save;
	bool always_check_status_rd;
	bool check_status_fl;
	bool wait_until_write_finished;
};

#endif /* ARC_JTAG_H */
