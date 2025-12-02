#include "xhal_export.h"
#include "xhal_assert.h"
#include "xhal_def.h"
#include "xhal_log.h"
#include "xhal_malloc.h"
#include "xhal_time.h"
#include <stdio.h>

XLOG_TAG("xExport");

#define XEXPORT_DISABLE_IRQ() __disable_irq()

#ifdef XHAL_UNIT_TEST
#include "../xtest/Unity/unity_fixture.h"
#endif

#ifdef XHAL_OS_SUPPORTING
#include "../xos/xhal_os.h"

#define XEXPORT_THREAD_OVERHEAD    (160)
#define XEXPORT_DEFAULT_STACK_SIZE (1024)
#define XEXPORT_DEFAULT_PRIORITY   (osPriorityNormal)

#ifndef XEXPORT_THREAD_STACK_SIZE
#define XEXPORT_THREAD_STACK_SIZE (1024)
#endif

static osMutexId_t _xexport_mutex(void);

static osMutexId_t xexport_mutex              = NULL;
static const osMutexAttr_t xexport_mutex_attr = {
    .name      = "xexport_mutex",
    .attr_bits = osMutexPrioInherit | osMutexRecursive,
    .cb_mem    = NULL,
    .cb_size   = 0,
};

static const osThreadAttr_t export_thread_attr = {
    .name       = "ThreadExport",
    .attr_bits  = osThreadDetached,
    .priority   = osPriorityRealtime,
    .stack_size = XEXPORT_THREAD_STACK_SIZE,
};
static void _entry_start_export(void *para);
static void _poll_thread(void *arg);
#endif

static void _get_poll_export_table(void);
static void _get_init_export_table(void);
static void _get_exit_export_table(void);

static void _export_init_func(int16_t level);
static void _export_poll_func(void);

static void null_poll(void)
{
#ifdef XHAL_OS_SUPPORTING
    osThreadExit();
#endif
}
POLL_EXPORT(null_poll, (60 * 1000));
static void null_init(void)
{
    /* do nothing */
}
INIT_EXPORT(null_init, EXPORT_LEVEL_NULL);
static void null_exit(void)
{
    /* do nothing */
}
EXIT_EXPORT(null_exit, EXPORT_LEVEL_NULL);

bool xhal_shutdown = 0;

static uint32_t poll_thread_alive_count = 0;

static const xhal_export_t *export_poll_table = NULL; /* 轮询导出表 */
static const xhal_export_t *export_init_table = NULL; /* 初始化导出表 */
static const xhal_export_t *export_exit_table = NULL; /* 退出导出表 */

static uint32_t export_poll_count    = 0; /* 轮询导出函数计数 */
static uint32_t export_init_count    = 0; /* 初始化导出函数计数 */
static uint32_t export_exit_count    = 0; /* 退出导出函数计数 */
static int16_t export_init_level_max = 0; /* 最大初始化导出级别 */
static int16_t export_exit_level_max = 0; /* 最大退出导出级别 */

/**
 * @brief  eLab启动函数
 */
void xhal_run(void)
{
#if XASSERT_BACKTRACE_ENABLE
    cm_backtrace_init(XTRACE_FIRMWARE_NAME, XTRACE_HARDWARE_VERSION,
                      XTRACE_SOFTWARE_VERSION);
#endif /* XASSERT_BACKTRACE_ENABLE */

    _get_poll_export_table();
    _get_init_export_table();
    _get_exit_export_table();

#ifdef XHAL_OS_SUPPORTING

#ifdef XHAL_UNIT_TEST
    XLOG_WARN("Not implemented");
    while (1)
    {
    }
#else
    osKernelInitialize();
    osThreadNew(_entry_start_export, NULL, &export_thread_attr);
    osKernelStart();
    while (1)
    {
    }
#endif /* XHAL_UNIT_TEST */

#else

#ifdef XHAL_UNIT_TEST
    _export_init_func(EXPORT_LEVEL_TEST);

    while (1)
    {
    }
#else
    for (uint16_t level = 0; level <= export_init_level_max; level++)
    {
        _export_init_func(level);
    }

    _export_poll_func();
#endif /* XHAL_UNIT_TEST */

#endif /* XHAL_OS_SUPPORTING */
}

