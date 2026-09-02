/**
 ******************************************************************************
 * @file    xhal_coro.c
 * @author  aystmjz
 * @brief   协程模块源文件，实现协作式调度、事件机制及 CPU 占用统计
 * @version 2.3.0
 * @date    2026-08-23
 ******************************************************************************
 * Copyright (c) 2026 aystmjz. All rights reserved.
 * SPDX-License-Identifier: MIT
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "xhal_coro.h"
#include "xhal_assert.h"
#include "xhal_log.h"
#include "xhal_malloc.h"
#include "xhal_time.h"
#include <string.h>

XHAL_TAG(xCoro);

/* Private defines -----------------------------------------------------------*/
#ifndef XCORO_EVENT_NUM_MAX
#define XCORO_EVENT_NUM_MAX (64) /* 最大事件数 */
#endif

/* Private variables ---------------------------------------------------------*/
static xhal_tick_t stat_window_start_ms; /* 统计窗口起始时间 */
static xhal_tick_t stat_idle_total_ms;   /* 窗口内累计空闲时间 */

static xhal_tick_t idle_enter_ms; /* 进入空闲状态时间 */
static bool in_idle;              /* 是否处于空闲状态 */

static xcoro_event_t *xcoro_event_table[XCORO_EVENT_NUM_MAX]; /* 事件表 */
static uint16_t xcoro_event_count = 0;                        /* 已注册事件数 */

/* Exported functions --------------------------------------------------------*/
/**
 * @brief  初始化事件对象
 * @param  event: 事件对象指针
 * @retval 错误码
 */
xhal_err_t xcoro_event_init(xcoro_event_t *event)
{
    xassert_not_null(event);

    xmemset(event, 0, sizeof(*event));

    return XHAL_OK;
}

/**
 * @brief  注册事件到事件表
 * @param  event: 事件对象指针
 * @retval 错误码，表满时返回 XHAL_ERR_NO_MEMORY
 */
