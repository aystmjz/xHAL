/**
 ******************************************************************************
 * @file    xhal_log.h
 * @author  aystmjz
 * @brief   日志模块头文件，提供分级日志输出、时间模式及日志宏接口
 * @version 2.3.0
 * @date    2026-08-23
 ******************************************************************************
 * Copyright (c) 2026 aystmjz. All rights reserved.
 * SPDX-License-Identifier: MIT
 ******************************************************************************
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __XLOG_H
#define __XLOG_H

/* Includes ------------------------------------------------------------------*/
#include "../xlib/xhal_bit.h"
#include "xhal_common.h"

/* Private define ------------------------------------------------------------*/
#ifndef XLOG_DEFAULT_LEVEL
    /**
     * @brief   日志默认输出级别
     * @note  可选值: XLOG_LEVEL_NULL/ERROR/WARNING/INFO/DEBUG
     */
    #define XLOG_DEFAULT_LEVEL XLOG_LEVEL_DEBUG
#endif /* XLOG_DEFAULT_LEVEL */

#ifndef XLOG_DEFAULT_TIME_MODE
    /**
     * @brief   日志默认时间模式
     * @note  可选值: XLOG_TIME_MOD_NONE/MILLIS/RELATIVE/ABSOLUTE
     */
    #define XLOG_DEFAULT_TIME_MODE XLOG_TIME_MOD_RELATIVE
#endif /* XLOG_DEFAULT_TIME_MODE */

#ifndef XLOG_COLOR_ENABLE
    /**
     * @brief   日志颜色显示
     * @note  1 使能 ANSI 颜色输出，0 关闭
     */
    #define XLOG_COLOR_ENABLE 1
#endif /* XLOG_COLOR_ENABLE */

#ifndef XLOG_NEWLINE_ENABLE
    /**
     * @brief   日志换行
     * @note  1 在日志末尾追加换行符，0 不追加
     */
    #define XLOG_NEWLINE_ENABLE 1
#endif /* XLOG_NEWLINE_ENABLE */

#ifndef XLOG_COMPILE_LEVEL
    /**
     * @brief   日志编译级别
     * @note  低于该级别的日志宏在编译时被剔除
     */
    #define XLOG_COMPILE_LEVEL XLOG_LEVEL_DEBUG
#endif /* XLOG_COMPILE_LEVEL */

#ifndef XLOG_FILEINFO_ENABLE
    /**
     * @brief   日志打印文件信息
     * @note  1 在日志中追加文件名与行号，0 关闭
     */
    #define XLOG_FILEINFO_ENABLE 1
#endif /* XLOG_FILEINFO_ENABLE */

#ifndef XLOG_FULL_PATH_ENABLE
    /**
     * @brief   日志打印完整文件路径
     * @note  1 打印完整路径，0 仅打印文件名
     */
    #define XLOG_FULL_PATH_ENABLE 0
#endif /* XLOG_FULL_PATH_ENABLE */

#if (XLOG_FULL_PATH_ENABLE == 1)
    /**
     * @brief   完整文件路径
     */
    #define __XLOG_FILE__ XHAL_FILEPATH
#else
    /**
     * @brief   文件名(不含路径)
     */
    #define __XLOG_FILE__ XHAL_FILENAME
#endif /* (XLOG_FULL_PATH_ENABLE == 1) */

#if (XLOG_FILEINFO_ENABLE == 1)
    /**
     * @brief   文件信息参数
     * @note  供 XLOG_FILE_FORMAT 展开使用
     */
    #define XLOG_FILE_INFO   __XLOG_FILE__, XHAL_LINE
    /**
     * @brief   文件信息格式
     */
    #define XLOG_FILE_FORMAT "(%s:%d) "
#else
    #define XLOG_FILE_INFO
    #define XLOG_FILE_FORMAT
#endif /* (XLOG_FILEINFO_ENABLE == 1) */

/* Private function prototypes -----------------------------------------------*/
xhal_err_t _xlog_printf(const char *fmt, ...);
xhal_err_t _xlog_print_log(const char *name, uint8_t level, const char *fmt,
                           ...);

/** @addtogroup XHAL
 * @{
 */

/** @defgroup XCORE
 * @{
 */

