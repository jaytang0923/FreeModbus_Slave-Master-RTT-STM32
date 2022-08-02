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

/* ----------------------- Static variables ---------------------------------*/
/* software simulation serial transmit IRQ handler thread */
static TaskHandle_t thread_serial_soft_trans_irq = NULL;
/* serial event */
static EventGroupHandle_t event_serial;
/* modbus slave serial device */
void Slave_RxCpltCallback(void);

/*
 * Serial FIFO mode
 */

static volatile uint8_t rx_buff[FIFO_SIZE_MAX];
static Serial_fifo Slave_serial_rx_fifo;
/* ----------------------- Defines ------------------------------------------*/
/* serial transmit event */
#define EVENT_SERIAL_TRANS_START (1 << 0)

/* ----------------------- static functions ---------------------------------*/
static void prvvUARTTxReadyISR(void);
static void prvvUARTRxISR(void);
static void serial_soft_trans_irq(void *parameter);

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
    Slave_serial_rx_fifo.buffer = rx_buff;
    Slave_serial_rx_fifo.get_index = 0;
    Slave_serial_rx_fifo.put_index = 0;

    /* ????????*/
    event_serial = xEventGroupCreate(); //????
    if (NULL != event_serial)
    {
        MODBUS_DEBUG("Create Slave event_serial Event success!\r\n");
    }
    else
    {
        MODBUS_DEBUG("Create Slave event_serial Event  Faild!\r\n");
        assert_param(0);
    }
    BaseType_t xReturn =
        xTaskCreate((TaskFunction_t)serial_soft_trans_irq, /* ????*/
                    (const char *)"slave trans",           /* ????*/
                    (uint16_t)256,                         /* ?*/
                    (void *)NULL,                          /* ???? */
                    (UBaseType_t)12,                       /* ???*/
                    (TaskHandle_t *)&thread_serial_soft_trans_irq); /*????*/

    if (xReturn == pdPASS)
    {
        MODBUS_DEBUG("xTaskCreate slave trans success\r\n");
    }
    return TRUE;
}

void vMBPortSerialEnable(BOOL xRxEnable, BOOL xTxEnable)
{
//    __HAL_UART_CLEAR_FLAG(serial,UART_FLAG_RXNE);
//   __HAL_UART_CLEAR_FLAG(serial,UART_FLAG_TC);
    // TODO: clean flags
    if (xRxEnable == FALSE && xTxEnable == FALSE)
    {
        printf("STOP MODBUS\n");
    }
    if (xRxEnable)
    {
        /* enable RX interrupt */
//    __HAL_UART_ENABLE_IT(serial, UART_IT_RXNE);
        endisSerialRecvIRQ(0, 1);
        /* switch 485 to receive mode */
        //MODBUS_DEBUG("RS232_RX_MODE\r\n");
//    SLAVE_RS485_RX_MODE;
    }
    else
    {
        /* switch 485 to transmit mode */
        //MODBUS_DEBUG("RS232_TX_MODE\r\n");
//    SLAVE_RS485_TX_MODE;
        /* disable RX interrupt */
        endisSerialRecvIRQ(0, 0);
    }
    if (xTxEnable)
    {
        /* start serial transmit */
        xEventGroupSetBits(event_serial, EVENT_SERIAL_TRANS_START);
    }
    else
    {
        /* stop serial transmit */
        xEventGroupClearBits(event_serial, EVENT_SERIAL_TRANS_START);
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
static void serial_soft_trans_irq(void *parameter)
{
    //uint32_t recved_event;
    while (1)
    {
        /* waiting for serial transmit start */
        xEventGroupWaitBits(event_serial,             /* ?????? */
                            EVENT_SERIAL_TRANS_START, /* ?????????? */
                            pdFALSE,                  /* ????????? */
                            pdFALSE,        /* ??????????? */
                            portMAX_DELAY); /* ??????,???? */
        /* execute modbus callback */
        prvvUARTTxReadyISR();
        //printf("Sendwater:%d\n", uxTaskGetStackHighWaterMark(NULL));
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
            break;
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
