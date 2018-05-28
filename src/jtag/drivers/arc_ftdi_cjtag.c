/**************************************************************************
*   Copyright (C) 2018 by Martin Hannon                                   *
*   martin.hannon@ashling.com                                             *
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

/**
* @file
* Handles cJTAG comms to the ARC processor.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* project specific includes */
#include <jtag/interface.h>
#include <jtag/swd.h>
#include <transport/transport.h>
#include <helper/time_support.h>

#if IS_CYGWIN == 1
#include <windows.h>
#endif

#include <assert.h>

/* FTDI access library includes */
#include "mpsse.h"

#define TDI_0 0 
#define TDI_1 1
#define TMS_0 0 
#define TMS_1 2 

#define STFMT 3 /*  jtag7 store format command. */
#define SCNS 9  /* jtag7 scan string */
#define STMC 0  /* jtag7 Store Miscellaneous Control */
#define MSC 7   /* jtag7 make scan group candidate */

static const unsigned OSCAN1 = 9;   /* jtag7 format=oscan1 parameter. */
static const unsigned EXIT_CMD_LVL = 1;

typedef enum
{
	normal, 
	oscan1
} TycJtagScanFormat;

#define MAX_SUPPORTED_SCAN_LENGTH 1000
static uint8_t pucRxBuffer[MAX_SUPPORTED_SCAN_LENGTH];
#define MAX_PREAMBLE  6
#define MAX_POSTAMBLE 2

static TycJtagScanFormat tycJtagScanFormat = normal;
static int iWantResult  = 1;
static unsigned long ulTxCount = 0;
static void enter_2wire_mode(void);
static int want_result(int wr);
static void set_format(TycJtagScanFormat sf);
static bool in_normal_scan_mode(void);
static int enter_2wire_mode_and_test(void);
static void enter_oscan1(void);
static void check_packet(void);
static void send_check_packet(void);
static void dummy_scan_packet(void);
static void send_JTAG(unsigned tms_and_tdi);
static void send_2part_command(unsigned opcode, unsigned operand);
static void enter_control_mode(int level) ;
static void non_zero_bit_scan_sequence(unsigned amount);
static void zero_bit_scan_sequence(int repeat);
static void transitions(const char *tms_values, int tdi_value);
static void set_JTAG_to_idle(void);
static void cjtag_move_to_state(tap_state_t goal_state);

static struct mpsse_ctx *local_ctx;

static const uint8_t puDigilentHs2cInitializeSequence[] = {
		0x80, /* Set output pins            */
		0xE8, /*        Value               */
		0xEB, /*        Direction           */
		0x82, /* Set output pins (high byte)*/
		0x00, /*        Value               */
		0x60, /*        Direction           */
		0x81, /* Read input pins            */
		0x87  /* Send immediate             */
};

static const uint8_t pucEscapeSequence[] = {
		0x80, /* Set output pins            */
		0xE8, /*        Value               */
		0xFB, /*        Direction           */
		0x80, /* Set output pins            */
		0xE8, /*        Value               */
		0xFA, /*        Direction           */
		0x80, /* Set output pins            */
		0xF9, /*        Value               */
		0xFA, /*        Direction           */
		0x8E, /* Clock with no data         */
		0x00, /*        Number of bits      */
		0x4B, /* Clock data to TMS pin      */
		0x05, /*        Length              */
		0x6A, /*        Data                */
		0x4B, /* Clock data to TMS pin      */
		0x01, /*        Length              */
		0x06, /*        Data                */
		0x8E, /* Clock with no data         */
		0x00, /*        Number of bits      */
		0x80, /* Set output pins            */
		0xE8, /*        Value               */
		0xFA, /*        Direction           */
		0x80, /* Set output pins            */
		0xE8, /*        Value               */
		0xFB, /*        Direction           */
		0x80, /* Set output pins            */
		0xE8, /*        Value               */
		0xEB  /*        Direction           */
};