void xhal_exit(void)
{
    static bool exited = false;
    if (exited)
    {
        XLOG_WARN("xhal_exit() has already been called");
        return;
    }
    exited = true;

    XLOG_INFO("xHAL exiting...");

#ifdef XHAL_OS_SUPPORTING
    XLOG_INFO("Stopping poll threads...");

    osStatus_t ret_os = osOK;
    osMutexId_t mutex = _xexport_mutex();
    ret_os            = osMutexAcquire(mutex, osWaitForever);
    xassert(ret_os == osOK);

    poll_thread_alive_count--;

    ret_os = osMutexRelease(mutex);
    xassert(ret_os == osOK);

    xhal_shutdown = true;

    uint32_t wait_ms        = 0;
    const uint32_t wait_max = 5000;

    while (poll_thread_alive_count > 0 && wait_ms < wait_max)
    {
        osDelay(1);
        wait_ms++;
    }

    if (poll_thread_alive_count > 0)
    {
        XLOG_WARN("Some poll threads did not exit in time");
    }
#endif

    XLOG_INFO("Run exit funcs...");

    for (int16_t level = export_exit_level_max; level >= 0; level--)
    {
        for (uint32_t i = 0; i < export_exit_count; i++)
        {
            const xhal_export_t *exp = &export_exit_table[i];
            if (exp->level != level)
                continue;

            XLOG_INFO("Exit: %s", exp->name);
            ((void (*)(void))exp->func)();
        }
    }

    XLOG_INFO("xHAL exit completed");

    xtime_delay_ms(100);

#ifdef XHAL_OS_SUPPORTING
    osKernelLock();
#endif

    XEXPORT_DISABLE_IRQ();
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
    export_init_table = func_block; /* 设置初始化导出表起始地址 */

    uint32_t i = 0;
    while (1)
    {
        if (export_init_table[i].magic_head == EXPORT_ID_INIT &&
            export_init_table[i].magic_tail == EXPORT_ID_INIT)
        {
            if (export_init_table[i].level > export_init_level_max)
            {
                /* 更新最大导出级别 */
                export_init_level_max = export_init_table[i].level;
            }
            i++;
        }
        else
        {
            break; /* 如果不是有效的初始化导出项，则退出循环 */
        }
    }
    export_init_count = i; /* 设置初始化导出函数计数 */

    XLOG_DEBUG("Export init table: %d", export_init_count);
    XLOG_DEBUG("Export init level max: %d", export_init_level_max);
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

    export_exit_table = func_block;

    uint32_t i = 0;
    while (1)
    {
        if (export_exit_table[i].magic_head == EXPORT_ID_EXIT &&
            export_exit_table[i].magic_tail == EXPORT_ID_EXIT)
        {
            if (export_exit_table[i].level > export_exit_level_max)
            {
                /* 更新最大导出级别 */
                export_exit_level_max = export_exit_table[i].level;
            }
            i++;
        }
        else
        {
            break; /* 非有效退出项，退出循环 */
        }
    }

    export_exit_count = i; /* 设置退出导出函数计数 */

    XLOG_DEBUG("Export exit table: %d", export_exit_count);
    XLOG_DEBUG("Export exit level max: %d", export_exit_level_max);
}

static void _get_poll_export_table(void)
{
    xhal_export_t *func_block = ((xhal_export_t *)&poll_null_poll);
    xhal_pointer_t address_last;

    while (1)
    {
        address_last = ((xhal_pointer_t)func_block - sizeof(xhal_export_t));
        xhal_export_t *table = (xhal_export_t *)address_last;
        if (table->magic_head != EXPORT_ID_POLL ||
            table->magic_tail != EXPORT_ID_POLL)
        {
            break; /* 如果不是有效的轮询导出项，则退出循环 */
        }
        func_block = table;
    }
    export_poll_table = func_block; /* 设置轮询导出表起始地址 */

    uint32_t i = 0;
    while (1)
    {
        if (export_poll_table[i].magic_head == EXPORT_ID_POLL &&
            export_poll_table[i].magic_tail == EXPORT_ID_POLL)
        {
            xhal_export_poll_data_t *data =
                (xhal_export_poll_data_t *)export_poll_table[i].data;
            /* 设置超时时间 */
            data->timeout_ms =
                xtime_get_tick_ms() + export_poll_table[i].period_ms;
            i++;
        }
        else
        {
            break; /* 如果不是有效的轮询导出项，则退出循环 */
        }
    }
    export_poll_count = i; /* 设置轮询导出函数计数 */

    XLOG_DEBUG("Export poll table: %d", export_poll_count);
}

static void _export_init_func(int16_t level)
{
    for (uint32_t i = 0; i < export_init_count; i++)
    {
        if (export_init_table[i].level == level)
        {
#ifdef XHAL_UNIT_TEST
            if (level == EXPORT_LEVEL_TEST)
            {
                XLOG_INFO("Export unit test: %s", export_init_table[i].name);
                const char *argv[] = {export_init_table[i].name, "-v"};
                int argc           = 2;
                UnityMain(argc, argv,
                          ((void (*)(void))export_init_table[i].func));
                continue;
            }
#endif
            XLOG_INFO("Export init: %s", export_init_table[i].name);

            ((void (*)(void))export_init_table[i].func)();
        }
    }
}

