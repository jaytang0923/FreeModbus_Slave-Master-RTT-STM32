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

#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"
#include "serial.h"
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
    // rt_pin_mode(MODBUS_SLAVE_RT_CONTROL_PIN_INDEX, PIN_MODE_OUTPUT);
    /* set serial name */
    /*registe recieve callback*/
    //  HAL_UART_RegisterCallback(serial, HAL_UART_RX_COMPLETE_CB_ID,
    //                            Slave_RxCpltCallback);
    serialRegisterCallback(0, HAL_UART_RX_COMPLETE_CB_ID, Slave_RxCpltCallback);
    serialinit(0, 115200);
    /* software initialize */
    /* ????????*/
    event_serial = osEventFlagsNew(NULL); //????
    assert_param(event_serial);
    serialTxTask = osThreadNew(serialSendTask, NULL, &serialTx_attributes);
    assert_param(serialTxTask != NULL);
    return TRUE;
}

void vMBPortSerialEnable(BOOL xRxEnable, BOOL xTxEnable)
{
    //    __HAL_UART_CLEAR_FLAG(serial,UART_FLAG_RXNE);
    //   __HAL_UART_CLEAR_FLAG(serial,UART_FLAG_TC);
    // TODO: clean flags
    if (xRxEnable == FALSE && xTxEnable == FALSE)
    {
        MODBUS_DEBUG("STOP MODBUS\n");
    }

    if (xRxEnable)
    {
        /* enable RX interrupt */
        //    __HAL_UART_ENABLE_IT(serial, UART_IT_RXNE);
        endisSerialRecvIRQ(0, 1);
        /* switch 485 to receive mode */
        MODBUS_DEBUG("RS232_RX_MODE ENABLE\r\n");
        //    SLAVE_RS485_RX_MODE;
    }
    else
    {
        /* switch 485 to transmit mode */
        //MODBUS_DEBUG("RS232_TX_MODE\r\n");
        //    SLAVE_RS485_TX_MODE;
        /* disable RX interrupt */
        endisSerialRecvIRQ(0, 0);
        MODBUS_DEBUG("RS232_RX_MODE DISABLE\r\n");
    }

    if (xTxEnable)
    {
        MODBUS_DEBUG("RS232_TX_ENABLE\r\n");
        /* start serial transmit */
        uint32_t evt = osEventFlagsSet(event_serial, EVENT_SERIAL_TRANS_START);
        MODBUS_DEBUG("send event %x %p\r\n", evt, event_serial);
    }
    else
    {
        MODBUS_DEBUG("RS232_TX_DISABLE\r\n");
        /* stop serial transmit */
        osEventFlagsClear(event_serial, EVENT_SERIAL_TRANS_START);
        /*????*/
        // printf("ms=%.2f,fps=%.2f\r\n", __HAL_TIM_GetCounter(&htim7) / 100.f,
        // 1000.f / (__HAL_TIM_GetCounter(&htim7) / 100.f));
    }
}

void vMBPortClose(void)
{
    serialclose(0);
}
/*Send a byte*/
BOOL xMBPortSerialPutByte(CHAR ucByte)
{
    dbg("%02X", ucByte);
    serialputc(0, ucByte);
    return TRUE;
}
/*Get a byte from fifo*/
BOOL xMBPortSerialGetByte(CHAR *pucByte)
{
#if 0
    Get_from_fifo(&Slave_serial_rx_fifo, (uint8_t *)pucByte, 1);
#else
    int ch = serialgetc(0);

    if (ch != -1)
    {
        *pucByte = (CHAR)ch & 0xff;
        printf("%02X\n", ch);
    }

#endif
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
        MODBUS_DEBUG("serialSendTask waiting ...");
        /* waiting for serial transmit start */
        osEventFlagsWait(event_serial, EVENT_SERIAL_TRANS_START, osFlagsNoClear | osFlagsWaitAny, osWaitForever);
        /* execute modbus callback */
        prvvUARTTxReadyISR();
        MODBUS_DEBUG(">");
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
#if 0
    int ch = -1;

    do
    {
        ch = serialgetc(0);

        if (ch == -1)
        {
            break;
        }

        printf("%02X ", ch);
        Put_in_fifo(&Slave_serial_rx_fifo, (uint8_t *)&ch, 1);
    }
    while (1);

#else
    // int ch = -1;
    /*
    do not take too much time,in case of T35 timeout.
    */
    //    do
    //    {
    //        ch = serialgetc(0);
    //        if (ch == -1)
    //            break;
    //        Put_in_fifo(&Slave_serial_rx_fifo, (uint8_t *)&ch, 1);
    //    }
    //    while (0);
#endif
    prvvUARTRxISR();
}
