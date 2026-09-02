/**
 ******************************************************************************
 * @file    xhal_coro.h
 * @author  aystmjz
 * @brief   协程模块头文件，提供轻量级协作式调度、延时及事件等待接口
 * @version 2.3.0
 * @date    2026-08-23
 ******************************************************************************
 * Copyright (c) 2026 aystmjz. All rights reserved.
 * SPDX-License-Identifier: MIT
 ******************************************************************************
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __XHAL_XCORO_H
#define __XHAL_XCORO_H

/* Includes ------------------------------------------------------------------*/
#include "xhal_def.h"
#include "xhal_time.h"

/** @addtogroup XHAL
 * @{
 */

/** @defgroup XCORE
 * @{
 */

/** @defgroup XHAL_CORO
 * @brief  协程模块，提供轻量级协作式调度、延时及事件等待接口
 * @{
 */

/* Exported constants --------------------------------------------------------*/
/**
 * @brief  事件等待超时时间：无限等待
 */
#define XCORO_WAIT_FOREVER 0xFFFFFFFFU

/* 事件等待标志 --------------------------------------------------------------*/
#define XCORO_FLAGS_WAIT_ANY      0x00000000U /*!< 任一匹配位满足即唤醒 */
#define XCORO_FLAGS_WAIT_ALL      0x00000001U /*!< 全部匹配位满足才唤醒 */
#define XCORO_FLAGS_WAIT_NO_CLEAR 0x00000002U /*!< 唤醒后不清除事件位 */

/**
 * @brief  协程 PC 保存的最大嵌套层级
 */
#define XCORO_PC_MAX_LEVEL (4)

/* Exported types ------------------------------------------------------------*/
/**
 * 协程状态机状态转换图：
 *
 * ┌-----------------------------┐
 * │                             │
 * │      SLEEPING--------┐      │
 * │       ↑   │          │      │
 * │       │   ↓          ↓      │
 * └----→  READY  ---→ FINISHED -┘
 *         ↑   │          ↑
 *         │   ↓          │
 *        WAITING --------┘
 */

/**
 * @brief  协程状态定义
 */
typedef enum
{
    XCORO_STATE_READY = 0, /*!< 可运行 */
    XCORO_STATE_SLEEPING,  /*!< 延时等待 */
    XCORO_STATE_WAITING,   /*!< 等待事件 */
    XCORO_STATE_FINISHED   /*!< 已结束 */
} xcoro_state_t;

/**
 * @brief  事件等待结果特殊值
 */
typedef enum
{
    XCORO_WAIT_TIMEOUT  = -1, /*!< 等待超时 */
    XCORO_WAIT_CANCELED = -2  /*!< 等待被取消 */
} xcoro_wait_result_t;

/**
 * @brief  协程优先级定义，数值越大优先级越高
 */
typedef enum
{
    XCORO_PRIO_IDLE     = 0,  /*!< 空闲优先级 */
    XCORO_PRIO_LOW      = 10, /*!< 低优先级 */
    XCORO_PRIO_NORMAL   = 20, /*!< 普通优先级 */
    XCORO_PRIO_HIGH     = 30, /*!< 高优先级 */
    XCORO_PRIO_REALTIME = 40, /*!< 实时优先级 */

    XCORO_PRIO_MAX = 64 /*!< 优先级上限 */
} xcoro_priority_t;

typedef struct xcoro_handle xcoro_handle_t;   /*!< 协程句柄 */
typedef struct xcoro_manager xcoro_manager_t; /*!< 协程管理器 */
typedef struct xcoro_event xcoro_event_t;     /*!< 协程事件 */

/**
 * @brief  协程入口函数类型
 * @param  handle: 协程句柄指针
 */
typedef void (*xcoro_entry_t)(xcoro_handle_t *handle);

/**
 * @brief  协程句柄结构体
 */
typedef struct xcoro_handle
{
    uint16_t pc[XCORO_PC_MAX_LEVEL]; /*!< 各层级程序计数保存 */
    uint16_t depth;                  /*!< 当前嵌套层级 */
    xcoro_entry_t entry;             /*!< 协程入口函数 */
    xcoro_state_t state;             /*!< 协程状态 */
    xcoro_priority_t prio;           /*!< 协程优先级 */
    xcoro_manager_t *mgr;            /*!< 所属管理器 */
    void *user_data;                 /*!< 用户数据 */

    xhal_tick_t wakeup_tick_ms; /*!< 唤醒时间点 */

    xcoro_event_t *waiting_event; /*!< 等待中的事件 */
    uint32_t wait_result;         /*!< 等待结果 */
    uint32_t wait_mask;           /*!< 等待事件位掩码 */
    uint32_t wait_flags;          /*!< 等待标志 */

    xcoro_handle_t *next; /*!< 链表下一节点 */

    int32_t ret_val; /*!< 子协程返回值 */
} xcoro_handle_t;

/**
 * @brief  事件结构体
 */
