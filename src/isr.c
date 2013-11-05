/*
    Copyright (C) 2013  Commtech, Inc.

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


#include "isr.h"
#include "port.h" /* struct fscc_port */
#include "utils.h" /* port_exists */
#include "frame.h" /* struct fscc_frame */
#include "debug.h"

#if defined(EVENT_TRACING)
#include "isr.tmh"
#endif

#define TX_FIFO_SIZE 4096
#define MAX_LEFTOVER_BYTES 3

//TODO: Not sure if I should delay some of this to the ISR DPC
BOOLEAN fscc_isr(WDFINTERRUPT Interrupt, ULONG MessageID)
{
    struct fscc_port *port = 0;
    BOOLEAN handled = FALSE;
    unsigned isr_value = 0;
    unsigned streaming = 0;

    UNREFERENCED_PARAMETER(MessageID);

    port = WdfObjectGet_FSCC_PORT(WdfInterruptGetDevice(Interrupt));

    //WdfTimerStop(port->timer, FALSE);

    isr_value = fscc_port_get_register(port, 0, ISR_OFFSET);

    if (!isr_value)
        return handled;

    handled = TRUE;

    port->last_isr_value |= isr_value;
    streaming = fscc_port_is_streaming(port);

    if (streaming) {
        if (isr_value & (RFT | RFS))
            WdfDpcEnqueue(port->istream_dpc);
    }
    else {
        if (isr_value & (RFE | RFT | RFS))
            WdfDpcEnqueue(port->iframe_dpc);
    }

    if (isr_value & TFT)
        WdfDpcEnqueue(port->oframe_dpc);

    if (isr_value & ALLS)
        WdfDpcEnqueue(port->clear_oframe_dpc);

#ifdef DEBUG
    WdfDpcEnqueue(port->print_dpc);
#endif
    //fscc_port_reset_timer(port);

    return handled;
}

void iframe_worker(WDFDPC Dpc)
{
    struct fscc_port *port = 0;
    int receive_length = 0; /* Needs to be signed */
    unsigned finished_frame = 0;
    static int rejected_last_frame = 0;
    unsigned current_memory = 0;
    unsigned memory_cap = 0;
    int status;

    port = WdfObjectGet_FSCC_PORT(WdfDpcGetParentObject(Dpc));

    return_if_untrue(port);

    do {
        current_memory = fscc_port_get_input_memory_usage(port);
        memory_cap = fscc_port_get_input_memory_cap(port);

        WdfSpinLockAcquire(port->board_rx_spinlock);
        WdfSpinLockAcquire(port->pending_iframe_spinlock);

        finished_frame = (fscc_port_get_RFCNT(port) > 0) ? 1 : 0;

        if (finished_frame) {
            unsigned bc_fifo_l = 0;
            unsigned current_length = 0;

            bc_fifo_l = fscc_port_get_register(port, 0, BC_FIFO_L_OFFSET);

            if (port->pending_iframe)
                current_length = fscc_frame_get_length(port->pending_iframe);
            else
                current_length = 0;

            receive_length = bc_fifo_l - current_length;
        } else {
            unsigned rxcnt = 0;

            rxcnt = fscc_port_get_RXCNT(port);

            /* We choose a safe amount to read due to more data coming in after we
               get our values. The rest will be read on the next interrupt. */
            receive_length = rxcnt - STATUS_LENGTH - MAX_LEFTOVER_BYTES;
            receive_length -= receive_length % 4;
        }

        /* Leave the interrupt handler if there is no data to read. */
        if (receive_length <= 0) {
            WdfSpinLockRelease(port->pending_iframe_spinlock);
            WdfSpinLockRelease(port->board_rx_spinlock);
            return;
        }

        /* Make sure we don't go over the user's memory constraint. */
        if (current_memory + receive_length > memory_cap) {
            if (rejected_last_frame == 0) {
                TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
                            "Rejecting frames (memory constraint)");
                rejected_last_frame = 1; /* Track that we dropped a frame so we
                                        don't have to warn the user again. */
            }

            if (port->pending_iframe) {
                fscc_frame_delete(port->pending_iframe);
                port->pending_iframe = 0;
            }

            WdfSpinLockRelease(port->pending_iframe_spinlock);
            WdfSpinLockRelease(port->board_rx_spinlock);
            return;
        }

        if (!port->pending_iframe) {
            port->pending_iframe = fscc_frame_new(port);

            if (!port->pending_iframe) {
                WdfSpinLockRelease(port->pending_iframe_spinlock);
                WdfSpinLockRelease(port->board_rx_spinlock);
                return;
            }
        }

        status = fscc_frame_add_data_from_port(port->pending_iframe, port, receive_length);
        if (status == FALSE) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                "Error adding stream data");
            WdfSpinLockRelease(port->pending_iframe_spinlock);
            WdfSpinLockRelease(port->board_rx_spinlock);
            return;
        }

    #ifdef __BIG_ENDIAN
        {
            char status[STATUS_LENGTH];

            /* Moves the status bytes to the end. */
            memmove(&status, port->pending_iframe->data, STATUS_LENGTH);
            memmove(port->pending_iframe->data, port->pending_iframe->data + STATUS_LENGTH, port->pending_iframe->current_length - STATUS_LENGTH);
            memmove(port->pending_iframe->data + port->pending_iframe->current_length - STATUS_LENGTH, &status, STATUS_LENGTH);
        }
    #endif

        TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE,
            "F#%i <= %i byte%s (%sfinished)",
            port->pending_iframe->number, receive_length,
            (receive_length == 1) ? "" : "s",
            (finished_frame) ? "" : "un");

        if (!finished_frame) {
            WdfSpinLockRelease(port->pending_iframe_spinlock);
            WdfSpinLockRelease(port->board_rx_spinlock);
            return;
        }

        if (port->pending_iframe) {
            KeQuerySystemTime(&port->pending_iframe->timestamp);
            WdfSpinLockAcquire(port->queued_iframes_spinlock);
            fscc_flist_add_frame(&port->queued_iframes, port->pending_iframe);
            WdfSpinLockRelease(port->queued_iframes_spinlock);
        }

        rejected_last_frame = 0; /* Track that we received a frame to reset the
                                memory constraint warning print message. */

        port->pending_iframe = 0;

        WdfSpinLockRelease(port->pending_iframe_spinlock);
        WdfSpinLockRelease(port->board_rx_spinlock);

        WdfDpcEnqueue(port->process_read_dpc);
    }
    while (receive_length);
}

