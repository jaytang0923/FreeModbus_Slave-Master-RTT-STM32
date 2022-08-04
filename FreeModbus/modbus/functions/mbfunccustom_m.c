/********************************************************************************

 **********************Copyright (C), 2022, KingShiDun Inc.**********************

 ********************************************************************************
 * @file       : mbfunccustom.c
 * @brief      : .C file function description
 * @author     : Jay
 * @version    : 1.0
 * @date       : 2022-07-30
 *
 *
 * @note History:
 * @note       : Jay 2022-07-30
 * @note       :
 *   Modification: Created file

********************************************************************************/
/* ----------------------- System includes ----------------------------------*/
#include "stdlib.h"
#include "string.h"

/* ----------------------- Platform includes --------------------------------*/
#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mb_m.h"
#include "mbframe.h"
#include "mbproto.h"
#include "mbconfig.h"

#if MB_FUNC_CUSTOM_ENABLED > 0

#ifdef USE_FULL_ASSERT
    #define DEBUG           1
    #define dbg(fmt, args...)                                             \
            printf("[%d]DBG: " fmt, osKernelGetTickCount(), ##args)

    #define err(x...)   do{}while(0)
#else
    #define dbg(x...)   do{}while(0)
    #define err(x...)   do{}while(0)
#endif

extern int eExecuteCustomFunc(uint8_t ucFunctionID, uint16_t *pusDatalen, uint8_t *pucDataBuf);

#define MB_PDU_FUNC_CUSTOM_SIZE_MIN          (1)
#define MB_PDU_CUSTOMSIZE_MAX       (MB_PDU_SIZE_MAX - MB_PDU_FUNC_CUSTOM_SIZE_MIN)

#define MB_PDU_REQ_DATA_OFF         (MB_PDU_DATA_OFF)


#define SECU_MCU_ADDR       2

static uint8_t *s_pucCustomData = NULL;
static uint16_t *s_pucCustomDataLen = NULL;
/**
 * This function will request read input register.
 *
 * @param ucSndAddr salve address
 * @param usRegAddr register start address
 * @param usNRegs register total number
 * @param lTimeOut timeout (-1 will waiting forever)
 *
 * @return error code
 */
eMBMasterReqErrCode eMBMasterReqCustom(UCHAR ucSndAddr, UCHAR *pucData, USHORT *pusDataLen, LONG lTimeOut)
{
    UCHAR                 *ucMBFrame;
    eMBMasterReqErrCode    eErrStatus = MB_MRE_NO_ERR;
    USHORT  slen = (*pusDataLen > MB_PDU_CUSTOMSIZE_MAX) ? MB_PDU_CUSTOMSIZE_MAX : *pusDataLen;
    assert_param(slen != 0);
    //FIXME:lTimeOut for wait or for response?
    if (ucSndAddr > MB_MASTER_TOTAL_SLAVE_NUM) eErrStatus = MB_MRE_ILL_ARG;
    else if (xMBMasterRunResTake(lTimeOut) == FALSE) eErrStatus = MB_MRE_MASTER_BUSY;
    else
    {
        //save the cmd point for response
        s_pucCustomData = pucData;
        s_pucCustomDataLen = pusDataLen;
    
        vMBMasterGetPDUSndBuf(&ucMBFrame);
        vMBMasterSetDestAddress(ucSndAddr);
        ucMBFrame[MB_PDU_FUNC_OFF]                = MB_FUNC_CUSTOM;
        
        //data
        memcpy(ucMBFrame + MB_PDU_REQ_DATA_OFF, pucData, slen);
        
        vMBMasterSetPDUSndLength(MB_PDU_SIZE_MIN + slen);
        (void) xMBMasterPortEventPost(EV_MASTER_FRAME_SENT);
        eErrStatus = eMBMasterWaitRequestFinish();
    }
    return eErrStatus;
}


/**
 * @fn          eMBException eMBFuncCustom(UCHAR *pucFrame, USHORT *usLen)
 * @brief       excute custom function
 *
 * @param[in]   UCHAR *pucFrame
                TLV format:  TAG(1) + Len(2) + Val(0~....)
 * @param[in]   USHORT *usLen
 * @return      eMBException
 */
eMBException eMBMasterFuncCustom(UCHAR *pucFrame, USHORT *usLen)
{
    UCHAR          *ucMBFrame;
    eMBException    eStatus = MB_EX_NONE;
    dbg("CB: Len=%d, data:\t",*usLen - MB_PDU_SIZE_MIN);
    hexdump(pucFrame + MB_PDU_REQ_DATA_OFF, *usLen - MB_PDU_SIZE_MIN);
    /* If this request is broadcast, and it's read mode. This request don't need execute. */
    if (xMBMasterRequestIsBroadcast())
    {
        eStatus = MB_EX_NONE;
    }else if (*usLen >= MB_PDU_SIZE_MIN + MB_PDU_FUNC_CUSTOM_SIZE_MIN)
    {
        vMBMasterGetPDUSndBuf(&ucMBFrame);
        dbg("%p %p %p %p\n",s_pucCustomData, s_pucCustomDataLen, ucMBFrame, pucFrame);
        //copy data to user
        memcpy(s_pucCustomData, ucMBFrame + MB_PDU_REQ_DATA_OFF, *usLen - MB_PDU_SIZE_MIN);
        *s_pucCustomDataLen = *usLen - MB_PDU_SIZE_MIN;dbg("4");
    }else
    {
        /* Can't be a valid request because the length is incorrect. */
        eStatus = MB_EX_ILLEGAL_DATA_VALUE;
    }
    dbg("done");
    return eStatus;
}

/*
error code for all command request status
*/
typedef enum{
    REQ_ERR_NOERR,
    REQ_ERR_TIMEOUT  = 4,
    REQ_ERR_BUSY,
    REQ_ERR_EXEC,
    REQ_ERR_PARA,
} eReqErrCode;

eReqErrCode ExecuteCommand(uint8_t *pucData, uint16_t *pusDataLen)
{
    #if DEBUG
    uint32_t tks = osKernelGetTickCount();
    #endif
    eReqErrCode sta = REQ_ERR_NOERR;
    if(pucData == NULL || !*pusDataLen)
    {
        return REQ_ERR_PARA;
    }
    dbg("%p %p\n", pucData, pusDataLen);
    eMBMasterReqErrCode reqsta = eMBMasterReqCustom(SECU_MCU_ADDR, pucData, pusDataLen, 200);
    if(reqsta==MB_MRE_NO_ERR)
    {
        dbg("ExecCMD Success,resLen=%d:\t", *pusDataLen);
        hexdump(pucData, *pusDataLen);
    }else if(reqsta == MB_MRE_MASTER_BUSY)
    {
        err("error:MB_MRE_MASTER_BUSY\n");
        sta = REQ_ERR_BUSY;
    }else
    {
        err("error: ReqCustom %x\n",reqsta);
        sta = 0xff;
    }
    #if DEBUG
    dbg("Total Take %dms\n",osKernelGetTickCount() - tks);
    #endif
    return sta;
}

#endif
