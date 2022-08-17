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
static osEventFlagsId_t event_serial;
/* modbus slave serial device */
void Slave_RxCpltCallback(void);

/*
 * Serial FIFO mode
 */
static UART_TypeDef *s_ucSerialID;

/* ----------------------- Defines ------------------------------------------*/
/* serial transmit event */
#define EVENT_SERIAL_TRANS_START (1 << 0)

/* ----------------------- static functions ---------------------------------*/
static void prvvUARTTxReadyISR(void);
static void prvvUARTRxISR(void);
static void serialSendTask(void *parameter);

static osThreadId_t serialTxTask;
static const osThreadAttr_t serialTx_attributes =
{
    .name = "SlaveTX",
    .priority = (osPriority_t)osPriorityHigh,
    .stack_size = 256 * 2
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
    }

    serialRegisterCallback(s_ucSerialID, HAL_UART_RX_COMPLETE_CB_ID, Slave_RxCpltCallback);
    serialOpen(s_ucSerialID, 115200);
    /* software initialize */
    event_serial = osEventFlagsNew(NULL); //????
    assert_param(event_serial);
    serialTxTask = osThreadNew(serialSendTask, NULL, &serialTx_attributes);
    assert_param(serialTxTask != NULL);
    return TRUE;
}

void vMBPortSerialEnable(BOOL xRxEnable, BOOL xTxEnable)
{
    // TODO: clean flags
    if (xRxEnable == FALSE && xTxEnable == FALSE)
    {
        MODBUS_DEBUG("STOP MODBUS\n");
    }

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
        osEventFlagsSet(event_serial, EVENT_SERIAL_TRANS_START);
    }
    else
    {
        /* stop serial transmit */
        osEventFlagsClear(event_serial, EVENT_SERIAL_TRANS_START);
    }
}

void vMBPortClose(void)
{
    serialClose(s_ucSerialID);
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
    //uint32_t recved_event;
    while (1)
    {
        /* waiting for serial transmit start */
        osEventFlagsWait(event_serial, EVENT_SERIAL_TRANS_START, osFlagsNoClear | osFlagsWaitAny, osWaitForever);
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