/* This function is syncronized so we don't have to worry about it being ran in
   parallel */
void istream_worker(WDFDPC Dpc)
{
    struct fscc_port *port = 0;
    int receive_length = 0; /* Needs to be signed */
    unsigned rxcnt = 0;
    unsigned current_memory = 0;
    unsigned memory_cap = 0;
    static int rejected_last_stream = 0;
    int status;

    port = WdfObjectGet_FSCC_PORT(WdfDpcGetParentObject(Dpc));

    return_if_untrue(port);

    current_memory = fscc_port_get_input_memory_usage(port);
    memory_cap = fscc_port_get_input_memory_cap(port);

    /* Leave the interrupt handler if we are at our memory cap. */
    if (current_memory >= memory_cap) {
        if (rejected_last_stream == 0) {
            TraceEvents(TRACE_LEVEL_WARNING, TRACE_DEVICE,
                        "Rejecting stream (memory constraint)");
            rejected_last_stream = 1; /* Track that we dropped stream data so we
                                         don't have to warn the user again. */
        }

        return;
    }

    WdfSpinLockAcquire(port->board_rx_spinlock);

    rxcnt = fscc_port_get_RXCNT(port);

    /* We choose a safe amount to read due to more data coming in after we
        get our values. The rest will be read on the next interrupt. */
    receive_length = rxcnt - MAX_LEFTOVER_BYTES;
    receive_length -= receive_length % 4;

    /* Leave the interrupt handler if there is no data to read. */
    if (receive_length <= 0) {
        WdfSpinLockRelease(port->board_rx_spinlock);
        return;
    }

    /* Trim the amount to read if there isn't enough memory space to read all
       of it. */
    if (receive_length + current_memory > memory_cap)
        receive_length = memory_cap - current_memory;

    WdfSpinLockAcquire(port->istream_spinlock);
    status = fscc_frame_add_data_from_port(port->istream, port, receive_length);
    WdfSpinLockRelease(port->istream_spinlock);
    if (status == FALSE) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
            "Error adding stream data");
        WdfSpinLockRelease(port->board_rx_spinlock);
        return;
    }

    WdfSpinLockRelease(port->board_rx_spinlock);

    rejected_last_stream = 0; /* Track that we received stream data to reset
                                the memory constraint warning print message.
                            */

    TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DEVICE,
        "Stream <= %i byte%s", receive_length,
            (receive_length == 1) ? "" : "s");

    WdfDpcEnqueue(port->process_read_dpc);
}

