/*
 * FreeModbus Libary: BARE Port
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
 * File: $Id: port.h ,v 1.60 2013/08/13 15:07:05 Armink add Master Functions $
 */

#ifndef _PORT_H
#define _PORT_H

#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "mbconfig.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"
#include <task.h>
#include <event_groups.h>

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

#define DEBUG 1
#if DEBUG == 1
#define MODBUS_DEBUG(fmt, args...)                                             \
  printf("  MODBUS_DEBUG(%s:%d):  \t" fmt, __func__, __LINE__, ##args)
#elif DEBUG == 0
#define MODBUS_DEBUG(fmt, args...)                                             \
  do {                                                                         \
  } while (0)
#endif

//#define IS_IRQ_MODE()             (__get_IPSR() != 0U)
#define assert_param(expr) ((expr) ? (int)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
//void assert_failed(uint8_t* file, uint32_t line);

typedef struct _serial_fifo {
  /* software fifo */
  volatile uint8_t *buffer;
  volatile uint16_t put_index, get_index;
} Serial_fifo;
#define FIFO_SIZE_MAX 512

///////////////////////////////////////////////////////////////////////////////////////////////
#define INLINE
#define PR_BEGIN_EXTERN_C           extern "C" {
#define PR_END_EXTERN_C             }

#define ENTER_CRITICAL_SECTION()    EnterCriticalSection()
#define EXIT_CRITICAL_SECTION()    ExitCriticalSection()

typedef uint8_t BOOL;

typedef unsigned char UCHAR;
typedef char    CHAR;

typedef uint16_t USHORT;
typedef int16_t SHORT;

typedef uint32_t ULONG;
typedef int32_t LONG;

#ifndef TRUE
#define TRUE            1
#endif

#ifndef FALSE
#define FALSE           0
#endif

void EnterCriticalSection(void);
void ExitCriticalSection(void);

extern __inline BOOL IS_IRQ(void);
void Put_in_fifo(Serial_fifo *buff, uint8_t *putdata, int length);
int Get_from_fifo(Serial_fifo *buff, uint8_t *getdata, int length);

#endif
