#include "xhal_coro.h"
#include "xhal_assert.h"
#include "xhal_log.h"
#include "xhal_malloc.h"
#include "xhal_time.h"
#include <string.h>

XLOG_TAG("xCoro");

#ifndef XCORO_EVENT_NUM_MAX
#define XCORO_EVENT_NUM_MAX (64)
#endif

static xcoro_event_t *xcoro_event_table[XCORO_EVENT_NUM_MAX];
static uint16_t xcoro_event_count = 0;

xhal_err_t xcoro_event_add(xcoro_event_t *event)
{
    xassert_not_null(event);
    xassert_not_null(event->name);
    xassert_name(xcoro_event_find(event->name) == NULL, event->name);

    for (uint16_t i = 0; i < XCORO_EVENT_NUM_MAX; i++)
    {
        if (xcoro_event_table[i] == NULL)
        {
            xcoro_event_table[i] = event;
            xcoro_event_count++;
            return XHAL_OK;
        }
    }

    return XHAL_ERR_NO_MEMORY;
}

xhal_err_t xcoro_event_remove(xcoro_event_t *event)
{
    xassert_not_null(event);

    xhal_err_t ret = XHAL_ERROR;

    for (uint16_t i = 0; i < XCORO_EVENT_NUM_MAX; i++)
    {
        if (xcoro_event_table[i] == event)
        {
            xcoro_event_table[i] = NULL;
            xcoro_event_count--;
            ret = XHAL_OK;
            break;
        }
    }

    return ret;
}

xcoro_event_t *xcoro_event_find(const char *name)
{
    xassert_not_null(name);

    xcoro_event_t *event = NULL;
    for (uint16_t i = 0; i < XCORO_EVENT_NUM_MAX; i++)
    {
        if (xcoro_event_table[i] ? xcoro_event_table[i]->name == NULL : NULL)
        {
            continue;
        }

        if (strcmp(xcoro_event_table[i]->name, name) == 0)
        {
            event = xcoro_event_table[i];
            break;
        }
    }

    return event;
}

bool xcoro_event_valid(const char *name)
{
    return xcoro_event_find(name) == NULL ? false : true;
}

bool xcoro_event_of_name(xcoro_event_t *event, const char *name)
{
    xassert_not_null(event);
    xassert_not_null(name);

    if (event->name ? strcmp(event->name, name) == 0 : NULL)
    {
        return true;
    }

    return false;
}

static void _ready_list_insert(xcoro_handle_t *handle)
{
    xassert_not_null(handle);
    xassert_not_null(handle->mgr);

    xcoro_handle_t **pp = &handle->mgr->ready_list;

    /*
     * 条件：当前节点优先级 >= 新节点优先级时，继续向后
     *
     * 1. 更高优先级的协程：插入在链表最前面
     * 2. 相同优先级的协程：插入在相同优先级段的最后面（先进先出）
     * 3. 更低优先级的协程：插入在第一个更低优先级节点前面
     */
    while (*pp && (*pp)->prio >= handle->prio)
        pp = &(*pp)->next;

    handle->next = *pp;
    *pp          = handle;
}

static void _sleep_list_insert(xcoro_handle_t *handle)
{
    xassert_not_null(handle);
    xassert_not_null(handle->mgr);

    xcoro_handle_t **pp = &handle->mgr->sleep_list;

    /*
     * 条件：当前节点唤醒时间 ≤ 新节点唤醒时间时，继续向后
     *
     * 1. 更早唤醒的协程：插入在链表最前面（如果新节点唤醒时间最早）
     * 2. 相同唤醒时间的协程：插入在相同时间段的最后面（先进先出）
     * 3. 更晚唤醒的协程：插入在第一个更晚唤醒节点前面
     */
    while (*pp && TIME_BEFOR_EQ((*pp)->wakeup_tick_ms, handle->wakeup_tick_ms))
        pp = &(*pp)->next;

    handle->next = *pp;
    *pp          = handle;
}

