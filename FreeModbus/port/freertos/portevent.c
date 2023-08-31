/*
 * FreeModbus Libary: RT-Thread Port
 * Copyright (C) 2013 Armink <armink.ztl@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * File: $Id: portevent.c,v 1.60 2013/08/13 15:07:05 Armink $
 */

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"


/* ----------------------- Variables ----------------------------------------*/
static osThreadId_t xSlaveThreadId;
/* ----------------------- Start implementation -----------------------------*/
BOOL xMBPortEventInit(void)
{
    xSlaveThreadId = osThreadGetId();
    return TRUE;
}

BOOL xMBPortEventPost(eMBEventType eEvent)
{
    int ret = osThreadFlagsSet(xSlaveThreadId, eEvent);

    if (ret < 0) {
        err("xMBPortEventPost error: %d", ret);
        return FALSE;
    }

    return TRUE;
}

#define EV_EXIT     (1<<4)
static BOOL s_slave_idle = FALSE;
static BOOL s_slave_exit = FALSE;

BOOL xMBPortEventGet(eMBEventType *eEvent)
{
    uint32_t recvedEvent;
    /* waiting forever OS event */
    recvedEvent = osThreadFlagsWait(EV_READY | EV_FRAME_RECEIVED | EV_EXECUTE | EV_FRAME_SENT | EV_EXIT, osFlagsWaitAny, osWaitForever);

    if ((int)recvedEvent < 0) {
        err("xMBPortEventGet error %d", (int)recvedEvent);
    }

    switch (recvedEvent) {
        case EV_READY:
            *eEvent = EV_READY;
            break;

        case EV_FRAME_RECEIVED:
            *eEvent = EV_FRAME_RECEIVED;
            break;

        case EV_EXECUTE:
            *eEvent = EV_EXECUTE;
            break;

        case EV_FRAME_SENT:
            *eEvent = EV_FRAME_SENT;
            break;

        case EV_EXIT:
            *eEvent = EV_EXIT;

            if (s_slave_idle == TRUE) {
                s_slave_exit = TRUE;
            }

            break;
    }

    if (recvedEvent == EV_READY) {
        s_slave_idle = TRUE;
    } else {
        s_slave_idle = FALSE;
    }

    return TRUE;
}

void vMBSlaveInit(void)
{
    s_slave_idle = FALSE;
    s_slave_exit = FALSE;
}

BOOL xMBPortEventExit(void)
{
    return s_slave_exit;
}

BOOL xMBPortEventSetExit(void)
{
    return xMBPortEventPost(EV_EXIT);
}