/**
* Initializes the cJTAG interface
* @param ctx is a pointer to the mpsse_ctx context.
*/
void cjtag_initialize(struct mpsse_ctx *ctx)
{
	uint8_t ucResponse;

	tycJtagScanFormat = normal;
	iWantResult = 1;
	ulTxCount   = 0;

	/* Keep a copy of the mpsse_ctx...*/
	local_ctx = ctx;

	/* Initialize the FTDI chip pins state...*/
	mpsse_write(local_ctx, puDigilentHs2cInitializeSequence, ARRAY_SIZE(puDigilentHs2cInitializeSequence));
	mpsse_read(local_ctx, &ucResponse, 1);

	/* Issue the escape sequence...*/
	mpsse_write(local_ctx, pucEscapeSequence, ARRAY_SIZE(pucEscapeSequence));

	/* goto idle state...*/
	set_JTAG_to_idle();

	enter_2wire_mode_and_test();
}

/**
* Performs a cJTAG scan
* @param cmd contains the JTAG scan command to perform.
*/
void cjtag_execute_scan(struct jtag_command *cmd)
{
	int i;
	unsigned char ucTDI = 0;
	unsigned int ulPreamble = 0;

	if (cmd->cmd.scan->num_fields != 1) {
		LOG_ERROR("Unexpected field size of %d", cmd->cmd.scan->num_fields);
		return;
	}

	struct scan_field *field = cmd->cmd.scan->fields;

	if ((field->num_bits + MAX_PREAMBLE + MAX_POSTAMBLE)  > MAX_SUPPORTED_SCAN_LENGTH) {
		LOG_ERROR("Scan length too long at %d", field->num_bits);
		return;
	}

	ulTxCount = 0;

	if (cmd->cmd.scan->ir_scan) {
		if (tap_get_state() != TAP_IRSHIFT)
			cjtag_move_to_state(TAP_IRSHIFT);
	}
	else {
		if (tap_get_state() != TAP_DRSHIFT)
			cjtag_move_to_state(TAP_DRSHIFT);
	}

	ulPreamble = ulTxCount;

	tap_set_end_state(cmd->cmd.scan->end_state);

	for (i = 0; i < field->num_bits - 1; i++) {
		if (field->out_value)
			bit_copy(&ucTDI, 0, field->out_value, i, 1);

		if (ucTDI)
			send_JTAG(TMS_0 | TDI_1);
		else
			send_JTAG(TMS_0 | TDI_0);
	}


	if (field->out_value)
		bit_copy(&ucTDI, 0, field->out_value, i, 1);

	if (ucTDI)
		send_JTAG(TMS_1 | TDI_1);
	else
		send_JTAG(TMS_1 | TDI_0);


	if (cmd->cmd.scan->ir_scan)
		tap_set_state(TAP_IREXIT1);
	else
		tap_set_state(TAP_DREXIT1);

	/* Move to the end state... */
	cjtag_move_to_state(cmd->cmd.scan->end_state);

	/* Perform the scan... */
	mpsse_read(local_ctx, pucRxBuffer, field->num_bits + ulTxCount);

	if (field->in_value) {
		for (i = 0; i < field->num_bits; i++) {
			bit_copy(field->in_value, i, (const unsigned char *)&pucRxBuffer[i + ulPreamble], 7, 1);
		}
	}
}

/**
* Sends the JTAG state machine to the idle state
*/
static void set_JTAG_to_idle(void)
{
	/* Go to idle...*/
	transitions("0111110",TDI_0);

	if (!in_normal_scan_mode()) {
		/* Go back to 2wire mode.  We've destroyed things in RTI.*/
		set_format(normal);
		set_JTAG_to_idle();
		enter_2wire_mode();
	}
}

