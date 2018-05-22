/****************************************************************************
       Module: ftdi_cjtag.c
     Engineer: Martin Hannon
  Description: Handles cJTAG comms to the ARC processor.
Date           Initials    Description
17-APR-2018    MOH         Initial
****************************************************************************/
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

static const unsigned OSCAN1 = 9;   // jtag7 format=oscan1 parameter.
static const unsigned EXIT_CMD_LVL = 1;


typedef enum
{
    normal, 
    oscan1

} TycJtagScanFormat;

#define MAX_SUPPORTED_SCAN_LENGTH 1000
static unsigned char pucRxBuffer[MAX_SUPPORTED_SCAN_LENGTH];
#define MAX_PREAMBLE  6
#define MAX_POSTAMBLE 2

TycJtagScanFormat tycJtagScanFormat = normal;
static int _want_result        = 1;
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
static void NZBS(unsigned amount);
static void ZBS(int repeat);
static void transitions (const char *tms_values, int tdi_value);
static void set_JTAG_to_idle(void);

static struct mpsse_ctx *local_ctx;

/****************************************************************************
     Function: cjtag_initialise
     Engineer: Martin Hannon
        Input: ctx :- Pointer to the mpsse_ctx context.
       Output: None
  Description: Initialises the cJTAG interface
Date           Initials    Description
17-APR-2018    MOH         Initial
****************************************************************************/
void cjtag_initialise(struct mpsse_ctx *ctx)
{
   unsigned char pucBuffer[0x100];
   unsigned char ucCount;

   tycJtagScanFormat = normal;
   _want_result      = 1;
   ulTxCount         = 0;

   // Keep a copy of the mpsse_ctx...
   local_ctx = ctx;

   // Initialsie the pins state....
   ucCount = 0;
   pucBuffer[ucCount++] = 0x80;
   pucBuffer[ucCount++] = 0xE8;
   pucBuffer[ucCount++] = 0xEB;
   pucBuffer[ucCount++] = 0x82;
   pucBuffer[ucCount++] = 0x00;
   pucBuffer[ucCount++] = 0x60;
   pucBuffer[ucCount++] = 0x81;
   pucBuffer[ucCount++] = 0x87;

   mpsse_write_read(local_ctx, pucBuffer, ucCount, pucBuffer, 1);



   // Issue the escape sequence....
   ucCount = 0;
   pucBuffer[ucCount++] = 0x80;
   pucBuffer[ucCount++] = 0xE8;
   pucBuffer[ucCount++] = 0xFB;
   pucBuffer[ucCount++] = 0x80;
   pucBuffer[ucCount++] = 0xE8;
   pucBuffer[ucCount++] = 0xFA;
   pucBuffer[ucCount++] = 0x80;
   pucBuffer[ucCount++] = 0xF9;
   pucBuffer[ucCount++] = 0xFA;
   pucBuffer[ucCount++] = 0x8E;
   pucBuffer[ucCount++] = 0x00;
   pucBuffer[ucCount++] = 0x4B;
   pucBuffer[ucCount++] = 0x05;
   pucBuffer[ucCount++] = 0x6A;
   pucBuffer[ucCount++] = 0x4B;
   pucBuffer[ucCount++] = 0x01;
   pucBuffer[ucCount++] = 0x06;
   pucBuffer[ucCount++] = 0x8E;
   pucBuffer[ucCount++] = 0x00;
   pucBuffer[ucCount++] = 0x80;
   pucBuffer[ucCount++] = 0xE8;
   pucBuffer[ucCount++] = 0xFA;
   pucBuffer[ucCount++] = 0x80;
   pucBuffer[ucCount++] = 0xE8;
   pucBuffer[ucCount++] = 0xFB;
   pucBuffer[ucCount++] = 0x80;
   pucBuffer[ucCount++] = 0xE8;
   pucBuffer[ucCount++] = 0xEB;

   mpsse_write_read(local_ctx, pucBuffer, ucCount, NULL, 0);

   // goto idle state....
   set_JTAG_to_idle();

   enter_2wire_mode_and_test();

}




