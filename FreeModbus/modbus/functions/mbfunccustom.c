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
eMBException
eMBFuncCustom(UCHAR *pucFrame, USHORT *usLen)
{
    UCHAR buf[32];
    short len = 3;
    buf[0] = '9';
    buf[1] = '7';
    buf[2] = '0';

    hexdump(pucFrame, *usLen);
    memcpy(pucFrame + MB_PDU_DATA_OFF, buf, len);

    *usLen = (USHORT)(MB_PDU_DATA_OFF + len);

    printf("water:%d %p\n", uxTaskGetStackHighWaterMark(NULL), pucFrame);
    return MB_EX_NONE;
}

#endif
