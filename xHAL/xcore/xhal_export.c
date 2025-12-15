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

#define XEXPORT_POLL_WAKE_FLAG              (1U << 0)
#define XEXPORT_POLL_EXIT_WAIT_MAX_MS       (5000)
#define XEXPORT_POLL_EXIT_CHECK_INTERVAL_MS (1000)

#define XEXPORT_THREAD_OVERHEAD             (160)
#define XEXPORT_MAX_POLL_THREADS            (32)
#define XEXPORT_DEFAULT_STACK_SIZE          (1024)
#define XEXPORT_DEFAULT_PRIORITY            (osPriorityNormal)

#ifndef XEXPORT_THREAD_STACK_SIZE
#define XEXPORT_THREAD_STACK_SIZE (1024)
#endif

bool xhal_shutdown_req                = 0;
osEventFlagsId_t xhal_poll_exit_event = NULL;

static osThreadId_t xexport_poll_thread_ids[XEXPORT_MAX_POLL_THREADS];
static uint32_t xexport_poll_thread_ids_count = 0;

static const osEventFlagsAttr_t xexport_poll_wake_flag_attr = {
    .name      = "xexport_poll_wake_flag",
    .attr_bits = 0,
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
static void _get_coro_export_table(xcoro_manager_t *mgr);
static void _get_init_export_table(void);
static void _get_exit_export_table(void);

static void _export_init_func(int16_t level);
static void _export_poll_coro_func(void);

static void null_poll(void)
{
#ifdef XHAL_OS_SUPPORTING
    osThreadExit();
#endif
}
POLL_EXPORT(null_poll, (60 * 1000));
static void null_coro(xcoro_handle_t *handle)
{
    /* do nothing */
}
CORO_EXPORT(null_coro, XCORO_PRIO_NORMAL);

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

static xcoro_manager_t xexport_coro_manager;

static const xhal_export_t *xexport_poll_table = NULL; /* 轮询导出表 */
static const xhal_export_t *xexport_coro_table = NULL; /* 协程导出表 */
static const xhal_export_t *xexport_init_table = NULL; /* 初始化导出表 */
static const xhal_export_t *xexport_exit_table = NULL; /* 退出导出表 */

static uint32_t xexport_poll_count    = 0; /* 轮询导出函数计数 */
static uint32_t xexport_coro_count    = 0; /* 协程导出函数计数 */
static uint32_t xexport_init_count    = 0; /* 初始化导出函数计数 */
static uint32_t xexport_exit_count    = 0; /* 退出导出函数计数 */
static int16_t xexport_init_level_max = 0; /* 最大初始化导出级别 */
static int16_t xexport_exit_level_max = 0; /* 最大退出导出级别 */

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
    _get_coro_export_table(&xexport_coro_manager);

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
    for (uint16_t level = 0; level <= xexport_init_level_max; level++)
    {
        _export_init_func(level);
    }

    _export_poll_coro_func();
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

    xhal_shutdown_req = true;

    if (xhal_poll_exit_event != NULL)
    {
        osEventFlagsSet(xhal_poll_exit_event, XEXPORT_POLL_WAKE_FLAG);
    }

    osThreadId_t current_tid      = osThreadGetId();
    uint32_t wait_ms              = 0;
    const uint32_t wait_max       = XEXPORT_POLL_EXIT_WAIT_MAX_MS;
    const uint32_t check_interval = XEXPORT_POLL_EXIT_CHECK_INTERVAL_MS;

    uint32_t active_threads = 0;
    for (uint32_t i = 0; i < xexport_poll_thread_ids_count; i++)
    {
        if (xexport_poll_thread_ids[i] == current_tid)
        {
            xexport_poll_thread_ids[i] = NULL;
            continue;
        }

        if (xexport_poll_thread_ids[i] != NULL)
        {
            active_threads++;
        }
    }

    while (active_threads > 0 && wait_ms < wait_max)
    {
        osDelay(XOS_MS_TO_TICKS(check_interval));
        wait_ms += check_interval;

        active_threads = 0;
        for (uint32_t i = 0; i < xexport_poll_thread_ids_count; i++)
        {
            if (xexport_poll_thread_ids[i] != NULL)
            {
                active_threads++;
            }
        }

        XLOG_DEBUG(
            "Waiting for threads to exit: %lu threads remaining, waited %lu ms",
            active_threads, wait_ms);
    }

    if (active_threads > 0)
    {
        XLOG_WARN("%lu poll threads did not exit in time.", active_threads);

        for (uint32_t i = 0; i < xexport_poll_thread_ids_count; i++)
        {
            if (xexport_poll_thread_ids[i] != NULL)
            {
                osThreadState_t state =
                    osThreadGetState(xexport_poll_thread_ids[i]);
                const char *name = osThreadGetName(xexport_poll_thread_ids[i]);
                osPriority_t priority =
                    osThreadGetPriority(xexport_poll_thread_ids[i]);

                XLOG_WARN("Forcing termination of thread: %s, state: %d, "
                          "priority: %d",
                          name ? name : "unknown", state, priority);

                osThreadTerminate(xexport_poll_thread_ids[i]);
                xexport_poll_thread_ids[i] = NULL;
            }
        }
    }

    if (xhal_poll_exit_event != NULL)
    {
        osEventFlagsDelete(xhal_poll_exit_event);
        xhal_poll_exit_event = NULL;
    }
#endif

    XLOG_INFO("Run exit funcs...");

    for (int16_t level = xexport_exit_level_max; level >= 0; level--)
    {
        for (uint32_t i = 0; i < xexport_exit_count; i++)
        {
            const xhal_export_t *exp = &xexport_exit_table[i];
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

    XLOG_DEBUG("Export init table: %d", xexport_init_count);
    XLOG_DEBUG("Export init level max: %d", xexport_init_level_max);
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

    XLOG_DEBUG("Export exit table: %d", xexport_exit_count);
    XLOG_DEBUG("Export exit level max: %d", xexport_exit_level_max);
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
    xexport_poll_table = func_block; /* 设置轮询导出表起始地址 */

    uint32_t i = 0;
    while (1)
    {
        if (xexport_poll_table[i].magic_head == EXPORT_ID_POLL &&
            xexport_poll_table[i].magic_tail == EXPORT_ID_POLL)
        {
            xhal_export_poll_data_t *data =
                (xhal_export_poll_data_t *)xexport_poll_table[i].data;
            /* 设置超时时间 */
            data->wakeup_tick_ms =
                xtime_get_tick_ms() + xexport_poll_table[i].period_ms;
            i++;
        }
        else
        {
            break; /* 如果不是有效的轮询导出项，则退出循环 */
        }
    }
    xexport_poll_count = i; /* 设置轮询导出函数计数 */

    XLOG_DEBUG("Export poll table: %d", xexport_poll_count);
}

static void _get_coro_export_table(xcoro_manager_t *mgr)
{
    xhal_export_t *func_block = ((xhal_export_t *)&coro_null_coro);
    xhal_pointer_t address_last;

    xcoro_manager_init(mgr);

    while (1)
    {
        address_last = ((xhal_pointer_t)func_block - sizeof(xhal_export_t));
        xhal_export_t *table = (xhal_export_t *)address_last;
        if (table->magic_head != EXPORT_ID_CORO ||
            table->magic_tail != EXPORT_ID_CORO)
        {
            break; /* 如果不是有效的协程导出项，则退出循环 */
        }
        func_block = table;
    }
    xexport_coro_table = func_block; /* 设置协程导出表起始地址 */

    uint32_t i = 0;
    while (1)
    {
        if (xexport_coro_table[i].magic_head == EXPORT_ID_CORO &&
            xexport_coro_table[i].magic_tail == EXPORT_ID_CORO)
        {
            xhal_export_coro_data_t *data =
                (xhal_export_coro_data_t *)xexport_coro_table[i].data;

            data->handle.entry = (xcoro_entry_t)xexport_coro_table[i].func;
            xcoro_register(mgr, &data->handle);
            i++;
        }
        else
        {
            break; /* 如果不是有效的协程导出项，则退出循环 */
        }
    }
    xexport_coro_count = i; /* 设置协程导出函数计数 */

    XLOG_DEBUG("Export coro table: %d", xexport_coro_count);
}

static void _export_init_func(int16_t level)
{
    for (uint32_t i = 0; i < xexport_init_count; i++)
    {
        if (xexport_init_table[i].level == level)
        {
#ifdef XHAL_UNIT_TEST
            if (level == EXPORT_LEVEL_TEST)
            {
                XLOG_INFO("Export unit test: %s", xexport_init_table[i].name);
                const char *argv[] = {xexport_init_table[i].name, "-v"};
                int argc           = 2;
                UnityMain(argc, argv,
                          ((void (*)(void))xexport_init_table[i].func));
                continue;
            }
#endif
            XLOG_INFO("Export init: %s", xexport_init_table[i].name);

            ((void (*)(void))xexport_init_table[i].func)();
        }
    }
}

#ifdef XHAL_OS_SUPPORTING
static void _entry_start_export(void *para)
{
    for (uint16_t level = 0; level <= xexport_init_level_max; level++)
    {
        _export_init_func(level);
    }

    uint16_t perused   = xmem_perused();
    uint32_t free_size = xmem_free_size();

    XLOG_INFO("Init thread ended, Memory usage: %d.%d%%, Free size: %lu bytes",
              perused / 10, perused % 10, free_size);

    _export_poll_coro_func();

    perused   = xmem_perused();
    free_size = xmem_free_size();

    XLOG_INFO("Poll thread ended, Memory usage: %d.%d%%, Free size: %lu bytes",
              perused / 10, perused % 10, free_size);

    osThreadExit();
}

static void _poll_thread(void *arg)
{
    xhal_export_t *export       = (xhal_export_t *)arg;
    const uint32_t period_ticks = XOS_MS_TO_TICKS(export->period_ms);

#ifdef XDEBUG
    XLOG_DEBUG("Poll thread started: %s, period: %lu ms(%lu ticks)",
               export->name, export->period_ms, period_ticks);
#endif

    uint32_t next_wake = osKernelGetTickCount();

    while (1)
    {
        next_wake += period_ticks;

        uint32_t start = osKernelGetTickCount();
        ((void (*)(void)) export->func)();
        uint32_t end = osKernelGetTickCount();

        if (xhal_shutdown_req)
        {
            break;
        }

        if (period_ticks == 0)
        {
            continue;
        }

        uint32_t exec_ticks = end - start;

        if (exec_ticks > period_ticks)
        {
            XLOG_WARN("Poll task '%s' execution overrun: %lu > %lu ticks",
                      export->name, exec_ticks, period_ticks);
        }

        int32_t wait_ticks = (int32_t)(next_wake - end);

        if (wait_ticks > 0)
        {
            uint32_t flags =
                osEventFlagsWait(xhal_poll_exit_event, XEXPORT_POLL_WAKE_FLAG,
                                 osFlagsWaitAny, wait_ticks);
            if ((flags & XEXPORT_POLL_WAKE_FLAG) != 0)
            {
                break;
            }
        }
        else
        {
            next_wake = end;
        }
    }

    osThreadId_t current_tid = osThreadGetId();
    for (uint32_t i = 0; i < xexport_poll_thread_ids_count; i++)
    {
        if (xexport_poll_thread_ids[i] == current_tid)
        {
            xexport_poll_thread_ids[i] = NULL;
            break;
        }
    }

    osThreadExit();
}

static void _export_poll_coro_func(void)
{
    xhal_poll_exit_event = osEventFlagsNew(&xexport_poll_wake_flag_attr);
    xassert_not_null(xhal_poll_exit_event);

    for (uint32_t i = 0; i < xexport_poll_count; i++)
    {
        if ((xhal_pointer_t)&xexport_poll_table[i] ==
            (xhal_pointer_t)&poll_null_poll)
            continue;

        osThreadAttr_t attr = {
            .attr_bits = osThreadDetached,
            .name      = xexport_poll_table[i].name,
        };

        if (xexport_poll_table[i].priority != osPriorityNone)
        {
            attr.priority = xexport_poll_table[i].priority;
        }
        else
        {
            attr.priority = XEXPORT_DEFAULT_PRIORITY;
        }

        if (xexport_poll_table[i].stack_size != 0)
        {
            attr.stack_size = xexport_poll_table[i].stack_size;
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
                       xexport_poll_table[i].name, free_size,
                       attr.stack_size + XEXPORT_THREAD_OVERHEAD,
                       attr.stack_size, XEXPORT_THREAD_OVERHEAD);
            continue;
        }

        osThreadId_t tid =
            osThreadNew(_poll_thread, (void *)&xexport_poll_table[i], &attr);

        if (tid == NULL)
        {
            XLOG_ERROR("Poll thread creation failed: %s", attr.name);
        }
        else
        {
            if (i < XEXPORT_MAX_POLL_THREADS)
            {
                xexport_poll_thread_ids[i] = tid;
                xexport_poll_thread_ids_count++;
            }
            else
            {
                XLOG_WARN("Too many poll threads, cannot track: %s", attr.name);
            } /* end of if (xexport_poll_count < XEXPORT_MAX_POLL_THREADS) */

#ifdef XDEBUG
            XLOG_DEBUG("Poll thread created: %s, stack size: %lu bytes",
                       attr.name, attr.stack_size);
#endif
        } /* end of if (tid == NULL) */
    }
}
#else

static void _export_poll_coro_func(void)
{
    xhal_export_poll_data_t *data;
    xcoro_manager_t *mgr = &xexport_coro_manager;

    while (1)
    {
        for (uint32_t i = 0; i < xexport_poll_count; i++)
        {
            data = xexport_poll_table[i].data;

            xhal_tick_t start = xtime_get_tick_ms();

            if (TIME_BEFOR(data->wakeup_tick_ms, start))
            {
                ((void (*)(void))xexport_poll_table[i].func)();

                xhal_tick_t end = xtime_get_tick_ms();

                data->wakeup_tick_ms = start + xexport_poll_table[i].period_ms;

                if (TIME_BEFOR(data->wakeup_tick_ms, end))
                {
                    data->wakeup_tick_ms = end;
                    XLOG_WARN("Poll function %s execution time exceeds period",
                              xexport_poll_table[i].name);
                }
            } /* if (TIME_BEFOR(data->wakeup_tick_ms, start)) */
        } /* for (uint32_t i = 0; i < xexport_poll_count; i++) */

        /* 唤醒延时到期 */
        _wake_expired_sleepers(mgr);

        /* 有 READY 协程直接运行 */
        xcoro_handle_t *handle = _get_next_ready(mgr);
        if (handle ? handle->entry : NULL)
        {
            handle->entry(handle);
        }
    } /* while (1) */
}

#endif