xhal_err_t xcoro_event_add(xcoro_event_t *event)
{
    xassert_not_null(event);
    xassert_not_null(event->name);

    for (uint16_t i = 0; i < XCORO_EVENT_NUM_MAX; i++)
    {
        if (xcoro_event_table[i] == event)
        {
            return XHAL_OK;
        }
    }

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

/**
 * @brief  从事件表移除事件
 * @param  event: 事件对象指针
 * @retval 错误码，未找到时返回 XHAL_ERROR
 */
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

/**
 * @brief  按名称查找事件
 * @param  name: 事件名称
 * @retval 事件对象指针，未找到返回 NULL 并打印错误日志
 */
xcoro_event_t *xcoro_event_find(const char *name)
{
    xassert_not_null(name);

    xcoro_event_t *event = NULL;
    for (uint16_t i = 0; i < XCORO_EVENT_NUM_MAX; i++)
    {
        if (xcoro_event_table[i] == NULL || xcoro_event_table[i]->name == NULL)
        {
            continue;
        }

        if (strcmp(xcoro_event_table[i]->name, name) == 0)
        {
            event = xcoro_event_table[i];
            break;
        }
    }

    if (event == NULL)
    {
        XLOG_ERROR("Event %s not found", name);
    }

    return event;
}

/**
 * @brief  校验事件名称是否存在
 * @param  name: 事件名称
 * @retval true 存在，false 不存在
 */
bool xcoro_event_valid(const char *name)
{
    return xcoro_event_find(name) == NULL ? false : true;
}

/**
 * @brief  校验事件对象名称是否匹配
 * @param  event: 事件对象指针
 * @param  name:  事件名称
 * @retval true 匹配，false 不匹配
 */
bool xcoro_event_of_name(xcoro_event_t *event, const char *name)
{
    xassert_not_null(event);
    xassert_not_null(name);

    if (event->name != NULL && strcmp(event->name, name) == 0)
    {
        return true;
    }

    return false;
}

/* Private functions ---------------------------------------------------------*/
/**
 * @brief  按优先级插入就绪链表
 * @note   更高优先级插入最前；同优先级先进先出；
 *         更低优先级插入在第一个更低优先级节点之前
 * @param  handle: 协程句柄
 */
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

/**
 * @brief  按唤醒时间插入睡眠链表
 * @note   更早唤醒插入最前；相同唤醒时间先进先出；
 *         更晚唤醒插入在第一个更晚唤醒节点之前
 * @param  handle: 协程句柄
 */
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

/**
 * @brief  从睡眠链表查找并移除指定协程
 * @param  handle: 协程句柄
 */
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

/**
 * @brief  从事件等待链表查找并移除指定协程
 * @param  handle: 协程句柄
 */
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
/**
 * @brief  唤醒所有已到期的睡眠协程
 * @note   遍历睡眠链表，将唤醒时间已到的协程移入就绪链表；
 *         若协程同时在等待事件则解除等待并置超时结果
 * @param  mgr: 协程管理器指针
 */
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

/**
 * @brief  计算距离下一次唤醒所需的延时
 * @param  mgr: 协程管理器指针
 * @retval 延时毫秒数，无睡眠协程返回 0，已到期返回 1
 */
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

/**
 * @brief  从就绪链表取出下一个待运行协程
 * @param  mgr: 协程管理器指针
 * @retval 协程句柄，链表为空返回 NULL
 */
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

/**
 * @brief  初始化协程管理器
 * @param  mgr: 协程管理器指针
 * @retval 错误码
 */
xhal_err_t xcoro_manager_init(xcoro_manager_t *mgr)
{
    xmemset(mgr, 0, sizeof(*mgr));

    return XHAL_OK;
}

/**
 * @brief  初始化协程句柄
 * @param  entry:     协程入口函数
 * @param  prio:      协程优先级
 * @param  user_data: 用户数据指针
 * @retval 错误码，优先级越界返回 XHAL_ERR_INVALID
 */
xhal_err_t xcoro_handle_init(xcoro_handle_t *handle, xcoro_entry_t entry,
                             xcoro_priority_t prio, void *user_data)
{
    xassert_not_null(handle);
    xassert_not_null(entry);

    if (prio > XCORO_PRIO_MAX)
    {
        return XHAL_ERR_INVALID;
    }

    xmemset(handle, 0, sizeof(*handle));

    handle->entry     = entry;
    handle->prio      = prio;
    handle->user_data = user_data;

    return XHAL_OK;
}

/**
 * @brief  注册协程到管理器
 * @note   保存用户设置的属性后清空句柄并重新挂载，
 *         使协程进入就绪态并插入就绪链表
 * @param  mgr:    协程管理器指针
 * @param  handle: 协程句柄
 * @retval 错误码
 */
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

/**
 * @brief  注销协程
 * @note   先调用 xcoro_finish() 结束协程，再将其从管理器中移除
 * @param  handle: 协程句柄
 * @retval 错误码，未注册时返回 XHAL_ERR_INVALID
 */
xhal_err_t xcoro_unregister(xcoro_handle_t *handle)
{

    xassert_not_null(handle);

    if (handle->mgr == NULL)
    {
        return XHAL_ERR_INVALID;
    }

    xcoro_finish(handle);

    handle->mgr->count--;
    handle->mgr  = NULL;
    handle->next = NULL;

    return XHAL_OK;
}

/**
 * @brief  判断协程是否仍在运行
 * @param  handle: 协程句柄
 * @retval true 运行中，false 已结束或句柄为空
 */
bool xcoro_is_running(xcoro_handle_t *handle)
{
    return handle && (handle->state != XCORO_STATE_FINISHED);
}

/**
 * @brief  协程睡眠指定时长
 * @note   设置唤醒时间并挂入睡眠链表，调度器到期后自动唤醒
 * @param  handle:   协程句柄
 * @param  delay_ms: 睡眠毫秒数
 */
void xcoro_sleep(xcoro_handle_t *handle, xhal_tick_t delay_ms)
{
    xassert_not_null(handle);
    xassert_not_null(handle->mgr);

    handle->wakeup_tick_ms = xtime_get_tick_ms() + delay_ms;
    handle->state          = XCORO_STATE_SLEEPING;

    _sleep_list_insert(handle);
}

/**
 * @brief  协程睡眠到指定时刻
 * @param  handle:  协程句柄
 * @param  tick_ms: 绝对唤醒时刻
 */
void xcoro_sleep_until(xcoro_handle_t *handle, xhal_tick_t tick_ms)
{
    xassert_not_null(handle);
    xassert_not_null(handle->mgr);

    handle->wakeup_tick_ms = tick_ms;
    handle->state          = XCORO_STATE_SLEEPING;

    _sleep_list_insert(handle);
}

/**
 * @brief  协程等待事件位
 * @note   事件已满足条件时立即唤醒；否则挂入事件等待链表，
 *         可按需设置超时(超时后返回 XCORO_WAIT_TIMEOUT)
 * @param  handle:     协程句柄
 * @param  event:      事件对象指针
 * @param  mask:       等待的事件位掩码
 * @param  flags:      等待标志(XCORO_FLAGS_WAIT_ALL/WAIT_NO_CLEAR)
 * @param  timeout_ms: 超时毫秒数，XCORO_WAIT_FOREVER 为永久等待
 */
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

/**
 * @brief  设置事件位并唤醒满足条件的等待协程
 * @note   对满足条件的等待者按标志自动清除事件位或保留，
 *         未触发的等待者重新挂回等待链表
 * @param  event: 事件对象指针
 * @param  bits:  待设置的事件位
 */
void xcoro_set_event(xcoro_event_t *event, uint32_t bits)
{
    xassert_not_null(event);

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

/**
 * @brief  清除事件位
 * @param  event: 事件对象指针
 * @param  bits:  待清除的事件位
 * @retval 清除前的事件标志
 */
uint32_t xcoro_clear_event(xcoro_event_t *event, uint32_t bits)
{
    xassert_not_null(event);

    uint32_t old = event->flags;

    event->flags &= ~bits;

    return old;
}

/**
 * @brief  协程主动让出 CPU
 * @param  handle: 协程句柄
 */
void xcoro_yield(xcoro_handle_t *handle)
{
    xassert_not_null(handle);

    handle->state = XCORO_STATE_READY;
    _ready_list_insert(handle);
}

/**
 * @brief  重新调度已结束的协程
 * @param  handle: 协程句柄
 * @retval 错误码，协程未结束时返回 XHAL_ERR_INVALID
 */
xhal_err_t xcoro_schedule(xcoro_handle_t *handle)
{
    xassert_not_null(handle);
    xassert_not_null(handle->mgr);

    if (handle->state != XCORO_STATE_FINISHED)
    {
        return XHAL_ERR_INVALID;
    }

    handle->state = XCORO_STATE_READY;
    _ready_list_insert(handle);

    return XHAL_OK;
}

/**
 * @brief  结束协程
 * @note   清空程序计数器，从就绪/睡眠/事件等待链表中移除，
 *         置状态为已结束；等待事件时返回 XCORO_WAIT_CANCELED
 * @param  handle: 协程句柄
 * @retval 错误码
 */
xhal_err_t xcoro_finish(xcoro_handle_t *handle)
{
    xassert_not_null(handle);
    xassert_not_null(handle->mgr);

    if (handle->state == XCORO_STATE_FINISHED)
    {
        return XHAL_OK;
    }

    xmemset(handle->pc, 0, sizeof(handle->pc));

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

    return XHAL_OK;
}

/**
 * @brief  请求关闭调度器
 * @param  mgr: 协程管理器指针
 */
void xcoro_request_shutdown(xcoro_manager_t *mgr)
{
    mgr->shutdown_req = true;
}

/**
 * @brief  协程状态转字符串(内部接口)
 * @param  state: 协程状态
 * @retval 状态字符串
 */
static const char *_state_str(xcoro_state_t state)
{
    switch (state)
    {
    case XCORO_STATE_READY:
        return "READY";
    case XCORO_STATE_SLEEPING:
        return "SLEEPING";
    case XCORO_STATE_WAITING:
        return "WAITING";
    case XCORO_STATE_FINISHED:
        return "FINISHED";
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief  协程优先级转字符串(内部接口)
 * @param  prio: 协程优先级
 * @retval 优先级字符串
 */
static const char *_prio_str(xcoro_priority_t prio)
{
    switch (prio)
    {
    case XCORO_PRIO_IDLE:
        return "IDLE";
    case XCORO_PRIO_LOW:
        return "LOW";
    case XCORO_PRIO_NORMAL:
        return "NORMAL";
    case XCORO_PRIO_HIGH:
        return "HIGH";
    case XCORO_PRIO_REALTIME:
        return "REALTIME";
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief  打印单个协程句柄详情
 * @note   输出入口、状态、优先级、程序计数器、用户数据及
 *         睡眠/等待事件相关信息
 * @param  handle: 协程句柄
 */
void xcoro_dump_handle(const xcoro_handle_t *handle)
{
    if (handle == NULL)
    {
        XLOG_PRINTF("xcoro: <null handle>\r\n");
        return;
    }

    XLOG_PRINTF("xcoro @%p\r\n", handle);
    XLOG_PRINTF("  entry       : 0x%p\r\n", handle->entry);
    XLOG_PRINTF("  state       : %s (%d)\r\n", _state_str(handle->state),
                handle->state);
    XLOG_PRINTF("  prio        : %s (%d)\r\n", _prio_str(handle->prio),
                handle->prio);
    XLOG_PRINTF("  depth       : %u\r\n", handle->depth);
    for (uint32_t lvl = 0; lvl < XCORO_PC_MAX_LEVEL; lvl++)
    {
        uint32_t pc_lvl = (handle)->pc[lvl];

        XLOG_PRINTF("  pc[%u] = %u%s\r\n", lvl, pc_lvl,
                    (lvl == handle->depth) ? " <active>" : "");
    }
    XLOG_PRINTF("  user_data   : 0x%p\r\n", handle->user_data);

    if (handle->state == XCORO_STATE_SLEEPING)
    {
        XLOG_PRINTF("  wakeup_tick : %lu\r\n",
                    (unsigned long)handle->wakeup_tick_ms);
    }

    if (handle->state == XCORO_STATE_WAITING)
    {
        XLOG_PRINTF("  waiting_evt : %p (%s)\r\n", handle->waiting_event,
                    handle->waiting_event && handle->waiting_event->name
                        ? handle->waiting_event->name
                        : "noname");
        XLOG_PRINTF("  wait_mask   : 0x%08lx\r\n", handle->wait_mask);
        XLOG_PRINTF("  wait_flags  : 0x%08lx\r\n", handle->wait_flags);
        XLOG_PRINTF("  wait_result : %ld\r\n", (long)handle->wait_result);
    }
}

/**
 * @brief  打印单个事件对象详情
 * @param  evt: 事件对象指针
 */
void xcoro_dump_event(const xcoro_event_t *evt)
{
    if (evt == NULL)
    {
        XLOG_PRINTF("xcoro_event: <null>\r\n");
        return;
    }

    XLOG_PRINTF("xcoro_event @%p\r\n", evt);
    XLOG_PRINTF("  name     : %s\r\n", evt->name ? evt->name : "noname");
    XLOG_PRINTF("  flags    : 0x%08lx\r\n", evt->flags);
    XLOG_PRINTF("  waitlist : %p\r\n", evt->wait_list);
}

/**
 * @brief  打印调度器全部就绪与睡眠协程
 * @param  mgr: 协程管理器指针
 */
void xcoro_dump_all(const xcoro_manager_t *mgr)
{
    if (mgr == NULL)
    {
        XLOG_PRINTF("xcoro_mgr: <null>\r\n");
        return;
    }

    XLOG_PRINTF("========== xcoro manager dump begin ==========\r\n");

    /* READY list */
    XLOG_PRINTF("[READY LIST]\r\n");
    if (mgr->ready_list == NULL)
    {
        XLOG_PRINTF("  <empty>\r\n");
    }
    else
    {
        const xcoro_handle_t *handle = mgr->ready_list;
        while (handle)
        {
            xcoro_dump_handle(handle);
            handle = handle->next;
        }
    }

    /* SLEEP list */
    XLOG_PRINTF("[SLEEP LIST]\r\n");
    if (mgr->sleep_list == NULL)
    {
        XLOG_PRINTF("  <empty>\r\n");
    }
    else
    {
        const xcoro_handle_t *handle = mgr->sleep_list;
        while (handle)
        {
            xcoro_dump_handle(handle);
            handle = handle->next;
        }
    }

    XLOG_PRINTF("=========== xcoro manager dump end ===========\r\n");
}

/**
 * @brief  初始化 CPU 占用率统计
 */
void xcoro_cpu_stat_init(void)
{
    stat_window_start_ms = xtime_get_tick_ms();
    stat_idle_total_ms   = 0;
    idle_enter_ms        = 0;
    in_idle              = false;
}

/**
 * @brief  记录协程开始运行时刻(退出空闲)
 */
void xcoro_cpu_stat_on_run(void)
{
    if (in_idle)
    {
        xhal_tick_t now = xtime_get_tick_ms();
        stat_idle_total_ms += TIME_DIFF(now, idle_enter_ms);
        in_idle = false;
    }
}

/**
 * @brief  记录进入空闲时刻
 */
void xcoro_cpu_stat_on_idle(void)
{
    if (!in_idle)
    {
        idle_enter_ms = xtime_get_tick_ms();
        in_idle       = true;
    }
}

/**
 * @brief  获取并重置 CPU 占用率
 * @note   返回自上次调用以来的 CPU 占用率(0.01% 精度)，
 *         计算后重置统计窗口
 * @retval CPU 占用率，范围 0~10000
 */
uint16_t xcoro_cpu_usage_get(void)
{
    xhal_tick_t now = xtime_get_tick_ms();

    xhal_tick_t idle_ms = stat_idle_total_ms;
    if (in_idle)
    {
        idle_ms += TIME_DIFF(now, idle_enter_ms);
    }

    xhal_tick_t total_ms = TIME_DIFF(now, stat_window_start_ms);
    if (total_ms == 0 || idle_ms >= total_ms)
    {
        return 0;
    }

    uint32_t usage = ((total_ms - idle_ms) * 10000U) / total_ms;

    stat_window_start_ms = now;
    stat_idle_total_ms   = 0;

    if (in_idle)
    {
        idle_enter_ms = now;
    }

    return (uint16_t)usage;
}

/**
 * @brief  运行协程调度器
 * @note   循环处理到期睡眠协程、运行就绪协程；
 *         无就绪协程时进入低功耗等待(tickless)，
 *         直到事件中断或定时唤醒
 * @param  mgr: 协程管理器指针
 */
void xcoro_scheduler_run(xcoro_manager_t *mgr)
{
    xcoro_cpu_stat_init();

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
        if (handle && handle->entry)
        {
            xcoro_cpu_stat_on_run();
            handle->entry(handle);
            continue;
        }
        else
        {
            xcoro_cpu_stat_on_idle();
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

/* ---------------------------------------------------------------------------*/