/**
* Performs TMS transitions to get to a required state.
* @param tms_values contains a string of TMS transitions to send.
* @param tdi_value contains a dixed TDI value to send.
*/
static void transitions(const char *tms_values, int tdi_value)
{
	uint8_t pucBuffer[0x100];
	unsigned char ucCount = 0;

	ulTxCount = 0;

	/* Take a string of 1s and 0s (tms_values) and execute transitions
	   with them.  TDI is held fixed. */
	for (const char *p = tms_values; *p; p++) {
		if (*p == '_' || *p == ' ') 
			continue;

		ulTxCount++;

		if (iWantResult && in_normal_scan_mode()) {
			pucBuffer[ucCount++] = 0x6F;
			pucBuffer[ucCount++] = 0x00;
			if (*p == '1')
				pucBuffer[ucCount++] = 0x01;
			else
				pucBuffer[ucCount++] = 0x00;
		}
		else if (!iWantResult && in_normal_scan_mode()) {
			pucBuffer[ucCount++] = 0x4B;
			pucBuffer[ucCount++] = 0x00;
			if (*p == '1')
				pucBuffer[ucCount++] = 0x01;
			else
				pucBuffer[ucCount++] = 0x00;
		}
		else {
			if (*p == '1') {
				pucBuffer[ucCount++] = 0x1B;
				pucBuffer[ucCount++] = 0x00;
				pucBuffer[ucCount++] = 0x03;

				pucBuffer[ucCount++] = 0x97;
				pucBuffer[ucCount++] = 0x97;
				pucBuffer[ucCount++] = 0x97;

				pucBuffer[ucCount++] = 0x80;
				pucBuffer[ucCount++] = 0xB3;
				pucBuffer[ucCount++] = 0xEB;

				pucBuffer[ucCount++] = 0x97;

				pucBuffer[ucCount++] = 0x80;
				pucBuffer[ucCount++] = 0x82;
				pucBuffer[ucCount++] = 0xEB;

				pucBuffer[ucCount++] = 0x2A;
				pucBuffer[ucCount++] = 0x00;

				pucBuffer[ucCount++] = 0x80;
				pucBuffer[ucCount++] = 0xA2;
				pucBuffer[ucCount++] = 0xEB;
			}
			else {
				pucBuffer[ucCount++] = 0x1B;
				pucBuffer[ucCount++] = 0x00;
				pucBuffer[ucCount++] = 0x01;

				pucBuffer[ucCount++] = 0x97;
				pucBuffer[ucCount++] = 0x97;
				pucBuffer[ucCount++] = 0x97;

				pucBuffer[ucCount++] = 0x80;
				pucBuffer[ucCount++] = 0xB1;
				pucBuffer[ucCount++] = 0xEB;

				pucBuffer[ucCount++] = 0x97;

				pucBuffer[ucCount++] = 0x80;
				pucBuffer[ucCount++] = 0x80;
				pucBuffer[ucCount++] = 0xEB;

				pucBuffer[ucCount++] = 0x2A;
				pucBuffer[ucCount++] = 0x00;

				pucBuffer[ucCount++] = 0x80;
				pucBuffer[ucCount++] = 0xA0;
				pucBuffer[ucCount++] = 0xEB;
			}
		}
	}

	if (iWantResult && in_normal_scan_mode()) {
		pucBuffer[ucCount++] = 0x87;
	}
	else if (!iWantResult && in_normal_scan_mode()) {
		pucBuffer[ucCount++] = 0x81;
		pucBuffer[ucCount++] = 0x87;
	}
	else {
		pucBuffer[ucCount++] = 0x87;
	}

	mpsse_write(local_ctx, pucBuffer, ucCount);

	if (!iWantResult && in_normal_scan_mode()) {
		mpsse_read(local_ctx, pucRxBuffer, 1);
	}
	else {
		mpsse_read(local_ctx, pucRxBuffer, ulTxCount);
	}
}

/**
* Performs a zero-bit-scan sequence from idle and back to idle.
* @param repeat contains the repeat count for the zero-bit-scan sequence.
*/
static void zero_bit_scan_sequence(int repeat)
{
	/* Perform a zero-bit-scan sequence from idle and back to idle.
	   rti, sds, cdr, sdr, exit-dr, update-dr, rti. */
	char buf[1024], *bp = buf; *bp = 0;

	/* Verify that the repeat parameter is within bounds */
	assert(repeat <= 10);

	for (int i = 0; i < repeat; i++) {
		strcat(bp,"10");   /* enter capture-DR from RTI or SDS */
		strcat(bp,"11_");  /* exit, update. */
	}
	/* Now go to RTI. */
	strcat(bp,"0");

	transitions(buf,TDI_0);
}

/**
* Perform a nonzero-bit-scan sequence from idle and back to idle.
* @param amount contains the amount of shifts in shift-DR state.
*/
static void non_zero_bit_scan_sequence(unsigned amount)
{
	/* Perform a nonzero-bit-scan sequence from idle and back to idle.
	   amount might == 0.
	   rti, sds, cdr, sdr, exit-dr, update-dr, rti.
	   It is OK to go to RTI after the bit scan, even though
	   not necessary.  But don't go to TLR!  That will break the logic. */
	char buf[1024], *bp = buf;

	/* Verify that the amount parameter is within bounds */
	assert(amount <= 10);

	/* A transition from shift-DR does a shift.  So to have a zero_bit_scan_sequence,
	   do not enter shift-DR at all. */
	strcpy(bp,"10_");   /* enter capture-DR */
	bp = bp+strlen(bp);
	/* Now cycle around shift-DR.  Each time you touch shift-DR
	   counts as 1. */
	for (unsigned int i = 0;i < amount; i++) 
		*bp++ = '0';
	/* Now go to update and RTI. */
	strcpy(bp,"_110");
	transitions(buf,TDI_0);
}

