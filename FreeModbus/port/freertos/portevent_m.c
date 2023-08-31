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
 * File: $Id: portevent_m.c v 1.60 2013/08/13 15:07:05 Armink add Master Functions$
 */

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mb_m.h"
#include "mbport.h"
#include "port.h"

#if MB_MASTER_RTU_ENABLED > 0 || MB_MASTER_ASCII_ENABLED > 0

#define EV_MASTER_EXIT     (1<<9)

/* ----------------------- Defines ------------------------------------------*/
/* ----------------------- Variables ----------------------------------------*/
static  osEventFlagsId_t xMasterOsEvent;
static osSemaphoreId_t xMasterRunRes;
static BOOL bMasterExit = FALSE;

/* ----------------------- Start implementation -----------------------------*/
BOOL xMBMasterPortEventInit(void)
{
    xMasterOsEvent = osEventFlagsNew(NULL);
    assert_param(xMasterOsEvent != NULL);
    return TRUE;
}

BOOL xMBMasterPortEventPost(eMBMasterEventType eEvent)
{
    uint32_t ret = osEventFlagsSet(xMasterOsEvent, eEvent);
    //assert_param(ret == eEvent);
    return TRUE;
}

BOOL
xMBMasterPortEventGet(eMBMasterEventType *eEvent)
{
    uint32_t recvedEvent;
    /* waiting forever OS event */
#if 0
    osEventFlagsWait(s_modbusMasterEvt,
                     EV_MASTER_READY | EV_MASTER_FRAME_RECEIVED | EV_MASTER_EXECUTE |
                     EV_MASTER_FRAME_SENT | EV_MASTER_ERROR_PROCESS,
                     RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER,
                     &recvedEvent);
#else

    recvedEvent = osEventFlagsWait(xMasterOsEvent,
                                   EV_MASTER_READY | EV_MASTER_FRAME_RECEIVED | EV_MASTER_EXECUTE |
                                   EV_MASTER_FRAME_SENT | EV_MASTER_ERROR_PROCESS | EV_MASTER_EXIT,
                                   osFlagsWaitAny, osWaitForever);
#endif
    /* the enum type couldn't convert to int type */
    switch (recvedEvent)
    {
        case EV_MASTER_READY:
            *eEvent = EV_MASTER_READY;
            break;
        case EV_MASTER_FRAME_RECEIVED:
            *eEvent = EV_MASTER_FRAME_RECEIVED;
            break;
        case EV_MASTER_EXECUTE:
            *eEvent = EV_MASTER_EXECUTE;
            break;
        case EV_MASTER_FRAME_SENT:
            *eEvent = EV_MASTER_FRAME_SENT;
            break;
        case EV_MASTER_ERROR_PROCESS:
            *eEvent = EV_MASTER_ERROR_PROCESS;
            break;
        case EV_MASTER_EXIT:
            *eEvent = EV_MASTER_EXIT;
            break;
    }
    //NOTE: to fixed slave send serial data during idle.
    if(vMBMasterRunResIdle() == TRUE)
    {
        if(recvedEvent & (EV_MASTER_FRAME_RECEIVED | EV_MASTER_EXECUTE | EV_MASTER_ERROR_PROCESS))
        {
            warn("ignore event %x during master IDLE", recvedEvent);
            *eEvent = 0;
        }else if(recvedEvent == EV_MASTER_EXIT)
        {
            bMasterExit = TRUE;
        }
    }else{
        bMasterExit = FALSE;
    }
    return TRUE;
}
/**
 * This function is initialize the OS resource for modbus master.
 * Note:The resource is define by OS.If you not use OS this function can be empty.
 *
 */
void vMBMasterOsResInit(void)
{
    xMasterRunRes = osSemaphoreNew(1, 1, NULL);
    assert_param(xMasterRunRes != NULL);
}

/**
 * This function is take Mobus Master running resource.
 * Note:The resource is define by Operating System.If you not use OS this function can be just return TRUE.
 *
 * @param lTimeOut the waiting time.
 *
 * @return resource taked result
 */
BOOL xMBMasterRunResTake(LONG lTimeOut)
{
    /*If waiting time is -1 .It will wait forever */
    //return rt_sem_take(&xMasterRunRes, lTimeOut) ? FALSE : TRUE ;
    osStatus_t sta = osSemaphoreAcquire(xMasterRunRes, (uint32_t)lTimeOut);
    assert_param(sta == osOK);
    return TRUE;
}

/**
 * This function is release Mobus Master running resource.
 * Note:The resource is define by Operating System.If you not use OS this function can be empty.
 *
 */
void vMBMasterRunResRelease(void)
{
    /* release resource */
    //rt_sem_release(&xMasterRunRes);
    uint32_t cnt = osSemaphoreGetCount(xMasterRunRes);
    osStatus_t sta = osSemaphoreRelease(xMasterRunRes);
    assert_param(sta == osOK);
}

BOOL vMBMasterRunResIdle(void)
{
    return osSemaphoreGetCount(xMasterRunRes) ? TRUE : FALSE;
}

/**
 * This is modbus master respond timeout error process callback function.
 * @note There functions will block modbus master poll while execute OS waiting.
 * So,for real-time of system.Do not execute too much waiting process.
 *
 * @param ucDestAddress destination salve address
 * @param pucPDUData PDU buffer data
 * @param ucPDULength PDU buffer length
 *
 */
