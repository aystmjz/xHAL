/**
 ******************************************************************************
 * @file    xhal_common.c
 * @author  aystmjz
 * @brief   公共模块源文件，实现版本信息获取及错误码转字符串功能
 * @version 2.3.0
 * @date    2026-08-23
 ******************************************************************************
 * Copyright (c) 2026 aystmjz. All rights reserved.
 * SPDX-License-Identifier: MIT
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "xhal_common.h"

/* Exported variables --------------------------------------------------------*/
/**
 * @brief  xHAL 启动 Logo 图案
 */
const char xhal_logo[] = "   _  __ __  _____    __ \r\n"
                         "  | |/ // / / /   |  / / \r\n"
                         "  |   // /_/ / /| | / /  \r\n"
                         " /   |/ __  / ___ |/ /___\r\n"
                         "/_/|_/_/ /_/_/  |_/_____/\r\n";

/* Exported functions --------------------------------------------------------*/
/**
 * @brief  获取 xHAL 版本号(十六进制)
 * @retval 版本号，形如 0x020300
 */
uint32_t xhal_version(void)
{
    return XHAL_VERSION_HEX;
}

/**
 * @brief  获取 xHAL 版本号字符串
 * @retval 版本号字符串，如 "2.3.0"
 */
const char *xhal_version_str(void)
{
    return XHAL_VERSION_STR;
}

/**
 * @brief  错误码转换为字符串描述
 * @param  err: 错误码
 * @retval 错误码对应的字符串，未知错误码返回 "Unknown error"
 */
const char *xhal_err_to_str(xhal_err_t err)
{
    switch (err)
    {
#define ERR(code, value, str) \
    case code:                \
        return str " (" #value ")";
        XHAL_ERR_LIST
#undef ERR
    default:
        return "Unknown error";
    }
}

/* ---------------------------------------------------------------------------*/