static void _sleep_list_find_remove(xcoro_handle_t *handle)
{
    xassert_not_null(handle);
    xassert_not_null(handle->mgr);

    xcoro_handle_t **pp = &handle->mgr->sleep_list;
    while (*pp)
    {
        if (*pp == handle)
        {
            *pp          = handle->next;
            handle->next = NULL;
            return;
        }
        pp = &(*pp)->next;
    }
}

static void _event_wait_list_find_remove(xcoro_handle_t *handle)
{
    xassert_not_null(handle);
    xassert_not_null(handle->waiting_event);

    xcoro_handle_t **pp = &handle->waiting_event->wait_list;
    while (*pp)
    {
        if (*pp == handle)
        {
            *pp          = handle->next;
            handle->next = NULL;
            return;
        }
        pp = &(*pp)->next;
    }
}
void _wake_expired_sleepers(xcoro_manager_t *mgr)
{
    xhal_tick_t now = xtime_get_tick_ms();
    xcoro_handle_t *handle;

    while ((handle = mgr->sleep_list) != NULL)
    {
        /* sleep_list 按时间排序，遇到未到期的直接退出 */
        if (TIME_AFTER(handle->wakeup_tick_ms, now))
            break;

        mgr->sleep_list = handle->next;

        if (handle->waiting_event)
        {
            _event_wait_list_find_remove(handle);
            handle->waiting_event = NULL;
            handle->wait_result   = (uint32_t)XCORO_WAIT_TIMEOUT;
            handle->wait_mask     = 0;
            handle->wait_flags    = 0;
        }

        handle->next           = NULL;
        handle->wakeup_tick_ms = 0;

        handle->state = XCORO_STATE_READY;
        _ready_list_insert(handle);
    }
}

xhal_tick_t _next_wakeup_delay_ms(xcoro_manager_t *mgr)
{
    if (mgr->sleep_list == NULL)
    {
        return 0;
    }

    xhal_tick_t now  = xtime_get_tick_ms();
    xhal_tick_t tick = mgr->sleep_list->wakeup_tick_ms;

    if (TIME_AFTER_EQ(now, tick))
    {
        return 1;
    }

    return TIME_DIFF(tick, now);
}

xcoro_handle_t *_get_next_ready(xcoro_manager_t *mgr)
{
    xcoro_handle_t *handle = mgr->ready_list;

    if (handle)
    {
        mgr->ready_list = handle->next;
        handle->next    = NULL;
    }
    return handle;
}

void xcoro_manager_init(xcoro_manager_t *mgr)
{
    xmemset(mgr, 0, sizeof(*mgr));
}

xhal_err_t xcoro_register(xcoro_manager_t *mgr, xcoro_handle_t *handle)
{
    xassert_not_null(mgr);
    xassert_not_null(handle);

    xcoro_priority_t save_prio = handle->prio;
    xcoro_entry_t save_entry   = handle->entry;
    void *save_user_data       = handle->user_data;

    xmemset(handle, 0, sizeof(*handle));

    handle->prio      = save_prio;
    handle->entry     = save_entry;
    handle->user_data = save_user_data;
    handle->mgr       = mgr;

    mgr->count++;

    handle->state = XCORO_STATE_READY;
    _ready_list_insert(handle);

    return XHAL_OK;
}

xhal_err_t xcoro_unregister(xcoro_handle_t *handle)
{

    xassert_not_null(handle);

    if (handle->mgr == NULL)
    {
        return XHAL_OK;
    }

    xcoro_handle_t **pp;
    pp = &handle->mgr->ready_list;
    while (*pp)
    {
        if (*pp == handle)
        {
            *pp = handle->next;
            break;
        }
        pp = &(*pp)->next;
    }

    if (handle->wakeup_tick_ms)
    {
        handle->wakeup_tick_ms = 0;
        _sleep_list_find_remove(handle);
    }

    if (handle->waiting_event)
    {
        _event_wait_list_find_remove(handle);
        handle->waiting_event = NULL;
        handle->wait_result   = (uint32_t)XCORO_WAIT_CANCELED;
        handle->wait_mask     = 0;
        handle->wait_flags    = 0;
    }

    handle->mgr->count--;

    handle->mgr   = NULL;
    handle->next  = NULL;
    handle->state = XCORO_STATE_FINISHED;

    return XHAL_OK;
}