typedef struct xcoro_event
{
    char *name;              /*!< 事件名称 */
    uint32_t volatile flags; /*!< 事件标志位 */
    xcoro_handle_t *wait_list; /*!< 等待该事件的协程链表 */
} xcoro_event_t;

/**
 * @brief  协程管理器结构体
 */
typedef struct xcoro_manager
{
    uint32_t count; /*!< 已注册协程数量 */

    xcoro_handle_t *ready_list; /*!< 就绪链表 */
    xcoro_handle_t *sleep_list; /*!< 延时链表 */

    bool shutdown_req; /*!< 关闭请求标志 */
} xcoro_manager_t;

/* Private macros ------------------------------------------------------------*/
#define _XCORO_PC_GET(handle)        ((handle)->pc[(handle)->depth])
#define _XCORO_PC_SET(handle, value) ((handle)->pc[(handle)->depth] = (value))
#define _XCORO_PC_CLEAR(handle)      ((handle)->pc[(handle)->depth] = 0)

/* Exported macros -----------------------------------------------------------*/
/**
 * @brief  协程函数体开始，初始化状态机并跳转到上次保存位置
 * @param  handle: 协程句柄指针
 */
#define XCORO_BEGIN(handle)                        \
    do                                             \
    {                                              \
        if (handle->state == XCORO_STATE_FINISHED) \
            return;                                \
        switch (_XCORO_PC_GET(handle))             \
        {                                          \
        case 0:

/**
 * @brief  协程函数体结束，标记协程为已完成
 * @param  handle: 协程句柄指针
 */
#define XCORO_END(handle)                 \
    }                                     \
    _XCORO_PC_CLEAR(handle);              \
    handle->state = XCORO_STATE_FINISHED; \
    return;                               \
    }                                     \
    while (0)

/**
 * @brief  让出 CPU，下次从该位置继续执行
 * @param  handle: 协程句柄指针
 */
#define XCORO_YIELD(handle)              \
    do                                   \
    {                                    \
        _XCORO_PC_SET(handle, __LINE__); \
        xcoro_yield(handle);             \
        return;                          \
    case __LINE__:;                      \
    } while (0)

/**
 * @brief  延时指定毫秒后继续执行
 * @param  handle:   协程句柄指针
 * @param  delay_ms: 延时毫秒数
 */
#define XCORO_DELAY_MS(handle, delay_ms) \
    do                                   \
    {                                    \
        xcoro_sleep(handle, delay_ms);   \
        _XCORO_PC_SET(handle, __LINE__); \
        return;                          \
    case __LINE__:;                      \
    } while (0)

/**
 * @brief  延时到指定 tick 时间点后继续执行
 * @param  handle:  协程句柄指针
 * @param  tick_ms: 唤醒时间点(系统 tick)
 */
#define XCORO_DELAY_UNTIL(handle, tick_ms)  \
    do                                      \
    {                                       \
        xcoro_sleep_until(handle, tick_ms); \
        _XCORO_PC_SET(handle, __LINE__);    \
        return;                             \
    case __LINE__:;                         \
    } while (0)

/**
 * @brief  等待事件，满足条件或超时后继续执行
 * @param  handle:     协程句柄指针
 * @param  event:      事件对象指针
 * @param  mask:       事件位掩码
 * @param  flags:      等待标志(XCORO_FLAGS_WAIT_xxx)
 * @param  timeout_ms: 超时时间，XCORO_WAIT_FOREVER 表示无限等待
 */
#define XCORO_WAIT_EVENT(handle, event, mask, flags, timeout_ms)  \
    do                                                            \
    {                                                             \
        xcoro_wait_event(handle, event, mask, flags, timeout_ms); \
        _XCORO_PC_SET(handle, __LINE__);                          \
        return;                                                   \
    case __LINE__:;                                               \
    } while (0)

/**
 * @brief  设置事件位，唤醒匹配的等待协程
 * @param  event: 事件对象指针
 * @param  bits:  要设置的事件位
 */
#define XCORO_SET_EVENT(event, bits)  \
    do                                \
    {                                 \
        xcoro_set_event(event, bits); \
    } while (0)

/**
 * @brief  清除事件位
 * @param  event: 事件对象指针
 * @param  bits:  要清除的事件位
 */
#define XCORO_CLEAR_EVENT(event, bits)  \
    do                                  \
    {                                   \
        xcoro_clear_event(event, bits); \
    } while (0)

/**
 * @brief  调用子协程函数，子协程未结束时挂起当前协程
 * @param  handle: 协程句柄指针
 * @param  func:   子协程函数
 * @param  ...:    子协程附加参数
 */
