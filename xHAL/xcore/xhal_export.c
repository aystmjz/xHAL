#include "xhal_export.h"
#include "xhal_assert.h"
#include "xhal_def.h"
#include "xhal_log.h"
#include "xhal_malloc.h"
#include "xhal_time.h"
#include <stdio.h>

XLOG_TAG("xExport");

#ifdef XHAL_UNIT_TEST
#include "../xtest/Unity/unity_fixture.h"
#endif

#ifdef XHAL_OS_SUPPORTING
#include "../xos/xhal_os.h"

#define XEXPORT_THREAD_OVERHEAD    (160)
#define XEXPORT_DEFAULT_STACK_SIZE (512)
#define XEXPORT_DEFAULT_PRIORITY   (osPriorityNormal)

#ifndef XEXPORT_THREAD_STACK_SIZE
#define XEXPORT_THREAD_STACK_SIZE (1024)
#endif

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

static void _export_init_func(int16_t level);
static void _export_poll_func(void);

static void module_null_init(void)
{
    /* do nothing */
}
INIT_EXPORT(module_null_init, 0);
static void module_null_poll(void)
{
    osThreadExit();
}
POLL_EXPORT(module_null_poll, (60 * 60 * 60 * 60));

static const xhal_export_t *export_init_table = NULL; /* 初始化导出表 */
static const xhal_export_t *export_poll_table = NULL; /* 轮询导出表 */
static uint32_t count_export_init = 0; /* 初始化导出函数计数 */
static uint32_t count_export_poll = 0; /* 轮询导出函数计数 */
static int16_t export_level_max   = 0; /* 最大导出级别 */

/**
 * @brief  eLab启动函数
 */
void xhal_run(void)
{
    _get_poll_export_table();
    _get_init_export_table();

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
    _export_init_func(EXPORT_UNIT_TEST);

    while (1)
    {
    }
#else
    for (uint16_t level = 0; level <= export_level_max; level++)
    {
        _export_init_func(level);
    }

    _export_poll_func();
#endif /* XHAL_UNIT_TEST */

#endif /* XHAL_OS_SUPPORTING */
}

static void _get_init_export_table(void)
{
    xhal_export_t *func_block = (xhal_export_t *)&init_module_null_init;
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
            if (export_init_table[i].level > export_level_max)
            {
                /* 更新最大导出级别 */
                export_level_max = export_init_table[i].level;
            }
            i++;
        }
        else
        {
            break; /* 如果不是有效的初始化导出项，则退出循环 */
        }
    }
    count_export_init = i; /* 设置初始化导出函数计数 */

    XLOG_DEBUG("Export init table: %d", count_export_init);
    XLOG_DEBUG("Export init level max: %d", export_level_max);
}

static void _get_poll_export_table(void)
{
    xhal_export_t *func_block = ((xhal_export_t *)&poll_module_null_poll);
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
    count_export_poll = i; /* 设置轮询导出函数计数 */

    XLOG_DEBUG("Export poll table: %d", count_export_poll);
}

static void _export_init_func(int16_t level)
{
    for (uint32_t i = 0; i < count_export_init; i++)
    {
        if (export_init_table[i].level == level)
        {
            if (!export_init_table[i].exit)
            {

#ifdef XHAL_UNIT_TEST
                if (level == EXPORT_UNIT_TEST)
                {
                    XLOG_INFO("Export unit test: %s",
                              export_init_table[i].name);
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
}

#ifdef XHAL_OS_SUPPORTING
static void _entry_start_export(void *para)
{
    for (uint16_t level = 0; level <= export_level_max; level++)
    {
        _export_init_func(level);
    }

    _export_poll_func();

    uint16_t perused   = xmem_perused();
    uint32_t free_size = xmem_free_size();

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
    while (1)
    {
        uint32_t start = osKernelGetTickCount();

        ((void (*)(void)) export->func)();

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
}

static void _export_poll_func(void)
{
    for (uint32_t i = 0; i < count_export_poll; i++)
    {
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
        for (uint32_t i = 0; i < count_export_poll; i++)
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
