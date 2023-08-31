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
 * File: $Id: portserial_m.c,v 1.60 2013/08/13 15:07:05 Armink add Master Functions $
 */
#include "mhscpu_uart.h"
#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"
#include "serial.h"
#include"RTE_Components.h"


#if MB_MASTER_RTU_ENABLED > 0 || MB_MASTER_ASCII_ENABLED > 0
/* ----------------------- Static variables ---------------------------------*/
//ALIGN(RT_ALIGN_SIZE)
/* software simulation serial transmit IRQ handler thread stack */
/* software simulation serial transmit IRQ handler thread */
/* serial event */
static osEventFlagsId_t event_serial;
/* modbus master serial device */

/* ----------------------- Defines ------------------------------------------*/
/* serial transmit event */
#define EVENT_SERIAL_TRANS_START    (1<<0)

/* ----------------------- static functions ---------------------------------*/
static void prvvUARTTxReadyISR(void);
static void prvvUARTRxISR(void);
static void serial_soft_trans_irq(void *parameter);
static void Master_RxCpltCallback(void);
void startMasterSendTask(void);

static UART_TypeDef *s_ucSerialID_m;

osThreadId_t modbusMasterTaskHandle = NULL;
static const osThreadAttr_t modbusMasterTask_attributes =
{
    .name = "MODBUS M-TX",
    .priority = (osPriority_t)osPriorityHigh,
    .stack_size = 256 * 4
};

/* ----------------------- Start implementation -----------------------------*/
BOOL xMBMasterPortSerialInit(UCHAR ucPORT, ULONG ulBaudRate, UCHAR ucDataBits, eMBParity eParity)
{
    if (ucPORT == 0)
    {
        s_ucSerialID_m = UART0;
    }
    else if (ucPORT == 1)
    {
        s_ucSerialID_m = UART1;
    }
    else if (ucPORT == 2)
    {
        s_ucSerialID_m = UART2;
    }else
    {
        return FALSE;
    }
    serialRegisterCallback(s_ucSerialID_m, HAL_UART_RX_COMPLETE_CB_ID, Master_RxCpltCallback);
    serialOpen(s_ucSerialID_m, ulBaudRate);
    return TRUE;
}

void vMBMasterPortSerialEnable(BOOL xRxEnable, BOOL xTxEnable)
{
    uint32_t recved_event;
    int ret;
#if MB_MASTER_SLAVE_AIO
    serialRegisterCallback(s_ucSerialID_m, HAL_UART_RX_COMPLETE_CB_ID, Master_RxCpltCallback);
#endif
    if (xRxEnable)
    {
        endisSerialRecvIRQ(s_ucSerialID_m, 1);
    }
    else
    {
        endisSerialRecvIRQ(s_ucSerialID_m, 0);
    }
    if (xTxEnable)
    {
        ret = (int)osEventFlagsSet(event_serial, EVENT_SERIAL_TRANS_START);
        assert_param(ret >= 0);
    }
    else
    {
        ret = osEventFlagsClear(event_serial, EVENT_SERIAL_TRANS_START);

    }
}

void vMBMasterPortClose(void)
{
#if MB_MASTER_SLAVE_AIO
#else
    serialClose(s_ucSerialID_m);
#endif
}

BOOL xMBMasterPortSerialPutByte(CHAR ucByte)
{
    serialputc(s_ucSerialID_m, ucByte);
    return TRUE;
}

BOOL xMBMasterPortSerialGetByte(CHAR *pucByte)
{
    int ch = serialgetc(s_ucSerialID_m);
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
static void prvvUARTTxReadyISR(void)
{
    pxMBMasterFrameCBTransmitterEmpty();
}

/*
 * Create an interrupt handler for the receive interrupt for your target
 * processor. This function should then call pxMBFrameCBByteReceived( ). The
 * protocol stack will then call xMBPortSerialGetByte( ) to retrieve the
 * character.
 */
static void prvvUARTRxISR(void)
{
    pxMBMasterFrameCBByteReceived();
}

/**
 * Software simulation serial transmit IRQ handler.
 *
 * @param parameter parameter
 */
static void serial_soft_trans_irq(void *parameter)
{
    uint32_t recved_event;
    while (1)
    {
        /* waiting for serial transmit start */
        recved_event = osEventFlagsWait(event_serial, EVENT_SERIAL_TRANS_START, osFlagsNoClear | osFlagsWaitAny, osWaitForever);
        /* execute modbus callback */
        prvvUARTTxReadyISR();
    }
}

///**
// * This function is serial receive callback function
// *
// * @param dev the device of serial
// * @param size the data size that receive
// *
// * @return return RT_EOK
// */
//static rt_err_t serial_rx_ind(rt_device_t dev, rt_size_t size) {
//    prvvUARTRxISR();
//    return RT_EOK;
//}
void Master_RxCpltCallback(void)
{
    prvvUARTRxISR();
}

void startMasterSendTask(void)
{
    if(modbusMasterTaskHandle == NULL)
    {
        event_serial = osEventFlagsNew(NULL);
        assert_param(event_serial != NULL);
        modbusMasterTaskHandle = osThreadNew(serial_soft_trans_irq, NULL, &modbusMasterTask_attributes);
        assert_param(modbusMasterTaskHandle != NULL);
        register_task_handle(modbusMasterTaskHandle);
    }
}
#endif