#define XCORO_CALL(handle, func, ...)                \
    do                                               \
    {                                                \
        _XCORO_PC_SET(handle, __LINE__);             \
    case __LINE__:;                                  \
        (handle)->depth++;                           \
        func(handle, ##__VA_ARGS__);                 \
        (handle)->depth--;                           \
        if ((handle)->state != XCORO_STATE_FINISHED) \
        {                                            \
            return;                                  \
        }                                            \
        (handle)->state = XCORO_STATE_READY;         \
    } while (0)

/**
 * @brief  打印当前协程句柄信息
 * @param  handle: 协程句柄指针
 */
#define XCORO_DUMP_SELF(handle)    \
    do                             \
    {                              \
        xcoro_dump_handle(handle); \
    } while (0)

/**
 * @brief  全部事件位
 */
#define XCORO_EVENT_BITS_ALL (~(uint32_t)0U)

/**
 * @brief  获取用户数据并转换为指定类型指针
 */
#define XCORO_USER_DATA(handle, type) ((type *)((handle)->user_data))
#define XCORO_SET_USER_DATA(handle, data) ((handle)->user_data = (void *)(data))
#define XCORO_CLEAR_USER_DATA(handle)     ((handle)->user_data = NULL)

/**
 * @brief  设置子协程返回值
 */
#define XCORO_SET_RET(handle, value) ((handle)->ret_val = (int32_t)(value))
#define XCORO_RET(handle, type)      ((type)((handle)->ret_val))

/**
 * @brief  获取事件等待结果
 */
#define XCORO_WAIT_RESULT(handle) ((handle)->wait_result)

/**
 * @brief  判断等待是否超时
 */
#define XCORO_WAIT_TIMEOUT(handle) \
    (XCORO_WAIT_RESULT(handle) == (uint32_t)XCORO_WAIT_TIMEOUT)

/**
 * @brief  判断等待是否被取消
 */
#define XCORO_WAIT_CANCELED(handle) \
    (XCORO_WAIT_RESULT(handle) == (uint32_t)XCORO_WAIT_CANCELED)

/**
 * @brief  判断事件是否已设置(等待成功且对应位匹配)
 */
#define XCORO_WAIT_EVENT_SET(handle, event)                         \
    (!XCORO_WAIT_TIMEOUT(handle) && !XCORO_WAIT_CANCELED(handle) && \
     ((XCORO_WAIT_RESULT(handle) & (event)) != 0U))

/**
 * @brief  查询管理器是否已请求关闭
 */
#define XCORO_SHUTDOWN_REQ(handle) ((handle)->mgr->shutdown_req)

// #define XCORO_HANDLE(_func, _priority) \
//     static xcoro_handle_t xcoro_##_func##_handle = {.prio = _priority}

/* Exported functions --------------------------------------------------------*/
void _wake_expired_sleepers(xcoro_manager_t *mgr);
xhal_tick_t _next_wakeup_delay_ms(xcoro_manager_t *mgr);
xcoro_handle_t *_get_next_ready(xcoro_manager_t *mgr);

xhal_err_t xcoro_event_init(xcoro_event_t *event);
xhal_err_t xcoro_event_add(xcoro_event_t *event);
xhal_err_t xcoro_event_remove(xcoro_event_t *event);
xcoro_event_t *xcoro_event_find(const char *name);
bool xcoro_event_valid(const char *name);
bool xcoro_event_of_name(xcoro_event_t *event, const char *name);

xhal_err_t xcoro_manager_init(xcoro_manager_t *mgr);
xhal_err_t xcoro_handle_init(xcoro_handle_t *handle, xcoro_entry_t entry,
                             xcoro_priority_t prio, void *user_data);
xhal_err_t xcoro_register(xcoro_manager_t *mgr, xcoro_handle_t *handle);
xhal_err_t xcoro_unregister(xcoro_handle_t *handle);
void xcoro_request_shutdown(xcoro_manager_t *mgr);

void xcoro_wait_event(xcoro_handle_t *handle, xcoro_event_t *event,
                      uint32_t mask, uint32_t flags, uint32_t timeout_ms);
void xcoro_set_event(xcoro_event_t *event, uint32_t bits);
uint32_t xcoro_clear_event(xcoro_event_t *event, uint32_t bits);
void xcoro_sleep(xcoro_handle_t *handle, xhal_tick_t delay_ms);
void xcoro_sleep_until(xcoro_handle_t *handle, xhal_tick_t tick_ms);
void xcoro_yield(xcoro_handle_t *handle);

bool xcoro_is_running(xcoro_handle_t *handle);
xhal_err_t xcoro_schedule(xcoro_handle_t *handle);
xhal_err_t xcoro_finish(xcoro_handle_t *handle);

void xcoro_dump_handle(const xcoro_handle_t *handle);
void xcoro_dump_event(const xcoro_event_t *evt);
void xcoro_dump_all(const xcoro_manager_t *mgr);

void xcoro_cpu_stat_init(void);
void xcoro_cpu_stat_on_run(void);
void xcoro_cpu_stat_on_idle(void);
uint16_t xcoro_cpu_usage_get(void);

void xcoro_scheduler_run(xcoro_manager_t *mgr);

/** @} */

/** @} */

/** @} */

#endif /* __XHAL_XCORO_H */
