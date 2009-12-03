/***************************************************************************
 *   Copyright (C) 2005 by Dominic Rath <Dominic.Rath@gmx.de>              *
 *   Copyright (C) 2007,2008 Øyvind Harboe <oyvind.harboe@zylin.com>       *
 *   Copyright (C) 2009 Zachary T Welch <zw@superlucidity.net>             *
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
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#ifndef MINIDRIVER_IMP_H
#define MINIDRIVER_IMP_H

#include <jtag/commands.h>

static inline void interface_jtag_alloc_in_value32(struct scan_field *field)
{
	field->in_value = (uint8_t *)cmd_queue_alloc(4);
}

static inline void interface_jtag_add_scan_check_alloc(struct scan_field *field)
{
	unsigned num_bytes = DIV_ROUND_UP(field->num_bits, 8);
	field->in_value = (uint8_t *)cmd_queue_alloc(num_bytes);
}

void interface_jtag_add_dr_out(struct jtag_tap* tap,
		int num_fields, const int* num_bits, const uint32_t* value,
		tap_state_t end_state);

void interface_jtag_add_callback(jtag_callback1_t f, jtag_callback_data_t data0);

void interface_jtag_add_callback4(jtag_callback_t f, jtag_callback_data_t data0,
		jtag_callback_data_t data1, jtag_callback_data_t data2,
		jtag_callback_data_t data3);

#endif // MINIDRIVER_IMP_H