bool xcoro_is_running(xcoro_handle_t *handle)
{
    return handle && (handle->state != XCORO_STATE_FINISHED);
}

void xcoro_sleep(xcoro_handle_t *handle, xhal_tick_t delay_ms)
{
    xassert_not_null(handle);
    xassert_not_null(handle->mgr);

    handle->wakeup_tick_ms = xtime_get_tick_ms() + delay_ms;
    handle->state          = XCORO_STATE_SLEEPING;

    _sleep_list_insert(handle);
}

void xcoro_wait_event(xcoro_handle_t *handle, xcoro_event_t *event,
                      uint32_t mask, uint32_t flags, uint32_t timeout_ms)
{
    xassert_not_null(handle);
    xassert_not_null(event);
    xassert(mask != 0);

    uint32_t matched = event->flags & mask;

    if ((flags & XCORO_FLAGS_WAIT_ALL) ? (matched == mask) : (matched != 0))
    {
        /* Auto-clear（默认） */
        if ((flags & XCORO_FLAGS_WAIT_NO_CLEAR) == 0)
        {
            event->flags &=
                (flags & XCORO_FLAGS_WAIT_ALL) ? (~mask) : (~matched);
        }

        handle->wait_result   = matched;
        handle->waiting_event = NULL;
        handle->wait_mask     = 0;
        handle->wait_flags    = 0;

        handle->state = XCORO_STATE_READY;
        _ready_list_insert(handle);
        return;
    }

    handle->state         = XCORO_STATE_WAITING;
    handle->waiting_event = event;
    handle->wait_mask     = mask;
    handle->wait_flags    = flags;

    /* 挂入 event wait_list */
    handle->next     = event->wait_list;
    event->wait_list = handle;

    /* 有超时则加入 sleep_list */
    if (timeout_ms != XCORO_WAIT_FOREVER)
    {
        handle->wakeup_tick_ms = xtime_get_tick_ms() + timeout_ms;
        _sleep_list_insert(handle);
    }
}

void xcoro_set_event(xcoro_event_t *event, uint32_t bits)
{
    event->flags |= bits;

    xcoro_handle_t *handle = event->wait_list;
    event->wait_list       = NULL;

    xcoro_handle_t *remain_list = NULL;

    while (handle)
    {
        xcoro_handle_t *next = handle->next;
        handle->next         = NULL;

        uint32_t matched = event->flags & handle->wait_mask;

        if ((handle->wait_flags & XCORO_FLAGS_WAIT_ALL)
                ? (matched == handle->wait_mask)
                : (matched != 0))
        {
            /* Auto-clear（默认） */
            if ((handle->wait_flags & XCORO_FLAGS_WAIT_NO_CLEAR) == 0)
            {
                event->flags &= (handle->wait_flags & XCORO_FLAGS_WAIT_ALL)
                                    ? (~handle->wait_mask)
                                    : (~matched);
            }

            if (handle->wakeup_tick_ms)
            {
                handle->wakeup_tick_ms = 0;
                _sleep_list_find_remove(handle);
            }

            handle->waiting_event = NULL;
            handle->wait_result   = matched;

            handle->state = XCORO_STATE_READY;
            _ready_list_insert(handle);
        }
        else
        {
            /* 未触发 → 放回等待链表 */
            handle->next = remain_list;
            remain_list  = handle;
        }

        handle = next;
    }

    /* 重新挂回未触发的 waiters */
    event->wait_list = remain_list;
}

void xcoro_yield(xcoro_handle_t *handle)
{
    xassert_not_null(handle);

    handle->state = XCORO_STATE_READY;
    _ready_list_insert(handle);
}