/**
* Enters the specified control mode.
* @param level contains the control mode level to enter.
*/
static void enter_control_mode(int level)
{
	/* Enter control level with multiple ZBS's in a row. */
	zero_bit_scan_sequence(level);

	/* Next you must lock control level. */
	non_zero_bit_scan_sequence(1);   /* non-zero bit scan, count 1. */

	transitions("000",TDI_0);
}

/**
* Sends a two part command to the device.
* @param opcode contains the opcode to send.
* @param operand contains the operand to send.
*/
static void send_2part_command(unsigned opcode, unsigned operand)
{
	non_zero_bit_scan_sequence(opcode);
	non_zero_bit_scan_sequence(operand);

	/* If we are currently in oscan1 mode... */
	if (tycJtagScanFormat == oscan1) {
		/* ...send a checkpacket. */
		check_packet();
	}
}

/**
* Low level cJTAG cycle for scan commands.
* @param tms_and_tdi contains the TMS / TDI values to send.
*/
static void send_JTAG(unsigned tms_and_tdi)
{
	uint8_t pucBuffer[0x100];
	unsigned char ucCount = 0;

	pucBuffer[ucCount++] = 0x1B;
	pucBuffer[ucCount++] = 0x00;
	if (tms_and_tdi == (TMS_0 | TDI_0))
		pucBuffer[ucCount++] = 0x01;
	else if (tms_and_tdi == (TMS_0 | TDI_1))
		pucBuffer[ucCount++] = 0x00;
	else if (tms_and_tdi == (TMS_1 | TDI_0))
		pucBuffer[ucCount++] = 0x03;
	else if (tms_and_tdi == (TMS_1 | TDI_1))
		pucBuffer[ucCount++] = 0x02;

	pucBuffer[ucCount++] = 0x97;
	pucBuffer[ucCount++] = 0x97;
	pucBuffer[ucCount++] = 0x97;

	pucBuffer[ucCount++] = 0x80;
	if (tms_and_tdi == (TMS_0 | TDI_0))
		pucBuffer[ucCount++] = 0xB1;
	else if (tms_and_tdi == (TMS_0 | TDI_1))
		pucBuffer[ucCount++] = 0xB1;
	else if (tms_and_tdi == (TMS_1 | TDI_0))
		pucBuffer[ucCount++] = 0xB3;
	else if (tms_and_tdi == (TMS_1 | TDI_1))
		pucBuffer[ucCount++] = 0xB3;
	pucBuffer[ucCount++] = 0xEB;

	pucBuffer[ucCount++] = 0x97;

	pucBuffer[ucCount++] = 0x80;
	if (tms_and_tdi == (TMS_0 | TDI_0))
		pucBuffer[ucCount++] = 0x80;
	else if (tms_and_tdi == (TMS_0 | TDI_1))
		pucBuffer[ucCount++] = 0x80;
	else if (tms_and_tdi == (TMS_1 | TDI_0))
		pucBuffer[ucCount++] = 0x82;
	else if (tms_and_tdi == (TMS_1 | TDI_1))
		pucBuffer[ucCount++] = 0x82;
	pucBuffer[ucCount++] = 0xEB;

	pucBuffer[ucCount++] = 0x2A;
	pucBuffer[ucCount++] = 0x00;

	pucBuffer[ucCount++] = 0x80;
	if (tms_and_tdi == (TMS_0 | TDI_0))
		pucBuffer[ucCount++] = 0xA0;
	else if (tms_and_tdi == (TMS_0 | TDI_1))
		pucBuffer[ucCount++] = 0xA0;
	else if (tms_and_tdi == (TMS_1 | TDI_0))
		pucBuffer[ucCount++] = 0xA2;
	else if (tms_and_tdi == (TMS_1 | TDI_1))
		pucBuffer[ucCount++] = 0xA2;
	pucBuffer[ucCount++] = 0xEB;

	mpsse_write(local_ctx, pucBuffer, ucCount);
}

