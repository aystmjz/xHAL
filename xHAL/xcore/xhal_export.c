/**
 ******************************************************************************
 * @file    xhal_export.c
 * @author  aystmjz
 * @brief   导出模块源文件，实现初始化/退出导出表的扫描与按级别执行
 * @version 2.3.0
 * @date    2026-08-23
 ******************************************************************************
 * Copyright (c) 2026 aystmjz. All rights reserved.
 * SPDX-License-Identifier: MIT
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "xhal_export.h"
#include "xhal_common.h"
#include "xhal_log.h"
#include "xhal_time.h"
#include <stdio.h>
#include XHAL_DEVICE_HEADER

#if (XHAL_UNIT_TEST == 1)
    #include "../xtest/Unity/unity_fixture.h"
#endif /* (XHAL_UNIT_TEST == 1) */

#if (XHAL_TRACE == 1)
    #include "../xtrace/xhal_trace.h"
#endif /* (XHAL_TRACE == 1) */

#if (XHAL_SHELL == 1)
    #include "../xshell/xhal_shell.h"
#endif /* (XHAL_SHELL == 1) */

#if (XHAL_OS_SUPPORTING == 1)
    #include "../xos/xhal_os.h"
    #include "../xperiph/xhal_periph.h"
#endif /* (XHAL_OS_SUPPORTING == 1) */

/* Private variables ---------------------------------------------------------*/
#if (XHAL_OS_SUPPORTING == 1)
static const osThreadAttr_t export_thread_attr = {
    .name       = "ThreadExport",
    .priority   = osPriorityRealtime,
    .stack_size = 2048,
};
#else
xcoro_manager_t g_coro_manager; /*!< 全局协程管理器(非 OS 模式) */
#endif /* (XHAL_OS_SUPPORTING == 1) */

static const xhal_export_t *xexport_init_table = NULL; /* 初始化导出表 */
static const xhal_export_t *xexport_exit_table = NULL; /* 退出导出表 */

static uint32_t xexport_init_count = 0; /* 初始化导出函数计数 */
static uint32_t xexport_exit_count = 0; /* 退出导出函数计数 */

static int16_t xexport_init_level_max = 0; /* 最大初始化导出级别 */
static int16_t xexport_exit_level_max = 0; /* 最大退出导出级别 */

static uint32_t xexport_init_ok   = 0; /* 初始化成功计数 */
static uint32_t xexport_init_fail = 0; /* 初始化失败计数 */

/* Private function prototypes -----------------------------------------------*/
#if (XHAL_SHELL == 1)
extern xhal_err_t shell_init(void);
#endif /* (XHAL_SHELL == 1) */

#if (XHAL_OS_SUPPORTING == 1)
extern osMutexId_t _get_xperiph_mutex(void);
extern osMutexId_t _get_xlog_mutex(void);
extern osMutexId_t _get_xtime_mutex(void);
static void _export_thread(void *para);
#endif /* (XHAL_OS_SUPPORTING == 1) */

static void _get_init_export_table(void);
static void _get_exit_export_table(void);
static void _export_init_func(int16_t level);
static void _export_exit_func(int16_t level);
static void _print_emerg_init_summary(uint32_t start_tick);

/* Exported functions --------------------------------------------------------*/
/**
 * @brief  启动 xHAL 系统
 * @note   打印 Logo 与版本信息，初始化崩溃回溯，扫描导出表，
 *         然后按 OS 模式创建初始化线程启动内核调度，
 *         或非 OS 模式顺序执行初始化后运行协程调度器
 */
void xhal_run(void)
{
    xhal_emerg_put_init();

    xhal_emerg_puts("\r\n\r\n");
    xhal_emerg_puts(xhal_logo);
    xhal_emerg_puts("Version: ");
    xhal_emerg_puts(xhal_version_str());
    xhal_emerg_puts("\r\nBuild Date: ");
    xhal_emerg_puts(XHAL_BUILD_DATE);
    xhal_emerg_puts("\r\nBuild Time: ");
    xhal_emerg_puts(XHAL_BUILD_TIME);
    xhal_emerg_puts("\r\n\r\n");

    xhal_emerg_puts("Starting ...\r\n");

#if (XHAL_TRACE == 1)
    cm_backtrace_init(FIRMWARE_NAME, HARDWARE_VERSION, SOFTWARE_VERSION);
#endif /* XHAL_TRACE */

    _get_init_export_table();
    _get_exit_export_table();

#if (XHAL_OS_SUPPORTING == 1)
    osKernelInitialize();

    (void)_get_xtime_mutex();
    (void)_get_xlog_mutex();
    (void)_get_xperiph_mutex();

    osThreadNew(_export_thread, NULL, &export_thread_attr);
    osKernelStart();
#else
    xcoro_manager_init(&g_coro_manager);
    uint32_t init_start_tick = xtime_get_tick_ms();
    for (uint16_t level = 0; level <= xexport_init_level_max; level++)
    {
        _export_init_func(level);
    }

    _print_emerg_init_summary(init_start_tick);

    #if (XHAL_SHELL == 1)
    shell_init();
    #endif /* (XHAL_SHELL == 1) */

    xcoro_scheduler_run(&g_coro_manager);
#endif     /* XHAL_OS_SUPPORTING */

    while (1)
    {
    }
}

