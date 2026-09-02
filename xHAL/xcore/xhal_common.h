/**
 ******************************************************************************
 * @file    xhal_common.h
 * @author  aystmjz
 * @brief   公共模块头文件，提供版本信息、地址合法性判断及错误码转字符串接口
 * @version 2.3.0
 * @date    2026-08-23
 ******************************************************************************
 * Copyright (c) 2026 aystmjz. All rights reserved.
 * SPDX-License-Identifier: MIT
 ******************************************************************************
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __XHAL_COMMON_H
#define __XHAL_COMMON_H

/* Includes ------------------------------------------------------------------*/
#include "xhal_config.h"
#include "xhal_def.h"

/** @addtogroup XHAL
 * @{
 */

/** @defgroup XCORE
 * @{
 */

/** @defgroup XHAL_COMMON
 * @brief  公共模块，提供版本信息、地址合法性判断及错误码转字符串接口
 * @{
 */

/* Exported macros -----------------------------------------------------------*/
/**
 * @brief  判断地址是否为合法的 RAM 地址
 * @param  addr: 待判断的地址
 * @retval 合法返回真，否则返回假
 */
#define XHAL_IS_VALID_RAM_ADDRESS(addr)                  \
    (((xhal_pointer_t)(addr) >= XHAL_VALID_RAM_START) && \
     ((xhal_pointer_t)(addr) <= XHAL_VALID_RAM_END))

/**
 * @brief  判断地址是否为合法的 Flash 地址
 * @param  addr: 待判断的地址
 * @retval 合法返回真，否则返回假
 */
#define XHAL_IS_VALID_FLASH_ADDRESS(addr)                  \
    (((xhal_pointer_t)(addr) >= XHAL_VALID_FLASH_START) && \
     ((xhal_pointer_t)(addr) <= XHAL_VALID_FLASH_END))

/* Exported constants --------------------------------------------------------*/
#define XHAL_VERSION_MAJOR 2 /*!< 主版本号 */
#define XHAL_VERSION_MINOR 3 /*!< 次版本号 */
#define XHAL_VERSION_PATCH 0 /*!< 修订版本号 */
#define XHAL_VERSION_HEX                                      \
    ((XHAL_VERSION_MAJOR << 16) | (XHAL_VERSION_MINOR << 8) | \
     (XHAL_VERSION_PATCH)) /*!< 十六进制版本号 */

#define XHAL_VERSION_STR "2.3.0" /*!< 版本号字符串 */

#define XHAL_BUILD_DATE  __DATE__ /*!< 编译日期 */
#define XHAL_BUILD_TIME  __TIME__ /*!< 编译时间 */

/* Exported variables --------------------------------------------------------*/
extern const char xhal_logo[]; /*!< xHAL 启动 Logo 图案 */

/* Exported functions --------------------------------------------------------*/
uint32_t xhal_version(void);
const char *xhal_version_str(void);
const char *xhal_err_to_str(xhal_err_t err);

/** @} */

/** @} */

/** @} */

#endif /* __XHAL_COMMON_H */