void clear_oframe_worker(WDFDPC Dpc)
{
    struct fscc_port *port = 0;
    struct fscc_frame *frame = 0;
    unsigned remove = 0;

    port = WdfObjectGet_FSCC_PORT(WdfDpcGetParentObject(Dpc));

    WdfSpinLockAcquire(port->sent_oframes_spinlock);

    frame = fscc_flist_peak_front(&port->sent_oframes);

    if (!frame) {
        WdfSpinLockRelease(port->sent_oframes_spinlock);
        return;
    }

    //if (fscc_frame_is_dma(frame)) {
    //	if (frame->d1->control == 0x40000000)
    //		remove = 1;
    //}
    //else {
        remove = 1;
    //}

    if (remove) {
        fscc_flist_remove_frame(&port->sent_oframes);
        fscc_frame_delete(frame);

        if (!fscc_flist_is_empty(&port->sent_oframes))
            WdfDpcEnqueue(port->clear_oframe_dpc);

        if (port->wait_on_write) {
            NTSTATUS status = STATUS_SUCCESS;
            WDFREQUEST request;
            WDF_REQUEST_PARAMETERS params;
            unsigned length = 0;

            status = WdfIoQueueRetrieveNextRequest(port->write_queue2, &request);
            if (!NT_SUCCESS(status)) {
                if (status != STATUS_NO_MORE_ENTRIES) {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                                "WdfIoQueueRetrieveNextRequest failed %!STATUS!",
                                status);
                }

                WdfSpinLockRelease(port->sent_oframes_spinlock);
                return;
            }

            WDF_REQUEST_PARAMETERS_INIT(&params);
            WdfRequestGetParameters(request, &params);
            length = (unsigned)params.Parameters.Write.Length;

            WdfRequestCompleteWithInformation(request, status, length);
        }
    }

    WdfSpinLockRelease(port->sent_oframes_spinlock);
}

void oframe_worker(WDFDPC Dpc)
{
    struct fscc_port *port = 0;

    unsigned fifo_space = 0;
    unsigned current_length = 0;
    unsigned buffer_size = 0;
    unsigned transmit_length = 0;
    unsigned size_in_fifo = 0;

    int result;

    port = WdfObjectGet_FSCC_PORT(WdfDpcGetParentObject(Dpc));

    return_if_untrue(port);

    WdfSpinLockAcquire(port->board_tx_spinlock);
    WdfSpinLockAcquire(port->pending_oframe_spinlock);

    /* Check if exists and if so, grabs the frame to transmit. */
    if (!port->pending_oframe) {
        WdfSpinLockAcquire(port->queued_oframes_spinlock);
        port->pending_oframe = fscc_flist_remove_frame(&port->queued_oframes);
        WdfSpinLockRelease(port->queued_oframes_spinlock);

        /* No frames in queue to transmit */
        if (!port->pending_oframe) {
            WdfSpinLockRelease(port->pending_oframe_spinlock);
            WdfSpinLockRelease(port->board_tx_spinlock);
            return;
        }
    }

    result = fscc_port_transmit_frame(port, port->pending_oframe);

    if (result == 2) {
        WdfSpinLockAcquire(port->sent_oframes_spinlock);
        fscc_flist_add_frame(&port->sent_oframes, port->pending_oframe);
        WdfSpinLockRelease(port->sent_oframes_spinlock);

        port->pending_oframe = 0;
	}

    WdfSpinLockRelease(port->pending_oframe_spinlock);
    WdfSpinLockRelease(port->board_tx_spinlock);

    //if (result == 2)
    //	wake_up_interruptible(&port->output_queue);
}

VOID timer_handler(WDFTIMER Timer)
{
    struct fscc_port *port = 0;
    unsigned streaming = 0;

    port = WdfObjectGet_FSCC_PORT(WdfTimerGetParentObject(Timer));

    streaming = fscc_port_is_streaming(port);

    if (streaming)
        WdfDpcEnqueue(port->istream_dpc);
    //else
    //	WdfDpcEnqueue(port->iframe_dpc);
}