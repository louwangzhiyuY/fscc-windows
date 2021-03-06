/*
    Copyright (C) 2016  Commtech, Inc.

    This file is part of fscc-windows.

    fscc-windows is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published bythe Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    fscc-windows is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along
    with fscc-windows.  If not, see <http://www.gnu.org/licenses/>.

*/


#ifndef FSCC_ISR_H
#define FSCC_ISR_H

#include <ntddk.h>
#include <wdf.h>

#include "Trace.h"

EVT_WDF_INTERRUPT_ISR fscc_isr;

EVT_WDF_DPC oframe_worker;
EVT_WDF_DPC clear_oframe_worker;
EVT_WDF_DPC iframe_worker;
EVT_WDF_DPC istream_worker;
EVT_WDF_DPC isr_alert_worker;
EVT_WDF_DPC request_worker;

EVT_WDF_TIMER timer_handler;

#endif