/** @defgroup XHAL_LOG
 * @brief  日志模块，提供分级日志输出、时间模式及日志宏接口
 * @{
 */

/* Exported constants --------------------------------------------------------*/
#define XLOG_LEVEL_NULL        0 /*!< 空级别，不输出 */
#define XLOG_LEVEL_ERROR       1 /*!< 错误级别，只输出错误信息 */
#define XLOG_LEVEL_WARNING     2 /*!< 警告级别，输出警告信息 */
#define XLOG_LEVEL_INFO        3 /*!< 信息级别，输出一般信息 */
#define XLOG_LEVEL_DEBUG       4 /*!< 调试级别，输出最详细的调试信息 */
#define XLOG_LEVEL_MAX         5 /*!< 级别总数 */

#define XLOG_TIME_MOD_NONE     0 /*!< 不显示时间 */
#define XLOG_TIME_MOD_MILLIS   1 /*!< 毫秒时间戳 */
#define XLOG_TIME_MOD_RELATIVE 2 /*!< 相对时间 */
#define XLOG_TIME_MOD_ABSOLUTE 3 /*!< 绝对时间 */
#define XLOG_TIME_MOD_MAX      4 /*!< 时间模式总数 */

/* Exported functions --------------------------------------------------------*/
uint8_t xlog_get_level(void);
uint8_t xlog_get_time_mod(void);
xhal_err_t xlog_set_level(uint8_t level);
xhal_err_t xlog_set_time_mod(uint8_t mod);

/* Exported macros -----------------------------------------------------------*/
/**
 * @brief  无标签日志输出
 */
#define XLOG_PRINTF(fmt, ...) _xlog_printf(fmt, ##__VA_ARGS__)

/**
 * @brief  错误级别日志输出
 */
#if XLOG_COMPILE_LEVEL >= XLOG_LEVEL_ERROR
    #define XLOG_ERROR(fmt, ...)                      \
        _xlog_print_log(__xhal_tag, XLOG_LEVEL_ERROR, \
                        XLOG_FILE_FORMAT fmt XLOG_FILE_INFO, ##__VA_ARGS__)
#else
    #define XLOG_ERROR(fmt, ...)
#endif /* XLOG_COMPILE_LEVEL >= XLOG_LEVEL_ERROR */

/**
 * @brief  警告级别日志输出
 */
#if XLOG_COMPILE_LEVEL >= XLOG_LEVEL_WARNING
    #define XLOG_WARN(fmt, ...)                         \
        _xlog_print_log(__xhal_tag, XLOG_LEVEL_WARNING, \
                        XLOG_FILE_FORMAT fmt XLOG_FILE_INFO, ##__VA_ARGS__)
#else
    #define XLOG_WARN(fmt, ...)
#endif /* XLOG_COMPILE_LEVEL >= XLOG_LEVEL_WARNING */

/**
 * @brief  信息级别日志输出
 */
#if XLOG_COMPILE_LEVEL >= XLOG_LEVEL_INFO
    #define XLOG_INFO(fmt, ...) \
        _xlog_print_log(__xhal_tag, XLOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#else
    #define XLOG_INFO(fmt, ...)
#endif /* XLOG_COMPILE_LEVEL >= XLOG_LEVEL_INFO */

/**
 * @brief  调试级别日志输出
 */
#if XLOG_COMPILE_LEVEL >= XLOG_LEVEL_DEBUG
    #define XLOG_DEBUG(fmt, ...)                      \
        _xlog_print_log(__xhal_tag, XLOG_LEVEL_DEBUG, \
                        XLOG_FILE_FORMAT fmt XLOG_FILE_INFO, ##__VA_ARGS__)
    #define XDEBUG /*!< 调试模式标志 */
#else
    #define XLOG_DEBUG(fmt, ...)
#endif /* XLOG_COMPILE_LEVEL >= XLOG_LEVEL_DEBUG */

/**
 * @brief  打印错误信息与错误码字符串
 * @param  info: 错误描述
 * @param  err:  错误码
 */
#define XLOG_PRINT_ERR(info, err) \
    XLOG_ERROR("%s failed in %s: %s", info, __func__, xhal_err_to_str(err))

/** @} */

/** @} */

/** @} */

#endif /* __XLOG_H */
