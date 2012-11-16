/*
    Copyright (C) 2012 Commtech, Inc.

    This file is part of fscc-linux.

    fscc-linux is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    fscc-linux is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with fscc-linux.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "debug.h"
#include "debug.tmh"

#include "utils.h" /* return_{val_}if_true */

void print_worker(WDFDPC Dpc)
{
	struct fscc_port *port = 0;
    unsigned isr_value = 0;
	
	port = WdfObjectGet_FSCC_PORT(WdfDpcGetParentObject(Dpc));

    isr_value = port->last_isr_value;
    port->last_isr_value = 0;
	
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, 
				"interrupt: 0x%08x\n", isr_value);

    if (isr_value & RFE)
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "RFE (Receive Frame End Interrupt)");

    if (isr_value & RFT)
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "RFT (Receive FIFO Trigger Interrupt)");

    if (isr_value & RFS)
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "RFS (Receive Frame Start Interrupt)");

    if (isr_value & RFO)
		TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, "RFO (Receive Frame Overflow Interrupt)");

    if (isr_value & RDO)
		TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, "RDO (Receive Data Overflow Interrupt)");

    if (isr_value & RFL)
		TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, "RFL (Receive Frame Lost Interrupt)");

    if (isr_value & TIN)
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "TIN (Timer Expiration Interrupt)");

    if (isr_value & TFT)
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "TFT (Transmit FIFO Trigger Interrupt)");

    if (isr_value & TDU)
		TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE, "TDU (Transmit Data Underrun Interrupt)");

    if (isr_value & ALLS)
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "ALLS (All Sent Interrupt)");

    if (isr_value & CTSS)
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "CTSS (CTS State Change Interrupt)");

    if (isr_value & DSRC)
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "DSRC (DSR Change Interrupt)");

    if (isr_value & CDC)
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "CDC (CD Change Interrupt)");

    if (isr_value & DT_STOP)
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "DT_STOP (DMA Transmitter Full Stop indication)");

    if (isr_value & DR_STOP)
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "DR_STOP (DMA Receiver Full Stop indication)");

    if (isr_value & DT_FE)
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "DT_FE (DMA Transmit Frame End indication)");

    if (isr_value & DR_FE)
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "DR_FE (DMA Receive Frame End indication)");

    if (isr_value & DT_HI)
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "DT_HI (DMA Transmit Host Interrupt indication)");

    if (isr_value & DR_HI)
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "DR_HI (DMA Receive Host Interrupt indication)");
}