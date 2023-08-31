/********************************************************************************

 **********************Copyright (C), 2022, KingShiDun Inc.**********************

 ********************************************************************************
 * @file       : mbuserconfig.h
 * @brief      : serial.c header file
 * @author     : Jay
 * @version    : 1.0
 * @date       : 2022-07-29
 * 
 * 
 * @note History:
 * @note       : Jay 2022-07-29
 * @note       : 
 *   Modification: Created file

********************************************************************************/

#ifndef _MBUSERCONFIG_H
#define _MBUSERCONFIG_H


#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

#define MB_MASTER_ENABLED               1

#define MB_SLAVE_ENABLED                1

#if MB_MASTER_ENABLED == 1 && MB_SLAVE_ENABLED == 1
    #define MB_MASTER_SLAVE_AIO                1
    #define DEBUG_MODBUS
#else
    #define MB_MASTER_SLAVE_AIO                0
#endif
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */


#endif /* _MBUSERCONFIG_H */