/**
* Sends a dummy packet to the device
*/
static void dummy_scan_packet(void)
{
	transitions("0",TDI_0);
}

/**
* Sends a check packet to the device
*/
static void send_check_packet(void)
{
	uint8_t pucBuffer[3];
	unsigned char ucCount = 0;

	if (in_normal_scan_mode())
		pucBuffer[ucCount++] = 0x4B;
	else
		pucBuffer[ucCount++] = 0x1B;
	pucBuffer[ucCount++] = 0x03;
	pucBuffer[ucCount++] = 0x00;

	mpsse_write(local_ctx, pucBuffer, ucCount);
}

/**
*  Sends a check packet to the device
*/
static void check_packet(void)
{
	/* dummy RTI->RTI when in oscan1. */
	bool in_oscan1 = !in_normal_scan_mode();
	if (in_oscan1)
	{
		dummy_scan_packet();
		/* From pranab:
		IEEE Std 1149.7-2009 
		- Section "21.7: TAPC state/packet relationships" 
		- with "Figure 21-9 - Typical DTS-initiated packet sequences and APU state transitions" 
		contains the picture I created earlier for Check packets & Dummy scan packets.
		*/
	}
	send_check_packet();
}

/**
*  Enters oscan1 mode.
*/
static void enter_oscan1(void)
{
	/* Enter the command STFMT MODE
	   where :
	   parameter STFMT    = 5'h03;  Store Format
	   parameter TAP7_OSCAN1 = 5'h9; */
	send_2part_command(STFMT,OSCAN1);
	/* After the command, must send a check packet */
	check_packet();
	/* Now in oscan1.  Port must know this. */
	set_format(oscan1);
}

/**
* Enters 2 wire_mode
* @return 1: Success, 0 : Failure
*/
static int enter_2wire_mode_and_test(void)
{
	int wr = want_result(0);

	set_JTAG_to_idle();

	/* Ahead of discovery, enter 2wire mode, and discover in that mode.
	   All JTAGs should be set to idle at this point. */
	enter_control_mode(2);

	/* cl 2 should now be locked and the arcs disconnected.
	   The 2wire device has now disconnected the ARCs. */
	enter_oscan1();

	/* Get out of disconnected state by going to IR and RTI
	   Now reconnect the arcs by entering shift-IR and then back to
	   RTI. */
	transitions("1100110",TDI_0);

	/* Now we can continue discovering the ARCs. */
	want_result(wr);
	return 1;
}

/**
* Checks if we are in normal / oscan1 mode.
* @return TRUE : Normal scan mode, FALSE : oscan1 mode.
*/
static bool in_normal_scan_mode(void)
{
	return tycJtagScanFormat == normal;
}

/**
* Sets the current scan mode.
* @param sf contains the Scan mode format
*/
static void set_format(TycJtagScanFormat sf)
{
	uint8_t normalFormatSeq[] = {
		0x80,0xA0,0xEB,
		0x82,0x60,0x60,
		0x80,0xA0,0xEB,
		0x82,0x00,0x60,
		0x80,0xE0,0xEB,
		0x82,0x00,0x60
};
	uint8_t oscan1FormatSeq[] = {
		0x80,0xE0,0xEB,
		0x82,0x00,0x60,
		0x80,0xA0,0xEB,
		0x82,0x60,0x60,
		0x80,0xA0,0xEB,
		0x82,0x60,0x60
};

	if (sf == tycJtagScanFormat)
		return ;

	if (sf == normal) {
		mpsse_write(local_ctx, normalFormatSeq, ARRAY_SIZE(normalFormatSeq));
	}
	else if (sf == oscan1) {
		mpsse_write(local_ctx, oscan1FormatSeq, ARRAY_SIZE(oscan1FormatSeq));
	}

	tycJtagScanFormat = sf;
}

/**
* Helper function to set/get whether result is required.
* @param wr : 1 Result required, 0 : No result required.
* @return Previous wr value.
*/
static int want_result(int wr)
{
	int ret = iWantResult;
	iWantResult = wr;
	return ret;
}

/**
* Enters two wire mode.
*/
static void enter_2wire_mode(void)
{
	int wr = want_result(0);

	set_JTAG_to_idle();
	enter_control_mode(2);
	enter_oscan1();
	/* exit command level so we can reconnect arcs.*/
	send_2part_command(STMC, EXIT_CMD_LVL);
	want_result(wr);
}

