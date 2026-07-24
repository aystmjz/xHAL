#ifndef __XHAL_XCORO_H
#define __XHAL_XCORO_H

#include "xhal_def.h"
#include "xhal_time.h"

#define XCORO_WAIT_FOREVER        0xFFFFFFFFU

#define XCORO_FLAGS_WAIT_ANY      0x00000000U
#define XCORO_FLAGS_WAIT_ALL      0x00000001U
#define XCORO_FLAGS_WAIT_NO_CLEAR 0x00000002U

#define XCORO_PC_MAX_LEVEL        (4)

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

typedef enum
{
    XCORO_WAIT_TIMEOUT  = -1,
    XCORO_WAIT_CANCELED = -2
} xcoro_wait_result_t;

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
    uint16_t pc[XCORO_PC_MAX_LEVEL];
    uint16_t depth;
    xcoro_entry_t entry;
    xcoro_state_t state;
    xcoro_priority_t prio;
    xcoro_manager_t *mgr;
    void *user_data;

    xhal_tick_t wakeup_tick_ms;

    xcoro_event_t *waiting_event;
    uint32_t wait_result;
    uint32_t wait_mask;
    uint32_t wait_flags;

    xcoro_handle_t *next;

    int32_t ret_val;
} xcoro_handle_t;

/* 事件结构 */
typedef struct xcoro_event
{
    char *name;
    uint32_t volatile flags;
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

#define XCORO_PC_GET(handle)        ((handle)->pc[(handle)->depth])
#define XCORO_PC_SET(handle, value) ((handle)->pc[(handle)->depth] = (value))
#define XCORO_PC_CLEAR(handle)      ((handle)->pc[(handle)->depth] = 0)

#define XCORO_BEGIN(handle)                        \
    do                                             \
    {                                              \
        if (handle->state == XCORO_STATE_FINISHED) \
            return;                                \
        switch (XCORO_PC_GET(handle))              \
        {                                          \
        case 0:

#define XCORO_END(handle)                 \
    }                                     \
    XCORO_PC_CLEAR(handle);               \
    handle->state = XCORO_STATE_FINISHED; \
    return;                               \
    }                                     \
    while (0)

#define XCORO_YIELD(handle)         \
    do                              \
    {                               \
        handle->pc = __LINE__;      \
        xcoro_yield(handle) return; \
    case __LINE__:;                 \
    } while (0)

#define XCORO_DELAY_MS(handle, delay_ms) \
    do                                   \
    {                                    \
        xcoro_sleep(handle, delay_ms);   \
        XCORO_PC_SET(handle, __LINE__);  \
        return;                          \
    case __LINE__:;                      \
    } while (0)

#define XCORO_DELAY_UNTIL(handle, tick_ms)  \
    do                                      \
    {                                       \
        xcoro_sleep_until(handle, tick_ms); \
        XCORO_PC_SET(handle, __LINE__);     \
        return;                             \
    case __LINE__:;                         \
    } while (0)

#define XCORO_WAIT_EVENT(handle, event, mask, flags, timeout_ms)  \
    do                                                            \
    {                                                             \
        xcoro_wait_event(handle, event, mask, flags, timeout_ms); \
        XCORO_PC_SET(handle, __LINE__);                           \
        return;                                                   \
    case __LINE__:;                                               \
    } while (0)

#define XCORO_SET_EVENT(event, bits)  \
    do                                \
    {                                 \
        xcoro_set_event(event, bits); \
    } while (0)

#define XCORO_CLEAR_EVENT(event, bits)  \
    do                                  \
    {                                   \
        xcoro_clear_event(event, bits); \
    } while (0)

#define XCORO_CALL(handle, func, ...)                \
    do                                               \
    {                                                \
        XCORO_PC_SET(handle, __LINE__);              \
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

#define XCORO_DUMP_SELF(handle)    \
    do                             \
    {                              \
        xcoro_dump_handle(handle); \
    } while (0)

#define XCORO_EVENT_BITS_ALL              (~(uint32_t)0U)

#define XCORO_USER_DATA(handle, type)     ((type *)((handle)->user_data))
#define XCORO_SET_USER_DATA(handle, data) ((handle)->user_data = (void *)(data))
#define XCORO_CLEAR_USER_DATA(handle)     ((handle)->user_data = NULL)

#define XCORO_SET_RET(handle, value)      ((handle)->ret_val = (int32_t)(value))
#define XCORO_RET(handle, type)           ((type)((handle)->ret_val))

#define XCORO_WAIT_RESULT(handle)         ((handle)->wait_result)

#define XCORO_WAIT_TIMEOUT(handle) \
    (XCORO_WAIT_RESULT(handle) == (uint32_t)XCORO_WAIT_TIMEOUT)

#define XCORO_WAIT_CANCELED(handle) \
    (XCORO_WAIT_RESULT(handle) == (uint32_t)XCORO_WAIT_CANCELED)

#define XCORO_WAIT_EVENT_SET(handle, event)                         \
    (!XCORO_WAIT_TIMEOUT(handle) && !XCORO_WAIT_CANCELED(handle) && \
     ((XCORO_WAIT_RESULT(handle) & (event)) != 0U))

void _wake_expired_sleepers(xcoro_manager_t *mgr);
xhal_tick_t _next_wakeup_delay_ms(xcoro_manager_t *mgr);
xcoro_handle_t *_get_next_ready(xcoro_manager_t *mgr);

xhal_err_t xcoro_event_init(xcoro_event_t *event);
xhal_err_t xcoro_event_add(xcoro_event_t *event);
xhal_err_t xcoro_event_remove(xcoro_event_t *event);
xcoro_event_t *xcoro_event_find(const char *name);
bool xcoro_event_valid(const char *name);
bool xcoro_event_of_name(xcoro_event_t *event, const char *name);

void xcoro_manager_init(xcoro_manager_t *mgr);
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
void xcoro_schedule(xcoro_handle_t *handle);
void xcoro_finish(xcoro_handle_t *handle);

void xcoro_dump_handle(const xcoro_handle_t *handle);
void xcoro_dump_event(const xcoro_event_t *evt);
void xcoro_dump_all(const xcoro_manager_t *mgr);

void xcoro_cpu_stat_init(void);
void xcoro_cpu_stat_on_run(void);
void xcoro_cpu_stat_on_idle(void);
uint16_t xcoro_cpu_usage_get(void);

void xcoro_scheduler_run(xcoro_manager_t *mgr);

#endif /* __XHAL_XCORO_H */
