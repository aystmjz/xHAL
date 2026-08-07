#include "xhal_export.h"
#include "xhal_common.h"
#include "xhal_log.h"
#include "xhal_time.h"
#include <stdio.h>
#include XHAL_DEVICE_HEADER

#if (XHAL_UNIT_TEST == 1)
    #include "../xtest/Unity/unity_fixture.h"
#endif

#if (XHAL_TRACE == 1)
    #include "../xtrace/xhal_trace.h"
#endif

#if (XHAL_SHELL == 1)
    #include "../xshell/xhal_shell.h"
extern xhal_err_t shell_init(void);
#endif

#if (XHAL_OS_SUPPORTING == 1)
    #include "../xos/xhal_os.h"
static const osThreadAttr_t export_thread_attr = {
    .name       = "ThreadExport",
    .priority   = osPriorityRealtime,
    .stack_size = 2048,
};
static void _export_thread(void *para);
#else
xcoro_manager_t g_coro_manager;
#endif

static void _get_init_export_table(void);
static void _get_exit_export_table(void);

static void _export_init_func(int16_t level);
static void _export_exit_func(int16_t level);

static void _print_emerg_init_summary(uint32_t start_tick);

static xhal_err_t null_init(void)
{
    return XHAL_OK;
}
INIT_EXPORT(null_init, EXPORT_LEVEL_NULL);

static xhal_err_t null_exit(void)
{
    return XHAL_OK;
}
EXIT_EXPORT(null_exit, EXPORT_LEVEL_NULL);

static const xhal_export_t *xexport_init_table = NULL; /* 初始化导出表 */
static const xhal_export_t *xexport_exit_table = NULL; /* 退出导出表 */

static uint32_t xexport_init_count = 0; /* 初始化导出函数计数 */
static uint32_t xexport_exit_count = 0; /* 退出导出函数计数 */

static int16_t xexport_init_level_max = 0; /* 最大初始化导出级别 */
static int16_t xexport_exit_level_max = 0; /* 最大退出导出级别 */

static uint32_t xexport_init_ok   = 0; /* 初始化成功计数 */
static uint32_t xexport_init_fail = 0; /* 初始化失败计数 */

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
    #endif

    xcoro_scheduler_run(&g_coro_manager);
#endif /* XHAL_OS_SUPPORTING */

    while (1)
    {
    }
}

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

void xhal_unit_test(void)
{
    _export_init_func(EXPORT_LEVEL_TEST);
}

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
#endif
            XLOG_PRINTF("[%s] %s initcall %-20s returned %s after %lums\r\n",
                        uptime, icon, exp->name, xhal_err_to_str(ret),
                        (unsigned long)elapsed);
        }
    }
}

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
#endif
        XLOG_PRINTF("[%s] %s exitcall  %-20s returned %s after %lums\r\n",
                    uptime, icon, exp->name, xhal_err_to_str(ret),
                    (unsigned long)elapsed);
    }
}

/**
 * @brief 打印初始化统计
 *
 * @param start_tick 初始化起始 tick
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
    #endif

    osThreadExit();
}
#endif