/**
* Performs TMS transitions to get to a required state.
* Optimised version of the transitions() function where reads 
* are not done until tthe end of the scan.
* @param tms_values contains a string of TMS transitions to send.
* @param tdi_value contains the fixed TDI value to send.    
*/
static void cjtag_transitions (const char *tms_values, int tdi_value)
{
	uint8_t pucBuffer[0x100];
	unsigned char ucCount = 0;

	/* Take a string of 1s and 0s (tms_values) and execute transitions
	   with them.  TDI is held fixed. */
	for (const char *p = tms_values; *p; p++) {

		if (*p == '_' || *p == ' ') 
			continue;

		ulTxCount++;

		if (*p == '1') {
			pucBuffer[ucCount++] = 0x1B;
			pucBuffer[ucCount++] = 0x00;
			pucBuffer[ucCount++] = 0x03;

			pucBuffer[ucCount++] = 0x97;
			pucBuffer[ucCount++] = 0x97;
			pucBuffer[ucCount++] = 0x97;

			pucBuffer[ucCount++] = 0x80;
			pucBuffer[ucCount++] = 0xB3;
			pucBuffer[ucCount++] = 0xEB;

			pucBuffer[ucCount++] = 0x97;

			pucBuffer[ucCount++] = 0x80;
			pucBuffer[ucCount++] = 0x82;
			pucBuffer[ucCount++] = 0xEB;

			pucBuffer[ucCount++] = 0x2A;
			pucBuffer[ucCount++] = 0x00;

			pucBuffer[ucCount++] = 0x80;
			pucBuffer[ucCount++] = 0xA2;
			pucBuffer[ucCount++] = 0xEB;
		}
		else {
			pucBuffer[ucCount++] = 0x1B;
			pucBuffer[ucCount++] = 0x00;
			pucBuffer[ucCount++] = 0x01;

			pucBuffer[ucCount++] = 0x97;
			pucBuffer[ucCount++] = 0x97;
			pucBuffer[ucCount++] = 0x97;

			pucBuffer[ucCount++] = 0x80;
			pucBuffer[ucCount++] = 0xB1;
			pucBuffer[ucCount++] = 0xEB;

			pucBuffer[ucCount++] = 0x97;

			pucBuffer[ucCount++] = 0x80;
			pucBuffer[ucCount++] = 0x80;
			pucBuffer[ucCount++] = 0xEB;

			pucBuffer[ucCount++] = 0x2A;
			pucBuffer[ucCount++] = 0x00;

			pucBuffer[ucCount++] = 0x80;
			pucBuffer[ucCount++] = 0xA0;
			pucBuffer[ucCount++] = 0xEB;
		}
	}

	mpsse_write(local_ctx, pucBuffer, ucCount);
}

/**
* Moves to the requied jtag state in 2 wire mode.
* @param goal_state contains the destination JTAG state,
*/
static void cjtag_move_to_state(tap_state_t goal_state)
{
	tap_state_t start_state = tap_get_state();

	switch (start_state) {
	case TAP_IDLE:
		if (goal_state == TAP_DRSHIFT)
			cjtag_transitions("100",TDI_0);
		else if (goal_state == TAP_IRSHIFT)
			cjtag_transitions("1100",TDI_0);
		break;
	case TAP_DRPAUSE:
		if (goal_state == TAP_DRSHIFT)
			cjtag_transitions("10",TDI_0);
		else if (goal_state == TAP_IRSHIFT)
			cjtag_transitions("111100",TDI_0);
		break;
	case TAP_IRPAUSE:
		if (goal_state == TAP_DRSHIFT)
			cjtag_transitions("11100",TDI_0);
		else if (goal_state == TAP_IRSHIFT)
			cjtag_transitions("10",TDI_0);
		break;
	case TAP_IREXIT1:
		if (goal_state == TAP_IRPAUSE)
			cjtag_transitions("0",TDI_0);
		else if (goal_state == TAP_IDLE)
			cjtag_transitions("10",TDI_0);
		break;
	case TAP_DREXIT1:
		if (goal_state == TAP_DRPAUSE)
			cjtag_transitions("0",TDI_0);
		else if (goal_state == TAP_IDLE)
			cjtag_transitions("10",TDI_0);
		break;
	default:
		break;
	}

	tap_set_state(goal_state);
}