void vMBMasterErrorCBRespondTimeout(UCHAR ucDestAddress, const UCHAR *pucPDUData,
                                    USHORT ucPDULength)
{
    /**
     * @note This code is use OS's event mechanism for modbus master protocol stack.
     * If you don't use OS, you can change it.
     */
    //rt_event_send(&xMasterOsEvent, EV_MASTER_ERROR_RESPOND_TIMEOUT);
    uint32_t ret = osEventFlagsSet(xMasterOsEvent, EV_MASTER_ERROR_RESPOND_TIMEOUT);
    ///assert_param(ret == EV_MASTER_ERROR_RESPOND_TIMEOUT);

    /* You can add your code under here. */
    // TODO:
}

/**
 * This is modbus master receive data error process callback function.
 * @note There functions will block modbus master poll while execute OS waiting.
 * So,for real-time of system.Do not execute too much waiting process.
 *
 * @param ucDestAddress destination salve address
 * @param pucPDUData PDU buffer data
 * @param ucPDULength PDU buffer length
 *
 */
void vMBMasterErrorCBReceiveData(UCHAR ucDestAddress, const UCHAR *pucPDUData,
                                 USHORT ucPDULength)
{
    /**
     * @note This code is use OS's event mechanism for modbus master protocol stack.
     * If you don't use OS, you can change it.
     */
    //rt_event_send(&xMasterOsEvent, EV_MASTER_ERROR_RECEIVE_DATA);
    uint32_t ret = osEventFlagsSet(xMasterOsEvent, EV_MASTER_ERROR_RECEIVE_DATA);
    //assert_param(ret == EV_MASTER_ERROR_RECEIVE_DATA);

    /* You can add your code under here. */

}

/**
 * This is modbus master execute function error process callback function.
 * @note There functions will block modbus master poll while execute OS waiting.
 * So,for real-time of system.Do not execute too much waiting process.
 *
 * @param ucDestAddress destination salve address
 * @param pucPDUData PDU buffer data
 * @param ucPDULength PDU buffer length
 *
 */
void vMBMasterErrorCBExecuteFunction(UCHAR ucDestAddress, const UCHAR *pucPDUData,
                                     USHORT ucPDULength)
{
    /**
     * @note This code is use OS's event mechanism for modbus master protocol stack.
     * If you don't use OS, you can change it.
     */
    //rt_event_send(&xMasterOsEvent, EV_MASTER_ERROR_EXECUTE_FUNCTION);
    uint32_t ret = osEventFlagsSet(xMasterOsEvent, EV_MASTER_ERROR_EXECUTE_FUNCTION);
    //assert_param(ret == EV_MASTER_ERROR_EXECUTE_FUNCTION);

    /* You can add your code under here. */

}

/**
 * This is modbus master request process success callback function.
 * @note There functions will block modbus master poll while execute OS waiting.
 * So,for real-time of system.Do not execute too much waiting process.
 *
 */
void vMBMasterCBRequestScuuess(void)
{
    /**
     * @note This code is use OS's event mechanism for modbus master protocol stack.
     * If you don't use OS, you can change it.
     */
    //rt_event_send(&xMasterOsEvent, EV_MASTER_PROCESS_SUCESS);
    uint32_t ret = osEventFlagsSet(xMasterOsEvent, EV_MASTER_PROCESS_SUCESS);
    ///assert_param(ret == EV_MASTER_PROCESS_SUCESS);

    /* You can add your code under here. */

}

/**
 * This function is wait for modbus master request finish and return result.
 * Waiting result include request process success, request respond timeout,
 * receive data error and execute function error.You can use the above callback function.
 * @note If you are use OS, you can use OS's event mechanism. Otherwise you have to run
 * much user custom delay for waiting.
 *
 * @return request error code
 */
eMBMasterReqErrCode eMBMasterWaitRequestFinish(void)
{
    eMBMasterReqErrCode    eErrStatus = MB_MRE_NO_ERR;
    uint32_t recvedEvent;
    /* waiting for OS event */
#if 0
    rt_event_recv(&xMasterOsEvent,
                  EV_MASTER_PROCESS_SUCESS | EV_MASTER_ERROR_RESPOND_TIMEOUT
                  | EV_MASTER_ERROR_RECEIVE_DATA
                  | EV_MASTER_ERROR_EXECUTE_FUNCTION,
                  RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER,
                  &recvedEvent);
#else
    recvedEvent = osEventFlagsWait(xMasterOsEvent,
                                   EV_MASTER_PROCESS_SUCESS | EV_MASTER_ERROR_RESPOND_TIMEOUT
                                   | EV_MASTER_ERROR_RECEIVE_DATA
                                   | EV_MASTER_ERROR_EXECUTE_FUNCTION,
                                   osFlagsWaitAny, osWaitForever);
#endif
    switch (recvedEvent)
    {
        case EV_MASTER_PROCESS_SUCESS:
            break;
        case EV_MASTER_ERROR_RESPOND_TIMEOUT:
        {
            eErrStatus = MB_MRE_TIMEDOUT;
            break;
        }
        case EV_MASTER_ERROR_RECEIVE_DATA:
        {
            eErrStatus = MB_MRE_REV_DATA;
            break;
        }
        case EV_MASTER_ERROR_EXECUTE_FUNCTION:
        {
            eErrStatus = MB_MRE_EXE_FUN;
            break;
        }
    }
    return eErrStatus;
}

void vMBMasterInit(void)
{
    bMasterExit = FALSE;
}

void vMBMasterSendExit(void)
{
    if(vMBMasterRunResIdle() == TRUE)
    {
        uint32_t ret = osEventFlagsSet(xMasterOsEvent, EV_MASTER_EXIT);
        dbg("send master exit cmd");
    }else
    {
        err("master busy");
    }
}

BOOL bMBMasterIfExit(void)
{
    if(vMBMasterRunResIdle() == TRUE)
    {
        //already get exit event
        return bMasterExit;
    }
    return FALSE;
}

#endif
