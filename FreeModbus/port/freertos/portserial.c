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
 * File: $Id: portserial.c,v 1.60 2013/08/13 15:07:05 Armink $
 */
#include "mhscpu_uart.h"
#include "serial.h"
#include "cmsis_os2.h"
#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"

#include "debug.h"
/* ----------------------- Static variables ---------------------------------*/
/* software simulation serial transmit IRQ handler thread */

/* serial event */
static osThreadId_t s_TxThreadId = NULL;
/* modbus slave serial device */
void Slave_RxCpltCallback(void);
void startSerialSendTask(void);
/*
 * Serial FIFO mode
 */
static UART_TypeDef *s_ucSerialID;

/* ----------------------- Defines ------------------------------------------*/
/* serial transmit event */
#define EVENT_SERIAL_TRANS_START (1 << 0)
#define EVENT_SERIAL_TRANS_STOP  (1 << 1)

/* ----------------------- static functions ---------------------------------*/
static void prvvUARTTxReadyISR(void);
static void prvvUARTRxISR(void);
static void serialSendTask(void *parameter);

static const osThreadAttr_t serialTx_attributes =
{
    .name = "SlaveTX",
    .priority = (osPriority_t)osPriorityHigh,
    .stack_size = 256 * 4
};

/* ----------------------- Start implementation -----------------------------*/
BOOL xMBPortSerialInit(UCHAR ucPORT, ULONG ulBaudRate, UCHAR ucDataBits,
                       eMBParity eParity)
{
    /**
     * set 485 mode receive and transmit control IO
     * @note MODBUS_SLAVE_RT_CONTROL_PIN_INDEX need be defined by user
     */
    if (ucPORT == 0)
    {
        s_ucSerialID = UART0;
    }
    else if (ucPORT == 1)
    {
        s_ucSerialID = UART1;
    }
    else if (ucPORT == 2)
    {
        s_ucSerialID = UART2;
    }else
    {
        return FALSE;
    }

    serialRegisterCallback(s_ucSerialID, HAL_UART_RX_COMPLETE_CB_ID, Slave_RxCpltCallback);
    serialOpen(s_ucSerialID, ulBaudRate);
    /* software initialize */
    // startSerialSendTask();
    return TRUE;
}

void vMBPortSerialEnable(BOOL xRxEnable, BOOL xTxEnable)
{
    // TODO: clean flags
    if (xRxEnable == FALSE && xTxEnable == FALSE)
    {
        MODBUS_DEBUG("STOP MODBUS\n");
    }
#if MB_MASTER_SLAVE_AIO
    serialRegisterCallback(s_ucSerialID, HAL_UART_RX_COMPLETE_CB_ID, Slave_RxCpltCallback);
#endif
    if (xRxEnable)
    {
        /* enable RX interrupt */
        endisSerialRecvIRQ(s_ucSerialID, 1);
        /* switch 485 to receive mode */
    }
    else
    {
        /* switch 485 to transmit mode */
        /* disable RX interrupt */
        endisSerialRecvIRQ(s_ucSerialID, 0);
    }

    if (xTxEnable)
    {
        /* start serial transmit */
        osThreadFlagsSet(s_TxThreadId, EVENT_SERIAL_TRANS_START);
    }
    else
    {
        /* stop serial transmit */
        osThreadFlagsSet(s_TxThreadId, EVENT_SERIAL_TRANS_STOP);
    }
}

void vMBPortClose(void)
{
#if MB_MASTER_SLAVE_AIO
#else
    serialClose(s_ucSerialID);
#endif
}
/*Send a byte*/
BOOL xMBPortSerialPutByte(CHAR ucByte)
{
    serialputc(s_ucSerialID, ucByte);
    return TRUE;
}
/*Get a byte from fifo*/
BOOL xMBPortSerialGetByte(CHAR *pucByte)
{
    int ch = serialgetc(s_ucSerialID);

    if (ch != -1)
    {
        *pucByte = (CHAR)ch & 0xff;
    }

    return TRUE;
}

/*
 * Create an interrupt handler for the transmit buffer empty interrupt
 * (or an equivalent) for your target processor. This function should then
 * call pxMBFrameCBTransmitterEmpty( ) which tells the protocol stack that
 * a new character can be sent. The protocol stack will then call
 * xMBPortSerialPutByte( ) to send the character.
 */
void prvvUARTTxReadyISR(void)
{
    pxMBFrameCBTransmitterEmpty();
}

/*
 * Create an interrupt handler for the receive interrupt for your target
 * processor. This function should then call pxMBFrameCBByteReceived( ). The
 * protocol stack will then call xMBPortSerialGetByte( ) to retrieve the
 * character.
 */
void prvvUARTRxISR(void)
{
    pxMBFrameCBByteReceived();
}

/**
 * Software simulation serial transmit IRQ handler.
 *
 * @param parameter parameter
 */
static void serialSendTask(void *parameter)
{
    uint32_t flags, tmout = osWaitForever;
    while (1)
    {
        /* waiting for serial transmit start */
        //FIXME: osFlagsNoClear not work
        flags = osThreadFlagsWait(EVENT_SERIAL_TRANS_START| EVENT_SERIAL_TRANS_STOP, osFlagsNoClear | osFlagsWaitAny, tmout);
        if(flags & EVENT_SERIAL_TRANS_STOP)
        {
            tmout = osWaitForever;
            osThreadFlagsClear(EVENT_SERIAL_TRANS_START | EVENT_SERIAL_TRANS_STOP);
            continue;
        }
        tmout = 0;
        /* execute modbus callback */
        prvvUARTTxReadyISR();
    }
}

/**
 * @brief  Rx Transfer completed callbacks.
 * @param  huart  Pointer to a UART_HandleTypeDef structure that contains
 *                the configuration information for the specified UART module.
 * @retval None
 */
void Slave_RxCpltCallback(void)
{
    prvvUARTRxISR();
}

void startSerialSendTask(void)
{
    if(s_TxThreadId == NULL)
    {
        s_TxThreadId = osThreadNew(serialSendTask, NULL, &serialTx_attributes);
        assert_param(s_TxThreadId != NULL);
        register_task_handle(s_TxThreadId);
    }
}
