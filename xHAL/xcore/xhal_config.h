/**
 ******************************************************************************
 * @file    xhal_config.h
 * @author  aystmjz
 * @brief   xHAL 全局配置头文件，集中配置各模块使能开关及运行参数
 * @version 2.3.0
 * @date    2026-08-23
 ******************************************************************************
 * Copyright (c) 2026 aystmjz. All rights reserved.
 * SPDX-License-Identifier: MIT
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "xhal_config_user.h"

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __XHAL_CONFIG_H
#define __XHAL_CONFIG_H

/** @addtogroup XHAL
 * @{
 */

/** @defgroup XCORE
 * @{
 */

/** @defgroup XHAL_CONFIG
 * @brief  xHAL 全局配置，集中配置各模块使能开关及运行参数
 * @{
 */

/* 固件与版本信息 ------------------------------------------------------------*/
#define FIRMWARE_NAME        "my_firmware_name" /*!< 固件名称 */
#define HARDWARE_VERSION     "1.0.0"            /*!< 硬件版本号 */
#define HARDWARE_VERSION_HEX ((1 << 16) | (0 << 8) | (0)) /*!< 硬件版本号(十六进制) */
#define SOFTWARE_VERSION     "1.0.0" /*!< 软件版本号 */
#define SOFTWARE_VERSION_HEX ((1 << 16) | (0 << 8) | (0)) /*!< 软件版本号(十六进制) */

/* 设备相关配置 --------------------------------------------------------------*/
#define XHAL_DEFAULT_UART  "debug_uart" /*!< 默认调试串口名称 */
#define XHAL_DEVICE_HEADER "stm32f10x.h" /*!< 设备寄存器定义头文件 */
#define XHAL_CPU_FREQ_HZ   SystemCoreClock /*!< CPU 主频(Hz) */

/* 模块使能开关 --------------------------------------------------------------*/
#define XHAL_OS_SUPPORTING (1) /*!< 操作系统支持使能 */
#define XHAL_UNIT_TEST     (1) /*!< 单元测试使能 */
#define XHAL_SHELL         (1) /*!< Shell 命令行使能 */
#define XHAL_TRACE         (1) /*!< 崩溃回溯(backtrace)使能 */

/* 断言模块配置 --------------------------------------------------------------*/
#define XASSERT_ENABLE           (1) /*!< 断言功能总开关 */
#define XASSERT_FULL_PATH_ENABLE (1) /*!< 断言打印完整文件路径 */
#define XASSERT_FUNC_ENABLE      (1) /*!< 断言打印函数名 */
#define XASSERT_BACKTRACE_ENABLE (1) /*!< 断言失败回溯使能 */
#define XASSERT_USER_HOOK_ENABLE (1) /*!< 断言用户钩子使能 */

/* 内存管理模块配置 ----------------------------------------------------------*/
#define XMALLOC_BLOCK_SIZE (16)      /*!< 内存块大小(字节) */
#define XMALLOC_MAX_SIZE   (20 * 1024) /*!< 内存池总大小(字节) */

/* 日志模块配置 --------------------------------------------------------------*/
#define XLOG_COLOR_ENABLE      (1) /*!< 日志颜色显示 */
#define XLOG_NEWLINE_ENABLE    (1) /*!< 日志自动换行 */
#define XLOG_FILEINFO_ENABLE   (0) /*!< 日志打印文件信息 */
#define XLOG_FULL_PATH_ENABLE  (0) /*!< 日志打印完整文件路径 */
#define XLOG_DEFAULT_TIME_MODE (XLOG_TIME_MOD_RELATIVE) /*!< 日志默认时间模式 */
#define XLOG_DEFAULT_LEVEL     (XLOG_LEVEL_DEBUG)       /*!< 日志默认输出级别 */
#define XLOG_COMPILE_LEVEL     (XLOG_LEVEL_DEBUG)       /*!< 日志编译级别 */

/* 时间模块配置 --------------------------------------------------------------*/
#define XTIME_USE_DWT_DELAY (0) /*!< 使用 DWT 精确延时 */
#define XTIME_NOP()         __NOP() /*!< 空操作指令 */

/* 系统节拍配置 --------------------------------------------------------------*/
#define XOS_TICK_RATE_HZ (1000) /*!< 系统节拍频率(Hz) */

/* Shell 模块配置 ------------------------------------------------------------*/
#define XSHELL_BUFFER_SIZE           (512) /*!< Shell 缓冲区大小 */
#define XSHELL_DEFAULT_USER          "user_name" /*!< 默认用户名 */
#define XSHELL_DEFAULT_USER_PASSWORD "pswd"      /*!< 默认用户密码 */
#define XSHELL_TEXT_INFO                               \
    "\r\n\r\n"                                         \
    "Project Name:   " FIRMWARE_NAME " \r\n"           \
    "HardWare Ver:     " HARDWARE_VERSION "\r\n"       \
    "SoftWare Ver:     " SOFTWARE_VERSION "\r\n"       \
    "Copyright:  (c) 2026 " XSHELL_DEFAULT_USER "\r\n" \
    "Type 'help' to list commands.\r\n" /*!< Shell 开机信息 */

/* 回溯模块配置 --------------------------------------------------------------*/
#define XTRACE_CPU_PLATFORM_TYPE  CMB_CPU_ARM_CORTEX_M3 /*!< CPU 平台类型 */
#define XTRACE_CMB_PRINT_LANGUAGE CMB_PRINT_LANGUAGE_ENGLISH /*!< 回溯打印语言 */

/* 地址范围配置 --------------------------------------------------------------*/
#define XHAL_VALID_RAM_START   (0x20000000U) /*!< RAM 起始地址 */
#define XHAL_VALID_RAM_END     (0x2000BFFFU) /*!< RAM 结束地址 */
#define XHAL_VALID_FLASH_START (0x08000000U) /*!< Flash 起始地址 */
#define XHAL_VALID_FLASH_END   (0x08040000U) /*!< Flash 结束地址 */

/* 系统操作宏 ----------------------------------------------------------------*/
#define XHAL_POWER_RESET() NVIC_SystemReset() /*!< 系统复位 */
#define XHAL_DISABLE_IRQ() __disable_irq()    /*!< 关闭全局中断 */
#define XHAL_ENABLE_IRQ()  __enable_irq()     /*!< 开启全局中断 */

/* 紧急输出宏 ----------------------------------------------------------------*/
#define xhal_emerg_put_init() (void)0 /*!< 紧急输出初始化 */

#define xhal_emerg_putc(c) (void)(c) /*!< 紧急输出单字符 */

#define xhal_emerg_puts(s) (void)(s) /*!< 紧急输出字符串 */

/** @} */

/** @} */

/** @} */

#endif /* __XHAL_CONFIG_H */
