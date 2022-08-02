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

osThreadId_t modbusMasterTaskHandle;
static const osThreadAttr_t modbusMasterTask_attributes =
{
    .name = "master uart trans",
    .priority = (osPriority_t)osPriorityHigh,
    .stack_size = 256 * 4
};

/* ----------------------- Start implementation -----------------------------*/
BOOL xMBMasterPortSerialInit(UCHAR ucPORT, ULONG ulBaudRate, UCHAR ucDataBits,
                             eMBParity eParity)
{
#if 0
    /**
     * set 485 mode receive and transmit control IO
     * @note MODBUS_MASTER_RT_CONTROL_PIN_INDEX need be defined by user
     */
    rt_pin_mode(MODBUS_MASTER_RT_CONTROL_PIN_INDEX, PIN_MODE_OUTPUT);

    /* set serial name */
    if (ucPORT == 1)
    {
#if defined(RT_USING_UART1) || defined(RT_USING_REMAP_UART1)
        extern struct rt_serial_device serial1;
        serial = &serial1;
#endif
    }
    else if (ucPORT == 2)
    {
#if defined(RT_USING_UART2)
        extern struct rt_serial_device serial2;
        serial = &serial2;
#endif
    }
    else if (ucPORT == 3)
    {
#if defined(RT_USING_UART3)
        extern struct rt_serial_device serial3;
        serial = &serial3;
#endif
    }
    /* set serial configure parameter */
    serial->config.baud_rate = ulBaudRate;
    serial->config.stop_bits = STOP_BITS_1;
    switch (eParity)
    {
        case MB_PAR_NONE:
        {
            serial->config.data_bits = DATA_BITS_8;
            serial->config.parity = PARITY_NONE;
            break;
        }
        case MB_PAR_ODD:
        {
            serial->config.data_bits = DATA_BITS_9;
            serial->config.parity = PARITY_ODD;
            break;
        }
        case MB_PAR_EVEN:
        {
            serial->config.data_bits = DATA_BITS_9;
            serial->config.parity = PARITY_EVEN;
            break;
        }
    }
    /* set serial configure */
    serial->ops->configure(serial, &(serial->config));

    /* open serial device */
    if (!serial->parent.open(&serial->parent,
                             RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX))
    {
        serial->parent.rx_indicate = serial_rx_ind;
    }
    else
    {
        return FALSE;
    }

    /* software initialize */
    rt_event_init(&event_serial, "master event", RT_IPC_FLAG_PRIO);
    rt_thread_init(&thread_serial_soft_trans_irq,
                   "master trans",
                   serial_soft_trans_irq,
                   RT_NULL,
                   serial_soft_trans_irq_stack,
                   sizeof(serial_soft_trans_irq_stack),
                   10, 5);
    rt_thread_startup(&thread_serial_soft_trans_irq);
#else
    serialRegisterCallback(0, HAL_UART_RX_COMPLETE_CB_ID, Master_RxCpltCallback);
    serialinit(0, 115200);

    event_serial = osEventFlagsNew(NULL);
    assert_param(event_serial != NULL);

    modbusMasterTaskHandle = osThreadNew(serial_soft_trans_irq, NULL, &modbusMasterTask_attributes);
    assert_param(modbusMasterTaskHandle != NULL);

#endif
    return TRUE;
}

void vMBMasterPortSerialEnable(BOOL xRxEnable, BOOL xTxEnable)
{
    uint32_t recved_event;
#if 0
    if (xRxEnable)
    {
        /* enable RX interrupt */
        serial->ops->control(serial, RT_DEVICE_CTRL_SET_INT, (void *)RT_DEVICE_FLAG_INT_RX);
        /* switch 485 to receive mode */
        rt_pin_write(MODBUS_MASTER_RT_CONTROL_PIN_INDEX, PIN_LOW);
    }
    else
    {
        /* switch 485 to transmit mode */
        rt_pin_write(MODBUS_MASTER_RT_CONTROL_PIN_INDEX, PIN_HIGH);
        /* disable RX interrupt */
        serial->ops->control(serial, RT_DEVICE_CTRL_CLR_INT, (void *)RT_DEVICE_FLAG_INT_RX);
    }
    if (xTxEnable)
    {
        /* start serial transmit */
        rt_event_send(&event_serial, EVENT_SERIAL_TRANS_START);
    }
    else
    {
        /* stop serial transmit */
        rt_event_recv(&event_serial, EVENT_SERIAL_TRANS_START,
                      RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, 0,
                      &recved_event);
    }
#else
    if (xRxEnable)
    {
        endisSerialRecvIRQ(0, 1);
    }
    else
    {
        endisSerialRecvIRQ(0, 0);
    }
    if (xTxEnable)
    {
        osEventFlagsSet(event_serial, EVENT_SERIAL_TRANS_START);
    }
    else
    {
        osEventFlagsClear(event_serial, EVENT_SERIAL_TRANS_START);

    }
#endif
}

void vMBMasterPortClose(void)
{
    //serial->parent.close(&(serial->parent));
    serialclose(0);
}

BOOL xMBMasterPortSerialPutByte(CHAR ucByte)
{
    //serial->parent.write(&(serial->parent), 0, &ucByte, 1);
    serialputc(0, ucByte);
    return TRUE;
}

BOOL xMBMasterPortSerialGetByte(CHAR *pucByte)
{
    //serial->parent.read(&(serial->parent), 0, pucByte, 1);
    int ch = serialgetc(0);
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
#if 0
    while (1)
    {
        /* waiting for serial transmit start */
        rt_event_recv(&event_serial, EVENT_SERIAL_TRANS_START, RT_EVENT_FLAG_OR,
                      RT_WAITING_FOREVER, &recved_event);
        /* execute modbus callback */
        prvvUARTTxReadyISR();
    }
#else
    while (1)
    {
        /* waiting for serial transmit start */
        recved_event = osEventFlagsWait(event_serial, EVENT_SERIAL_TRANS_START, osFlagsWaitAny, osWaitForever);
        /* execute modbus callback */
        prvvUARTTxReadyISR();
    }
#endif
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
#endif
