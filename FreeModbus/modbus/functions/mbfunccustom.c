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
#include "mbframe.h"
#include "mbproto.h"
#include "mbconfig.h"

#if MB_FUNC_CUSTOM_ENABLED > 0
extern int eExecuteCustomFunc(uint8_t ucFunctionID, uint16_t *pusDatalen, uint8_t *pucDataBuf);
#define MIN_CUSTOM_DATALEN          3
/**
 * @fn          eMBException eMBFuncCustom(UCHAR *pucFrame, USHORT *usLen)
 * @brief       excute custom function
 *
 * @param[in]   UCHAR *pucFrame
                TLV format:  TAG(1) + Len(2) + Val(0~....)
 * @param[in]   USHORT *usLen
 * @return      eMBException
 */
eMBException eMBFuncCustom(UCHAR *pucFrame, USHORT *usLen)
{
    eMBException sta;
    uint8_t funcid;
    uint16_t *pusdlen;
    uint8_t *pucdata;

    // data length error
    if (*usLen < MIN_CUSTOM_DATALEN)
    {
        return MB_EX_ILLEGAL_FUNCTION;
    }

    //TLV command format
    funcid = pucFrame[MB_PDU_DATA_OFF];
    pusdlen = (uint16_t *)(pucFrame + MB_PDU_DATA_OFF + 1); //MSB
    pucdata = pucFrame + MB_PDU_DATA_OFF + MIN_CUSTOM_DATALEN;
    printf(">>T=%x L=%d\n", funcid, *pusdlen);

    sta = (eMBException)eExecuteCustomFunc(funcid, pusdlen, pucdata);
    if (sta)
        printf("error: sta=%d\n", sta);
    //TLV response
    *usLen = MB_PDU_DATA_OFF + MIN_CUSTOM_DATALEN + *pusdlen;
    //printf("water:%d %p\n", uxTaskGetStackHighWaterMark(NULL), pucFrame);
    printf("<<T=%x L=%d\n", funcid, *usLen);
    return sta;
}

#endif