/****************************************************************************
     Function: set_JTAG_to_idle
     Engineer: Martin Hannon
        Input: None
       Output: None
  Description: Sends the JTAG state machine to the idle state
Date           Initials    Description
17-APR-2018    MOH         Initial
****************************************************************************/
static void set_JTAG_to_idle(void)
{
   // Go to idle....
   transitions("0111110",TDI_0);

   if (!in_normal_scan_mode())
   {
       // Go back to 2wire mode.  We've destroyed things in RTI.
       set_format(normal);
       set_JTAG_to_idle();
       enter_2wire_mode();
   }
}

/****************************************************************************
     Function: transitions
     Engineer: Martin Hannon
        Input: tms_values :- String of TMS transitions to send.
               tdi_value  :- Fixed TDI value to send.
       Output: None
  Description: Performs TMS transitions to get to a required state.
Date           Initials    Description
17-APR-2018    MOH         Initial
****************************************************************************/
static void transitions (const char *tms_values, int tdi_value)
{
   unsigned char pucBuffer[0x100];
   unsigned char ucCount = 0;

   ulTxCount = 0;

   // Take a string of 1s and 0s (tms_values) and execute transitions
   // with them.  TDI is held fixed.
   for (const char *p = tms_values; *p; p++) 
   {
      if (*p == '_' || *p == ' ') 
         continue;   // nice for tracing.

      ulTxCount++;

      if (_want_result && in_normal_scan_mode())
      {
         pucBuffer[ucCount++] = 0x6F;
         pucBuffer[ucCount++] = 0x00;
         if (*p == '1')
            pucBuffer[ucCount++] = 0x01;
         else
            pucBuffer[ucCount++] = 0x00;
      }
      else if (!_want_result && in_normal_scan_mode())
      {
         pucBuffer[ucCount++] = 0x4B;
         pucBuffer[ucCount++] = 0x00;
         if (*p == '1')
            pucBuffer[ucCount++] = 0x01;
         else
            pucBuffer[ucCount++] = 0x00;
      }
      else
      {
         if (*p == '1')
         {
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
         else
         {
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

   if (_want_result && in_normal_scan_mode())
   {
      pucBuffer[ucCount++] = 0x87;
   }
   else if (!_want_result && in_normal_scan_mode())
   {
      pucBuffer[ucCount++] = 0x81;
      pucBuffer[ucCount++] = 0x87;
   }
   else
   {
      pucBuffer[ucCount++] = 0x87;
   }

   if (!_want_result && in_normal_scan_mode())
   {
      mpsse_write_read(local_ctx, pucBuffer, ucCount, pucRxBuffer, 1);
   }
   else
   {
      mpsse_write_read(local_ctx, pucBuffer, ucCount, pucRxBuffer, ulTxCount);
   }
}



/****************************************************************************
     Function: ZBS
     Engineer: Martin Hannon
        Input: repeat : Repeat count for the zero-bit-scan sequence.
       Output: None
  Description: Performs a zero-bit-scan sequence from idle and back to idle..
Date           Initials    Description
17-APR-2018    MOH         Initial
****************************************************************************/
static void ZBS(int repeat)
{
   // Perform a zero-bit-scan sequence from idle and back to idle.
   // rti, sds, cdr, sdr, exit-dr, update-dr, rti.
   char buf[1024], *bp = buf; *bp = 0;
   for (int i = 0; i < repeat; i++) 
   {
      strcat(bp,"10");   // enter capture-DR from RTI or SDS
      strcat(bp,"11_");   // exit, update.
   }
   // Now go to RTI.
   strcat(bp,"0");

   transitions(buf,TDI_0);
}

/****************************************************************************
     Function: NZBS
     Engineer: Martin Hannon
        Input: amount : Amounty of shifts in shift-DR state.
       Output: None
  Description: Perform a nonzero-bit-scan sequence from idle and back to idle.
Date           Initials    Description
17-APR-2018    MOH         Initial
****************************************************************************/
static void NZBS(unsigned amount)
{
   // Perform a nonzero-bit-scan sequence from idle and back to idle.
   // amount might == 0.
   // rti, sds, cdr, sdr, exit-dr, update-dr, rti.
   // It is OK to go to RTI after the bit scan, even though
   // not necessary.  But don't go to TLR!  That will break the logic.
   char buf[1024], *bp = buf;
   // A transition from shift-DR does a shift.  So to have a ZBS,
   // do not enter shift-DR at all.
   strcpy(bp,"10_");   // enter capture-DR
   bp = bp+strlen(bp);
   // Now cycle around shift-DR.  Each time you touch shift-DR
   // counts as 1.
   for (unsigned int i = 0;i < amount; i++) 
      *bp++ = '0';
   // Now go to update and RTI.
   strcpy(bp,"_110");
   transitions(buf,TDI_0);
}


/****************************************************************************
     Function: enter_control_mode
     Engineer: Martin Hannon
        Input: level : Control mode level to enter.
       Output: None
  Description: Enters the specified control mode.
Date           Initials    Description
17-APR-2018    MOH         Initial
****************************************************************************/
static void enter_control_mode(int level) 
{
   // Enter control level 2 with two ZBSs in a row.
   ZBS(level);

   // Next you must lock control level 2 with 
   NZBS(1);   // non-zero bit scan, count 1.

   // rascal2jtag7 has 3 trips around RTI.  Why?
   transitions("000",TDI_0);
}


/****************************************************************************
     Function: send_2part_command
     Engineer: Martin Hannon
        Input: opcode : Opcode to send.
               operand : Operand to send.
       Output: None
  Description: Sends a two part command to the device.
Date           Initials    Description
17-APR-2018    MOH         Initial
****************************************************************************/
static void send_2part_command(unsigned opcode, unsigned operand)
{
   NZBS(opcode);
   NZBS(operand);

   // Hm: verilog says:
   // tap7_three_part_cmd
   // Issue check packet if called in Advanced protocol
   // if(drive_cp && (scan_format > 3)) gen_check_packet();
   if (tycJtagScanFormat == oscan1)
   {
      // Dummy scan packet?!  See pranab email.
      check_packet();
   }
}

/****************************************************************************
     Function: send_JTAG
     Engineer: Martin Hannon
        Input: tms_and_tdi : TMS / TDI values to send
       Output: None
  Description: Low level cJTAG cycle for scan commands.
Date           Initials    Description
17-APR-2018    MOH         Initial
****************************************************************************/
static void send_JTAG(unsigned tms_and_tdi)
{
   unsigned char pucBuffer[0x100];
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

   mpsse_write_read(local_ctx, pucBuffer, ucCount, NULL, 0);
}

/****************************************************************************
     Function: dummy_scan_packet
     Engineer: Martin Hannon
        Input: None
       Output: None
  Description: Sends a dummy packet to the device
Date           Initials    Description
17-APR-2018    MOH         Initial
****************************************************************************/
static void dummy_scan_packet(void)
{
   transitions("0",TDI_0);
}


/****************************************************************************
     Function: send_check_packet
     Engineer: Martin Hannon
        Input: None
       Output: None
  Description: Sends a check packet to the device
Date           Initials    Description
17-APR-2018    MOH         Initial
****************************************************************************/
static void send_check_packet(void)
{
   unsigned char pucBuffer[0x100];
   unsigned char ucCount = 0;

   if (in_normal_scan_mode())
      pucBuffer[ucCount++] = 0x4B;
   else
      pucBuffer[ucCount++] = 0x1B;
   pucBuffer[ucCount++] = 0x03;
   pucBuffer[ucCount++] = 0x00;

   mpsse_write_read(local_ctx, pucBuffer, ucCount, NULL, 0);

}

/****************************************************************************
     Function: check_packet
     Engineer: Martin Hannon
        Input: None
       Output: None
  Description: Sends a check packet to the device
Date           Initials    Description
17-APR-2018    MOH         Initial
****************************************************************************/
static void check_packet(void)
{
   // dummy RTI->RTI when in oscan1.
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



/****************************************************************************
     Function: enter_oscan1
     Engineer: Martin Hannon
        Input: None
       Output: None
  Description: Enters oscan1 mode.
Date           Initials    Description
17-APR-2018    MOH         Initial
****************************************************************************/
static void enter_oscan1(void)
{
   // Enter the command STFMT MODE
   // where :
   // parameter STFMT       = 5'h03;  // Store Format
   // parameter TAP7_OSCAN1 = 5'h9;
   send_2part_command(STFMT,OSCAN1);
   // check packet.
   // After the command, must go to RTI and then enter 100 (?)
   // This confirms the command to go into oscan1 mode.
   // I don't get the "1" as that would exit RTIi and go to SDS.
   // transitions("0100",TDI_0,RTI_STRING);
   // r*7.v seems to generate 0 (preamble) and then 00 (body);
   // then what about 0 (postamble)?
   // See page 231 of the 1149.7 spec.
   check_packet();
   // Now in oscan1.  Port must know this.
   set_format(oscan1);
}


/****************************************************************************
     Function: enter_2wire_mode_and_test
     Engineer: Martin Hannon
        Input: None
       Output: 1: Success, 0 : Failure
  Description: Enters 2 wire_mode
Date           Initials    Description
17-APR-2018    MOH         Initial
****************************************************************************/
static int enter_2wire_mode_and_test(void)
{
   int wr = want_result(0);


   set_JTAG_to_idle();

   // Ahead of discovery, enter 2wire mode, and discover in that mode.
   // All JTAGs should be set to idle at this point.
   enter_control_mode(2);

   // cl 2 should now be locked and the arcs disconnected.
   // The 2wire device has now disconnected the ARCs.
   enter_oscan1();


   // Get out of disconnected state by going to IR and RTI
   // Now reconnect the arcs by entering shift-IR and then back to
   // RTI.
   transitions("1100110",TDI_0);

   // Now we can continue discovering the ARCs.
   want_result(wr);
   return 1;
}


/****************************************************************************
     Function: in_normal_scan_mode
     Engineer: Martin Hannon
        Input: None
       Output: TRUE : Normal scan mode, FALSE : oscan1 mode.
  Description: Checks if we are in normal / oscan1 mode.
Date           Initials    Description
17-APR-2018    MOH         Initial
****************************************************************************/
static bool in_normal_scan_mode(void)
{
   return tycJtagScanFormat == normal;
}

/****************************************************************************
     Function: set_format
     Engineer: Martin Hannon
        Input: sf : Scan mode format
       Output: None
  Description: Sets the current scan mode.
Date           Initials    Description
17-APR-2018    MOH         Initial
****************************************************************************/
static void set_format(TycJtagScanFormat sf)
{
   unsigned char pucBuffer[0x100];
   unsigned char ucCount = 0;

   if (sf == tycJtagScanFormat)
      return ;

   if (sf == normal)
   {
      pucBuffer[ucCount++] = 0x80;
      pucBuffer[ucCount++] = 0xA0;
      pucBuffer[ucCount++] = 0xEB;
      pucBuffer[ucCount++] = 0x82;
      pucBuffer[ucCount++] = 0x60;
      pucBuffer[ucCount++] = 0x60;
      pucBuffer[ucCount++] = 0x80;
      pucBuffer[ucCount++] = 0xA0;
      pucBuffer[ucCount++] = 0xEB;
      pucBuffer[ucCount++] = 0x82;
      pucBuffer[ucCount++] = 0x00;
      pucBuffer[ucCount++] = 0x60;
      pucBuffer[ucCount++] = 0x80;
      pucBuffer[ucCount++] = 0xE0;
      pucBuffer[ucCount++] = 0xEB;
      pucBuffer[ucCount++] = 0x82;
      pucBuffer[ucCount++] = 0x00;
      pucBuffer[ucCount++] = 0x60;
   }
   else if (sf == oscan1)
   {
      pucBuffer[ucCount++] = 0x80;
      pucBuffer[ucCount++] = 0xE0;
      pucBuffer[ucCount++] = 0xEB;
      pucBuffer[ucCount++] = 0x82;
      pucBuffer[ucCount++] = 0x00;
      pucBuffer[ucCount++] = 0x60;
      pucBuffer[ucCount++] = 0x80;
      pucBuffer[ucCount++] = 0xA0;
      pucBuffer[ucCount++] = 0xEB;
      pucBuffer[ucCount++] = 0x82;
      pucBuffer[ucCount++] = 0x60;
      pucBuffer[ucCount++] = 0x60;
      pucBuffer[ucCount++] = 0x80;
      pucBuffer[ucCount++] = 0xA0;
      pucBuffer[ucCount++] = 0xEB;
      pucBuffer[ucCount++] = 0x82;
      pucBuffer[ucCount++] = 0x60;
      pucBuffer[ucCount++] = 0x60;
   }

   mpsse_write_read(local_ctx, pucBuffer, ucCount, NULL, 0);


   tycJtagScanFormat = sf;
}

/****************************************************************************
     Function: want_result
     Engineer: Martin Hannon
        Input: wr : 1 Result required, 0 : No result required.
       Output: int : Previous wr value:
  Description: Helper function to set/get whether result is required.
Date           Initials    Description
17-APR-2018    MOH         Initial
****************************************************************************/
static int want_result(int wr)
{
   int ret = _want_result;
   _want_result = wr;
   return ret;
}


/****************************************************************************
     Function: enter_2wire_mode
     Engineer: Martin Hannon
        Input: None
       Output: None
  Description: Enters two wire mode.
Date           Initials    Description
17-APR-2018    MOH         Initial
****************************************************************************/
static void enter_2wire_mode(void)
{
   int wr = want_result(0);

   set_JTAG_to_idle();
   enter_control_mode(2);
   enter_oscan1();
   // exit command level so we can reconnect arcs.
   send_2part_command(STMC, EXIT_CMD_LVL);
   want_result(wr);
}

/****************************************************************************
     Function: cjtag_transitions
     Engineer: Martin Hannon
        Input: tms_values :- String of TMS transitions to send.
               tdi_value  :- Fixed TDI value to send.          
       Output: None
  Description: Performs TMS transitions to get to a required state.
               Optimised version of the transitions() function where reads 
               are not done until tthe end of the scan.
Date           Initials    Description
17-APR-2018    MOH         Initial
****************************************************************************/
static void cjtag_transitions (const char *tms_values, int tdi_value)
{
   unsigned char pucBuffer[0x100];
   unsigned char ucCount = 0;

   // Take a string of 1s and 0s (tms_values) and execute transitions
   // with them.  TDI is held fixed.
   for (const char *p = tms_values; *p; p++) 
   {
      if (*p == '_' || *p == ' ') 
         continue;   // nice for tracing.

      ulTxCount++;

         if (*p == '1')
         {
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
         else
         {
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

   mpsse_write_read(local_ctx, pucBuffer, ucCount, NULL, 0);

}

/****************************************************************************
     Function: cjtag_move_to_state
     Engineer: Martin Hannon
        Input: goal_state : Destination JTAG state,
       Output: None
  Description: Moves to the requied jtag state in 2 wire mode.
Date           Initials    Description
17-APR-2018    MOH         Initial
****************************************************************************/
static void cjtag_move_to_state(tap_state_t goal_state)
{
   tap_state_t start_state = tap_get_state();

   switch (start_state)
   {
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




/****************************************************************************
     Function: cjtag_execute_scan
     Engineer: Martin Hannon
        Input: cmd :- JTAG scan command to perform.
       Output: None
  Description: Performs a cJTAG scan
Date           Initials    Description
17-APR-2018    MOH         Initial
****************************************************************************/
void cjtag_execute_scan(struct jtag_command *cmd)
{
   int i;
   unsigned char ucTDI = 0;
   unsigned int ulPreamble = 0;



   if (cmd->cmd.scan->num_fields != 1) 
   {
      LOG_ERROR("Unexpected field size of %d", cmd->cmd.scan->num_fields);
      return;
   }

   struct scan_field *field = cmd->cmd.scan->fields;

   if ((field->num_bits + MAX_PREAMBLE  + MAX_POSTAMBLE)  > MAX_SUPPORTED_SCAN_LENGTH)
   {
      LOG_ERROR("Scan length too long at %d", field->num_bits);
      return;
   }


   ulTxCount = 0;


   if (cmd->cmd.scan->ir_scan) 
   {
      if (tap_get_state() != TAP_IRSHIFT)
         cjtag_move_to_state(TAP_IRSHIFT);
   } 
   else 
   {
      if (tap_get_state() != TAP_DRSHIFT)
         cjtag_move_to_state(TAP_DRSHIFT);
   }

   ulPreamble = ulTxCount;

   tap_set_end_state(cmd->cmd.scan->end_state);

   for (i = 0; i < field->num_bits - 1; i++)
   {
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

   // Move to the end state...
   cjtag_move_to_state(cmd->cmd.scan->end_state);

   // Perform the scan...
   mpsse_write_read(local_ctx, NULL, 0, pucRxBuffer, field->num_bits + ulTxCount);

   if (field->in_value)
   {
      for (i=0; i < field->num_bits; i++)
      {
         bit_copy(field->in_value, i, (const unsigned char *)&pucRxBuffer[i+ulPreamble], 7, 1);
      }
   }
}

