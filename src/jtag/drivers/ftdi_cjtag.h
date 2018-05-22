/****************************************************************************
       Module: ftdi_cjtag.c
     Engineer: Martin Hannon
  Description: Handles cJTAG comms to the ARC processor.
Date           Initials    Description
17-APR-2018    MOH         Initial
****************************************************************************/
void cjtag_initialise(struct mpsse_ctx *ctx);
void cjtag_execute_scan(struct jtag_command *cmd);