#ifdef XHAL_OS_SUPPORTING
static void _entry_start_export(void *para)
{
    for (uint16_t level = 0; level <= export_init_level_max; level++)
    {
        _export_init_func(level);
    }

    uint16_t perused   = xmem_perused();
    uint32_t free_size = xmem_free_size();

    XLOG_INFO("Init thread ended, Memory usage: %d.%d%%, Free size: %lu bytes",
              perused / 10, perused % 10, free_size);

    _export_poll_func();

    perused   = xmem_perused();
    free_size = xmem_free_size();

    XLOG_INFO("Poll thread ended, Memory usage: %d.%d%%, Free size: %lu bytes",
              perused / 10, perused % 10, free_size);

    osThreadExit();
}

static void _poll_thread(void *arg)
{
    xhal_export_t *export = (xhal_export_t *)arg;
    uint32_t period_ticks = XOS_MS_TO_TICKS(export->period_ms);
    uint32_t next_wake    = osKernelGetTickCount() + period_ticks;

#ifdef XDEBUG
    XLOG_DEBUG("Poll thread started: %s, period: %lu ms(%lu ticks)",
               export->name, export->period_ms, period_ticks);
#endif

    osStatus_t ret_os = osOK;
    osMutexId_t mutex = _xexport_mutex();
    ret_os            = osMutexAcquire(mutex, osWaitForever);
    xassert(ret_os == osOK);

    poll_thread_alive_count++;

    ret_os = osMutexRelease(mutex);
    xassert(ret_os == osOK);

    while (1)
    {
        uint32_t start = osKernelGetTickCount();

        ((void (*)(void)) export->func)();

        if (xhal_shutdown)
        {
            break;
        }

        if (period_ticks == 0)
        {
            continue;
        }

        uint32_t end        = osKernelGetTickCount();
        uint32_t exec_ticks = end - start;

        if (exec_ticks > period_ticks)
        {
            XLOG_WARN("Poll task '%s' execution overrun: %lu > %lu ticks",
                      export->name, exec_ticks, period_ticks);
        }

        next_wake += period_ticks;
        osDelayUntil(next_wake);
    }

    ret_os = osMutexAcquire(mutex, osWaitForever);
    xassert(ret_os == osOK);

    poll_thread_alive_count--;

    ret_os = osMutexRelease(mutex);
    xassert(ret_os == osOK);

    osThreadExit();
}

static void _export_poll_func(void)
{
    for (uint32_t i = 0; i < export_poll_count; i++)
    {
        if ((xhal_pointer_t)&export_poll_table[i] ==
            (xhal_pointer_t)&poll_null_poll)
            continue;

        osThreadAttr_t attr = {
            .attr_bits = osThreadDetached,
            .name      = export_poll_table[i].name,
        };

        if (export_poll_table[i].priority != osPriorityNone)
        {
            attr.priority = export_poll_table[i].priority;
        }
        else
        {
            attr.priority = XEXPORT_DEFAULT_PRIORITY;
        }

        if (export_poll_table[i].stack_size != 0)
        {
            attr.stack_size = export_poll_table[i].stack_size;
        }
        else
        {
            attr.stack_size = (uint32_t)XEXPORT_DEFAULT_STACK_SIZE;
        }

        uint32_t free_size = 0;

        free_size = xmem_free_size();

        if (free_size < (attr.stack_size + XEXPORT_THREAD_OVERHEAD))
        {
            XLOG_ERROR("Poll thread no memory: %s, free size: %lu bytes, "
                       "required: %lu bytes (stack: %lu + overhead: %u)",
                       export_poll_table[i].name, free_size,
                       attr.stack_size + XEXPORT_THREAD_OVERHEAD,
                       attr.stack_size, XEXPORT_THREAD_OVERHEAD);
            continue;
        }
        osThreadId_t tid =
            osThreadNew(_poll_thread, (void *)&export_poll_table[i], &attr);
        if (tid == NULL)
        {
            XLOG_ERROR("Poll thread creation failed: %s", attr.name);
        }
        else
        {
#ifdef XDEBUG
            XLOG_DEBUG("Poll thread created: %s, stack size: %lu bytes",
                       attr.name, attr.stack_size);
#endif
        }
    }
}
#else

static void _export_poll_func(void)
{
    xhal_export_poll_data_t *data;

    while (1)
    {
        for (uint32_t i = 0; i < export_poll_count; i++)
        {
            data = export_poll_table[i].data;

            uint64_t time = xtime_get_tick_ms();

            if (time >= data->timeout_ms)
            {
                /* 更新超时时间 */
                data->timeout_ms = time + export_poll_table[i].period_ms;

                ((void (*)(void))export_poll_table[i].func)();

                uint64_t after_time = xtime_get_tick_ms();

                if (after_time >= data->timeout_ms)
                {
                    XLOG_WARN("Poll function %s execution time exceeds period",
                              export_poll_table[i].name);
                }
            }
        }
    }
}

#endif

#ifdef XHAL_OS_SUPPORTING
static osMutexId_t _xexport_mutex(void)
{
    if (xexport_mutex == NULL)
    {
        xexport_mutex = osMutexNew(&xexport_mutex_attr);
        xassert_not_null(_xexport_mutex);
    }

    return xexport_mutex;
}
#endif