void xcoro_schedule(xcoro_handle_t *handle)
{
    xassert_not_null(handle);
    xassert_not_null(handle->mgr);

    if (handle->wakeup_tick_ms)
    {
        handle->wakeup_tick_ms = 0;
        _sleep_list_find_remove(handle);
    }

    if (handle->waiting_event)
    {
        _event_wait_list_find_remove(handle);
        handle->waiting_event = NULL;
        handle->wait_result   = (uint32_t)XCORO_WAIT_CANCELED;
        handle->wait_mask     = 0;
        handle->wait_flags    = 0;
    }

    handle->state = XCORO_STATE_READY;
    _ready_list_insert(handle);
}

void xcoro_finish(xcoro_handle_t *handle)
{
    xassert_not_null(handle);

    if (handle->mgr == NULL)
    {
        return;
    }

    xcoro_handle_t **pp;
    pp = &handle->mgr->ready_list;
    while (*pp)
    {
        if (*pp == handle)
        {
            *pp = handle->next;
            break;
        }
        pp = &(*pp)->next;
    }

    if (handle->wakeup_tick_ms)
    {
        handle->wakeup_tick_ms = 0;
        _sleep_list_find_remove(handle);
    }

    if (handle->waiting_event)
    {
        _event_wait_list_find_remove(handle);
        handle->waiting_event = NULL;
        handle->wait_result   = (uint32_t)XCORO_WAIT_CANCELED;
        handle->wait_mask     = 0;
        handle->wait_flags    = 0;
    }

    handle->state = XCORO_STATE_FINISHED;
}

void xcoro_request_shutdown(xcoro_manager_t *mgr)
{
    mgr->shutdown_req = true;
}

void xcoro_scheduler_run(xcoro_manager_t *mgr)
{
    while (!mgr->shutdown_req)
    {
        /* ------------------------------------------------------------
         * 1. 处理所有已到期的延时协程（sleep_list → ready_list）
         * ------------------------------------------------------------ */
        _wake_expired_sleepers(mgr);

        /* ------------------------------------------------------------
         * 2. 若有 READY 协程，则立即运行调度
         * ------------------------------------------------------------ */
        xcoro_handle_t *handle = _get_next_ready(mgr);
        if (handle)
        {
            if (handle->entry)
            {
                handle->entry(handle);
            }
            continue;
        }

        /* ------------------------------------------------------------
         * 3. 若无 READY 协程 → 准备进入“tickless 低功耗”
         *    计算下一次需要唤醒的时间点（下一协程超时）
         * ------------------------------------------------------------ */
        xhal_tick_t delay = _next_wakeup_delay_ms(mgr);

        if (delay == 0)
        {
            /* --------------------------------------------------------
             * 3.1 没有任何未来的超时事件（sleep_list 为空）
             *     → 协程系统完全事件驱动
             *
             *     行为：
             *       - 不配置定时唤醒
             *       - CPU 可立即进入 WFI/WFE 等待外设/中断事件
             *       - 外部事件触发后重新进入调度循环
             * -------------------------------------------------------- */

            /* 进入低功耗等待（WFI/WFE），直到事件中断唤醒 */
            continue;
        }

        /* ------------------------------------------------------------
         * 3.2 存在未来的唤醒时间点（sleep_list 非空）
         *     → 安排一次定时唤醒以进入下一个协程节点
         *
         *     要做的事：
         *       (1) 配置低功耗定时器，delay 毫秒后触发中断
         *       (2) 执行 WFI/WFE 进入低功耗
         *       (3) 自动在中断后恢复 tick 计数（必要时校准）
         * ------------------------------------------------------------ */

        /* 3.2.1 配置低功耗定时器，使其在 delay ms 后触发唤醒中断 */

        /* 3.2.2 进入 WFI/WFE，CPU 进入低功耗睡眠 */

        /* 3.2.3 中断返回后，若系统使用 tickless，需要在此校准系统 tick */
    }
}