/**
 * @brief  退出 xHAL 系统
 * @note   按导出级别逆序执行退出函数，输出完成信息后延时并关闭全局中断
 */
void xhal_exit(void)
{
    static bool exited = false;
    if (exited)
    {
        return;
    }
    exited = true;

    for (int16_t level = xexport_exit_level_max; level >= 0; level--)
    {
        _export_exit_func(level);
    }

    xhal_emerg_puts("Exit completed\r\n");

    xtime_delay_ms(100);

    XHAL_DISABLE_IRQ();
}

/**
 * @brief  执行所有单元测试导出函数
 */
void xhal_unit_test(void)
{
    _export_init_func(EXPORT_LEVEL_TEST);
}

/* Private functions ---------------------------------------------------------*/
/**
 * @brief  空初始化函数，用于标记导出表起始位置
 * @retval XHAL_OK
 */
static xhal_err_t null_init(void)
{
    return XHAL_OK;
}
INIT_EXPORT(null_init, EXPORT_LEVEL_NULL);

/**
 * @brief  空退出函数，用于标记退出表起始位置
 * @retval XHAL_OK
 */
static xhal_err_t null_exit(void)
{
    return XHAL_OK;
}
EXIT_EXPORT(null_exit, EXPORT_LEVEL_NULL);

/**
 * @brief  扫描初始化导出表
 * @note   从 init_null_init 向前回溯，校验魔数直到找到表头，
 *         统计导出函数总数并更新最大导出级别
 */
static void _get_init_export_table(void)
{
    xhal_export_t *func_block = (xhal_export_t *)&init_null_init;
    xhal_pointer_t address_last;

    while (1)
    {
        address_last = ((xhal_pointer_t)func_block - sizeof(xhal_export_t));
        xhal_export_t *table = (xhal_export_t *)address_last;
        if (table->magic_head != EXPORT_ID_INIT ||
            table->magic_tail != EXPORT_ID_INIT)
        {
            break; /* 如果不是有效的初始化导出项，则退出循环 */
        }
        func_block = table;
    }
    xexport_init_table = func_block; /* 设置初始化导出表起始地址 */

    uint32_t i = 0;
    while (1)
    {
        if (xexport_init_table[i].magic_head == EXPORT_ID_INIT &&
            xexport_init_table[i].magic_tail == EXPORT_ID_INIT)
        {
            if (xexport_init_table[i].level > xexport_init_level_max)
            {
                /* 更新最大导出级别 */
                xexport_init_level_max = xexport_init_table[i].level;
            }
            i++;
        }
        else
        {
            break; /* 如果不是有效的初始化导出项，则退出循环 */
        }
    }
    xexport_init_count = i; /* 设置初始化导出函数计数 */
}

/**
 * @brief  扫描退出导出表
 * @note   从 exit_null_exit 向前回溯，校验魔数直到找到表头，
 *         统计退出函数总数并更新最大退出级别
 */
static void _get_exit_export_table(void)
{
    xhal_export_t *func_block = (xhal_export_t *)&exit_null_exit;
    xhal_pointer_t address_last;

    while (1)
    {
        address_last = ((xhal_pointer_t)func_block - sizeof(xhal_export_t));
        xhal_export_t *table = (xhal_export_t *)address_last;
        if (table->magic_head != EXPORT_ID_EXIT ||
            table->magic_tail != EXPORT_ID_EXIT)
        {
            break;
        }
        func_block = table;
    }

    xexport_exit_table = func_block;

    uint32_t i = 0;
    while (1)
    {
        if (xexport_exit_table[i].magic_head == EXPORT_ID_EXIT &&
            xexport_exit_table[i].magic_tail == EXPORT_ID_EXIT)
        {
            if (xexport_exit_table[i].level > xexport_exit_level_max)
            {
                /* 更新最大导出级别 */
                xexport_exit_level_max = xexport_exit_table[i].level;
            }
            i++;
        }
        else
        {
            break; /* 非有效退出项，退出循环 */
        }
    }

    xexport_exit_count = i; /* 设置退出导出函数计数 */
}

