/**
 ******************************************************************************
 * @file    xhal_export.h
 * @author  aystmjz
 * @brief   导出模块头文件，提供初始化/退出/单元测试函数的自动注册与执行接口
 * @version 2.3.0
 * @date    2026-08-23
 ******************************************************************************
 * Copyright (c) 2026 aystmjz. All rights reserved.
 * SPDX-License-Identifier: MIT
 ******************************************************************************
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __XHAL_EXPORT_H
#define __XHAL_EXPORT_H

/* Includes ------------------------------------------------------------------*/
#include "xhal_coro.h"
#include "xhal_def.h"

/** @addtogroup XHAL
 * @{
 */

/** @defgroup XCORE
 * @{
 */

/** @defgroup XHAL_EXPORT
 * @brief  导出模块，提供初始化/退出/单元测试函数的自动注册与执行接口
 * @{
 */

/* Exported constants --------------------------------------------------------*/
#define EXPORT_ID_INIT (0xabababab) /*!< 初始化导出项魔数 */
#define EXPORT_ID_EXIT (0xcdcdcdcd) /*!< 退出导出项魔数 */

/* Exported types ------------------------------------------------------------*/
/**
 * @brief  导出级别枚举，级别越高执行越晚
 */
typedef enum export_level
{
    EXPORT_LEVEL_NULL = -2, /*!< 空级别 */
    EXPORT_LEVEL_TEST = -1, /*!< 单元测试级别 */

    EXPORT_LEVEL_DEBUG   = 0, /*!< 调试级别 */
    EXPORT_LEVEL_CORE    = 1, /*!< 核心模块级别 */
    EXPORT_LEVEL_PERIPH  = 2, /*!< 外设级别 */
    EXPORT_LEVEL_DRIVER  = 3, /*!< 驱动级别 */
    EXPORT_LEVEL_MIDWARE = 4, /*!< 中间件级别 */
    EXPORT_LEVEL_APP     = 5, /*!< 应用级别 */
    EXPORT_LEVEL_USER    = 6, /*!< 用户级别 */

    EXPORT_LEVEL_MAX /*!< 级别总数 */
} export_level_t;

/**
 * @brief  导出函数类型
 * @retval 错误码
 */
typedef xhal_err_t (*export_func_t)(void);

/**
 * @brief  导出项结构体
 */
typedef struct xhal_export
{
    uint32_t magic_head; /*!< 头部魔数 */
    const char *name;    /*!< 导出函数名称 */
    export_func_t func;  /*!< 导出函数指针 */
    int32_t level;       /*!< 导出级别 */
    uint32_t magic_tail; /*!< 尾部魔数 */
} xhal_export_t;

/* Exported variables --------------------------------------------------------*/
#if (XHAL_OS_SUPPORTING == 0)
extern xcoro_manager_t g_coro_manager; /*!< 全局协程管理器(非 OS 模式) */
#endif

/* Exported functions --------------------------------------------------------*/
#if (XHAL_UNIT_TEST == 1)
void xhal_unit_test(void);
#endif
void xhal_run(void);
void xhal_exit(void);

/* Exported macros -----------------------------------------------------------*/
/**
 * @brief  初始化函数导出宏
 * @param  _func:  初始化函数
 * @param  _level: 导出级别，范围[0, 127]
 * @note   将函数注册到 .xhal_init_export 段，由 xhal_run() 按级别调用
 */
#define INIT_EXPORT(_func, _level)                           \
    XHAL_USED const xhal_export_t init_##_func XHAL_SECTION( \
        ".xhal_init_export") = {                             \
        .name       = #_func,                                \
        .func       = (export_func_t)(_func),                \
        .level      = (int32_t)(_level),                     \
        .magic_head = EXPORT_ID_INIT,                        \
        .magic_tail = EXPORT_ID_INIT,                        \
    }

/**
 * @brief  退出函数导出宏
 * @param  _func:  退出函数
 * @param  _level: 导出级别，范围[0, 127]
 * @note   将函数注册到 .xhal_exit_export 段，由 xhal_exit() 按级别逆序调用
 */
#define EXIT_EXPORT(_func, _level)                           \
    XHAL_USED const xhal_export_t exit_##_func XHAL_SECTION( \
        ".xhal_exit_export") = {                             \
        .name       = #_func,                                \
        .func       = (export_func_t)(_func),                \
        .level      = (int32_t)(_level),                     \
        .magic_head = EXPORT_ID_EXIT,                        \
        .magic_tail = EXPORT_ID_EXIT,                        \
    }

/**
 * @brief  单元测试函数导出宏
 * @param  _func: 单元测试函数
 * @note   仅在 XHAL_UNIT_TEST 使能时有效
 */
#if (XHAL_UNIT_TEST == 1)
    #define UNIT_TEST_EXPORT(_func) INIT_EXPORT(_func, EXPORT_LEVEL_TEST)
#else
    #define UNIT_TEST_EXPORT(_func)
#endif

/** @} */

/** @} */

/** @} */

#endif /* __XHAL_EXPORT_H */
