#ifndef __XHAL_XCORO_H
#define __XHAL_XCORO_H

#include "xhal_def.h"
#include "xhal_time.h"

#define XCORO_WAIT_FOREVER        0xFFFFFFFFU

#define XCORO_FLAGS_WAIT_ANY      0x00000000U
#define XCORO_FLAGS_WAIT_ALL      0x00000001U
#define XCORO_FLAGS_WAIT_NO_CLEAR 0x00000002U

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

/* 状态定义 */
typedef enum
{
    XCORO_STATE_READY = 0, /* 可运行 */
    XCORO_STATE_SLEEPING,  /* 延时等待*/
    XCORO_STATE_WAITING,   /* 等待事件*/
    XCORO_STATE_FINISHED   /* 已结束 */
} xcoro_state_t;

/* 优先级 */
typedef enum
{
    XCORO_PRIO_IDLE     = 0,
    XCORO_PRIO_LOW      = 10,
    XCORO_PRIO_NORMAL   = 20,
    XCORO_PRIO_HIGH     = 30,
    XCORO_PRIO_REALTIME = 40,

    XCORO_PRIO_MAX = 64
} xcoro_priority_t;

typedef struct xcoro_handle xcoro_handle_t;
typedef struct xcoro_manager xcoro_manager_t;
typedef struct xcoro_event xcoro_event_t;

typedef void (*xcoro_entry_t)(xcoro_handle_t *handle);

/* 协程句柄 */
typedef struct xcoro_handle
{
    uint32_t line;
    xcoro_entry_t entry;
    xcoro_state_t state;
    xcoro_priority_t prio;
    xcoro_manager_t *mgr;
    void *user_data;

    xhal_tick_t wakeup_tick_ms;

    xcoro_event_t *waiting_event;
    uint32_t wait_mask;
    uint32_t wait_flags;

    xcoro_handle_t *next;
} xcoro_handle_t;

/* 事件结构 */
typedef struct xcoro_event
{
    uint32_t volatile flag;
    xcoro_handle_t *wait_list;
} xcoro_event_t;

/* 管理器 */
typedef struct xcoro_manager
{
    uint32_t count;

    xcoro_handle_t *ready_list;
    xcoro_handle_t *sleep_list;

    bool shutdown_req;
} xcoro_manager_t;

#define XCORO_BEGIN(handle)                        \
    do                                             \
    {                                              \
        if (handle->state == XCORO_STATE_FINISHED) \
            return;                                \
        switch (handle->line)                      \
        {                                          \
        case 0:

#define XCORO_END(handle)                 \
    }                                     \
    handle->line  = 0;                    \
    handle->state = XCORO_STATE_FINISHED; \
    return;                               \
    }                                     \
    while (0)

#define XCORO_YIELD(handle)         \
    do                              \
    {                               \
        handle->line = __LINE__;    \
        xcoro_yield(handle) return; \
    case __LINE__:;                 \
    } while (0)

#define XCORO_DELAY_MS(handle, delay_ms) \
    do                                   \
    {                                    \
        xcoro_sleep(handle, delay_ms);   \
        handle->line = __LINE__;         \
        return;                          \
    case __LINE__:;                      \
    } while (0)

#define XCORO_WAIT_EVENT(handle, event, mask, flags, timeout_ms)  \
    do                                                            \
    {                                                             \
        xcoro_wait_event(handle, event, mask, flags, timeout_ms); \
        handle->line = __LINE__;                                  \
        return;                                                   \
    case __LINE__:;                                               \
    } while (0)

#define XCORO_SET_EVENT(event, bits)  \
    do                                \
    {                                 \
        xcoro_set_event(event, bits); \
    } while (0)

#define XCORO_USER_DATA(handle, type) ((type *)((handle)->user_data))

void _wake_expired_sleepers(xcoro_manager_t *mgr);
xhal_tick_t _next_wakeup_delay_ms(xcoro_manager_t *mgr);
xcoro_handle_t *_get_next_ready(xcoro_manager_t *mgr);

void xcoro_manager_init(xcoro_manager_t *mgr);
xhal_err_t xcoro_register(xcoro_manager_t *mgr, xcoro_handle_t *handle);
xhal_err_t xcoro_unregister(xcoro_handle_t *handle);
void xcoro_request_shutdown(xcoro_manager_t *mgr);

void xcoro_wait_event(xcoro_handle_t *handle, xcoro_event_t *event,
                      uint32_t mask, uint32_t flags, uint32_t timeout_ms);
void xcoro_set_event(xcoro_event_t *event, uint32_t bits);
void xcoro_sleep(xcoro_handle_t *handle, xhal_tick_t delay_ms);
void xcoro_yield(xcoro_handle_t *handle);

bool xcoro_is_running(xcoro_handle_t *handle);
void xcoro_schedule(xcoro_handle_t *handle);
void xcoro_finish(xcoro_handle_t *handle);

void xcoro_scheduler_run(xcoro_manager_t *mgr);

#endif /* __XHAL_XCORO_H */
