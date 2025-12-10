#include "xhal_coro.h"
#include "xhal_assert.h"
#include "xhal_log.h"
#include "xhal_malloc.h"
#include "xhal_time.h"

XLOG_TAG("xCoro");

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
            handle->waiting_event = NULL;
            handle->wait_mask     = 0;
            handle->wait_flags    = 0;
            _event_wait_list_find_remove(handle);
        }

        handle->next           = NULL;
        handle->wakeup_tick_ms = 0;

        handle->state = XCORO_STATE_READY;
        _ready_list_insert(handle);
    }
}

xhal_tick_t _next_wakeup_delay_ms(xcoro_manager_t *mgr)
{
    if (!mgr->sleep_list)
        return 0;

    xhal_tick_t now  = xtime_get_tick_ms();
    xhal_tick_t tick = mgr->sleep_list->wakeup_tick_ms;

    if (TIME_AFTER_EQ(now, tick))
        return 1;

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
    handle->entry      = save_entry;
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
        handle->waiting_event = NULL;
        handle->wait_mask     = 0;
        handle->wait_flags    = 0;
        _event_wait_list_find_remove(handle);
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

    handle->state         = XCORO_STATE_WAITING;
    handle->waiting_event = event;
    handle->wait_mask     = mask;
    handle->wait_flags    = flags;

    /* 加入 event wait_list */
    handle->next     = event->wait_list;
    event->wait_list = handle;

    /* 如有超时，则还要加入 sleep_list */
    if (timeout_ms != XCORO_WAIT_FOREVER)
    {
        handle->wakeup_tick_ms = xtime_get_tick_ms() + timeout_ms;
        _sleep_list_insert(handle);
    }
}

void xcoro_set_event(xcoro_event_t *event, uint32_t bits)
{
    event->flag |= bits;

    /* 取出整个 wait_list */
    xcoro_handle_t *handle = event->wait_list;
    event->wait_list       = NULL;

    /* 未满足条件的重新挂回 event->wait_list */
    xcoro_handle_t *remain_list = NULL;

    while (handle)
    {
        xcoro_handle_t *next = handle->next;
        handle->next         = NULL;

        uint8_t wait_all = (handle->wait_flags & XCORO_FLAGS_WAIT_ALL) != 0;

        uint8_t satisfied;

        if (wait_all)
        {
            /* ALL: 所有 mask 位都满足 */
            satisfied =
                ((event->flag & handle->wait_mask) == handle->wait_mask);
        }
        else
        {
            /* ANY: 只要一个位满足即可 */
            satisfied = ((event->flag & handle->wait_mask) != 0);
        }

        if (satisfied)
        {
            /* 如果存在 timeout，则从 sleep_list 移除 */
            if (handle->wakeup_tick_ms)
            {
                handle->wakeup_tick_ms = 0;
                _sleep_list_find_remove(handle);
            }

            /* 默认 auto-clear（除非设置 NO_CLEAR） */
            if ((handle->wait_flags & XCORO_FLAGS_WAIT_NO_CLEAR) == 0)
            {
                event->flag &= ~handle->wait_mask;
            }

            handle->waiting_event = NULL;

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
        handle->waiting_event = NULL;
        handle->wait_mask     = 0;
        handle->wait_flags    = 0;
        _event_wait_list_find_remove(handle);
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
        handle->waiting_event = NULL;
        handle->wait_mask     = 0;
        handle->wait_flags    = 0;
        _event_wait_list_find_remove(handle);
    }

    handle->state = XCORO_STATE_FINISHED;
}

/* 请求 shutdown */
void xcoro_request_shutdown(xcoro_manager_t *mgr)
{
    mgr->shutdown_req = true;
}

/* 主调度循环（tickless） */
void xcoro_scheduler_run(xcoro_manager_t *mgr)
{
    while (!mgr->shutdown_req)
    {
        /* 唤醒延时到期 */
        _wake_expired_sleepers(mgr);

        /* 有 READY 协程直接运行 */
        xcoro_handle_t *handle = _get_next_ready(mgr);
        if (handle)
        {
            if (handle->entry)
            {
                handle->entry(handle);
            }
            continue;
        }

        /* 无 READY → tickless 进入低功耗 */
        xhal_tick_t delay = _next_wakeup_delay_ms(mgr);

        if (delay == 0)
        {
            /* 无限等待事件 */
            // xcoro_enter_lowpower();
            continue;
        }

        // /* 有限等待：sleep 到下一次超时 */
        // xcoro_timer_start_oneshot(delay);
        // xcoro_enter_lowpower();
        // xcoro_timer_stop();
    }
}