/**
 * @brief  执行指定级别的初始化导出函数
 * @param  level: 导出级别
 * @note   统计执行耗时与成功/失败次数，并打印每条 initcall 的执行结果
 */
static void _export_init_func(int16_t level)
{
    for (uint32_t i = 0; i < xexport_init_count; i++)
    {
        const xhal_export_t *exp = &xexport_init_table[i];
        if (exp->level != level)
            continue;

        if (level == EXPORT_LEVEL_TEST)
        {
            ((void (*)(void))exp->func)();
        }
        else
        {
            xhal_tick_t start   = xtime_get_tick_ms();
            xhal_err_t ret      = exp->func();
            xhal_tick_t elapsed = TIME_DIFF(xtime_get_tick_ms(), start);

            if (ret == XHAL_OK)
            {
                xexport_init_ok++;
            }
            else
            {
                xexport_init_fail++;
            }

            char uptime[20];
            xtime_get_format_uptime(uptime, sizeof(uptime));
#if (XLOG_COLOR_ENABLE == 1)
            const char *icon = (ret == XHAL_OK) ? "\033[1;32m[  OK  ]\033[0m"
                                                : "\033[1;31m[ FAIL ]\033[0m";
#else
            const char *icon = (ret == XHAL_OK) ? "[  OK  ]" : "[ FAIL ]";
#endif /* (XLOG_COLOR_ENABLE == 1) */
            XLOG_PRINTF("[%s] %s initcall %-20s returned %s after %lums\r\n",
                        uptime, icon, exp->name, xhal_err_to_str(ret),
                        (unsigned long)elapsed);
        }
    }
}

/**
 * @brief  执行指定级别的退出导出函数
 * @param  level: 导出级别
 * @note   打印每条 exitcall 的执行结果与耗时
 */
static void _export_exit_func(int16_t level)
{
    for (uint32_t i = 0; i < xexport_exit_count; i++)
    {
        const xhal_export_t *exp = &xexport_exit_table[i];
        if (exp->level != level)
            continue;

        xhal_tick_t start   = xtime_get_tick_ms();
        xhal_err_t ret      = exp->func();
        xhal_tick_t elapsed = TIME_DIFF(xtime_get_tick_ms(), start);

        char uptime[20];
        xtime_get_format_uptime(uptime, sizeof(uptime));
#if (XLOG_COLOR_ENABLE == 1)
        const char *icon = (ret == XHAL_OK) ? "\033[1;32m[  OK  ]\033[0m"
                                            : "\033[1;31m[ FAIL ]\033[0m";
#else
        const char *icon = (ret == XHAL_OK) ? "[  OK  ]" : "[ FAIL ]";
#endif /* (XLOG_COLOR_ENABLE == 1) */
        XLOG_PRINTF("[%s] %s exitcall  %-20s returned %s after %lums\r\n",
                    uptime, icon, exp->name, xhal_err_to_str(ret),
                    (unsigned long)elapsed);
    }
}

/**
 * @brief  打印初始化统计
 * @param  start_tick: 初始化起始 tick
 */
static void _print_emerg_init_summary(uint32_t start_tick)
{
    char buf[80];
    uint32_t elapsed = TIME_DIFF(xtime_get_tick_ms(), start_tick);

    xhal_emerg_puts("      \r\n");

    snprintf(buf, sizeof(buf),
             "Init done: %lu OK, %lu FAIL, total %lu, Elapsed: %lums\r\n",
             (unsigned long)xexport_init_ok, (unsigned long)xexport_init_fail,
             (unsigned long)(xexport_init_ok + xexport_init_fail),
             (unsigned long)elapsed);
    xhal_emerg_puts(buf);
    xhal_emerg_puts("System ready.\r\n");
}

#if (XHAL_OS_SUPPORTING == 1)
/**
 * @brief  初始化导出线程(OS 模式)
 * @note   依次执行各级初始化导出函数，打印统计信息，
 *         初始化 Shell 后退出线程
 * @param  para: 线程参数(未使用)
 */
static void _export_thread(void *para)
{
    uint32_t init_start_tick = xtime_get_tick_ms();

    for (uint16_t level = 0; level <= xexport_init_level_max; level++)
    {
        _export_init_func(level);
    }

    _print_emerg_init_summary(init_start_tick);

    #if (XHAL_SHELL == 1)
    shell_init();
    #endif /* (XHAL_SHELL == 1) */

    osThreadExit();
}
#endif /* (XHAL_OS_SUPPORTING == 1) */

/* ---------------------------------------------------------------------------*/
