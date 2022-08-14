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

static uint8_t *s_pucCustomResBuf = NULL;
static uint16_t *s_pusCustomResBufLen = NULL;
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
eMBMasterReqErrCode eMBMasterReqCustom(UCHAR ucSndAddr, UCHAR *pucData, USHORT usDataLen, UCHAR *pucResBuf, USHORT *pusResLen, LONG lTimeOut)
{
    UCHAR                 *ucMBFrame;
    eMBMasterReqErrCode    eErrStatus = MB_MRE_NO_ERR;
    USHORT  slen = (usDataLen > MB_PDU_CUSTOMSIZE_MAX) ? MB_PDU_CUSTOMSIZE_MAX : usDataLen;
    assert_param(slen != 0);
    //FIXME:lTimeOut for wait or for response?
    if (ucSndAddr > MB_MASTER_TOTAL_SLAVE_NUM) eErrStatus = MB_MRE_ILL_ARG;
    else if (xMBMasterRunResTake(lTimeOut) == FALSE) eErrStatus = MB_MRE_MASTER_BUSY;
    else
    {
        //save the cmd point for response
        s_pucCustomResBuf = pucResBuf;
        s_pusCustomResBufLen = pusResLen;
    
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
    /* If this request is broadcast, and it's read mode. This request don't need execute. */
    if (xMBMasterRequestIsBroadcast())
    {
        eStatus = MB_EX_NONE;
    }else if (*usLen >= MB_PDU_SIZE_MIN + MB_PDU_FUNC_CUSTOM_SIZE_MIN)
    {
        vMBMasterGetPDUSndBuf(&ucMBFrame);
        //copy data to user
        if(s_pucCustomResBuf != NULL && s_pusCustomResBufLen != NULL)
        {
            memcpy(s_pucCustomResBuf, ucMBFrame + MB_PDU_REQ_DATA_OFF, *usLen - MB_PDU_SIZE_MIN);
            *s_pusCustomResBufLen = *usLen - MB_PDU_SIZE_MIN;
        }
    }else
    {
        /* Can't be a valid request because the length is incorrect. */
        eStatus = MB_EX_ILLEGAL_DATA_VALUE;
    }
    return eStatus;
}

/*
error code for all command request status
*/
typedef enum{
    REQ_ERR_NOERR,
    REQ_ERR_ILL_ID,
    REQ_ERR_ILL_ARG,
    REQ_ERR_RECVDATA,
    REQ_ERR_TIMEOUT,
    REQ_ERR_BUSY,
    REQ_ERR_EXEC,
    REQ_ERR_PARA,
    REQ_ERR_OTHER = 0x88
} eReqErrCode;

eReqErrCode ExecuteCommand(uint8_t *pucCmdData, uint16_t usCmdDataLen, uint8_t *pucResBuf, uint16_t *pusResLen)
{
    #if DEBUG
    uint32_t tks = osKernelGetTickCount();
    #endif
    eReqErrCode sta = REQ_ERR_NOERR;
    if(pucCmdData == NULL || !usCmdDataLen)
    {
        return REQ_ERR_PARA;
    }

    if(eMBMasterIsEstablished() != TRUE)
    {
        return REQ_ERR_BUSY;
    }
    eMBMasterReqErrCode reqsta = eMBMasterReqCustom(SECU_MCU_ADDR, pucCmdData, usCmdDataLen, pucResBuf, pusResLen, 200);
    if(reqsta==MB_MRE_NO_ERR)
    {
        #if DEBUG
            dbg("ExecCMD Success.\n");
            if(pucResBuf && pusResLen)
            {
                hexdump(pucResBuf, *pusResLen);
            }
        #endif
    }else if(reqsta == MB_MRE_MASTER_BUSY)
    {
        err("error:MB_MRE_MASTER_BUSY\n");
        sta = REQ_ERR_BUSY;
    }else
    {
        err("error: ReqCustom %x\n",reqsta);
        sta = (eReqErrCode)reqsta;
    }
    #if DEBUG
    dbg("Total Take %dms\n",osKernelGetTickCount() - tks);
    #endif
    return sta;
}

#endif
