/**
 ******************************************************************************
 * @file    xhal_malloc.h
 * @author  aystmjz
 * @brief   内存管理模块头文件，提供内存池分配/释放及内存操作接口
 * @version 2.3.0
 * @date    2026-08-23
 ******************************************************************************
 * Copyright (c) 2026 aystmjz. All rights reserved.
 * SPDX-License-Identifier: MIT
 ******************************************************************************
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __XHAL_MALLOC_H
#define __XHAL_MALLOC_H

/* Includes ------------------------------------------------------------------*/
#include "xhal_config.h"
#include "xhal_std.h"

/** @addtogroup XHAL
 * @{
 */

/** @defgroup XCORE
 * @{
 */

/** @defgroup XHAL_MALLOC
 * @brief  内存管理模块，提供内存池分配/释放及内存操作接口
 * @{
 */

/* Exported types ------------------------------------------------------------*/
/**
 * @brief  内存池结构体
 */
typedef struct xmem_pool
{
    uint8_t *const membase; /*!< 内存池基地址 */
    uint16_t *const memmap; /*!< 内存块状态表 */
    uint8_t memrdy;         /*!< 内存池就绪标志 */
} xmem_pool_t;

/* Exported functions --------------------------------------------------------*/
uint32_t xmem_free_size(void);
uint16_t xmem_perused(void);
void xmemset(void *s, uint8_t c, uint32_t count);
void xmemcpy(void *des, const void *src, uint32_t n);
void xfree(void *ptr);
void *xmalloc(uint32_t size);
void *xcalloc(uint32_t n, uint32_t size);
void *xrealloc(void *ptr, uint32_t size);

/** @} */

/** @} */

/** @} */

#endif /* __XHAL_MALLOC_H */