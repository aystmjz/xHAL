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
    uint64_t pc : 60;
    uint64_t depth : 4;
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

/* -------------------------------------------------------------
 *  PC/Depth 多级槽位宏（4 层，每层 15 bit，总 60 bit）
 *  depth=0 为最低槽（位 0~14）
 *  depth=3 为最高槽（位 45~59）
 * ------------------------------------------------------------- */

#define XCORO_PC_BITS_PER_LEVEL (15ULL)
#define XCORO_PC_MAX_LEVEL      (4ULL)
#define XCORO_PC_SLOT_MASK      ((1ULL << XCORO_PC_BITS_PER_LEVEL) - 1ULL)

#define XCORO_PC_OFFSET(depth)  ((depth) * XCORO_PC_BITS_PER_LEVEL)

#define XCORO_PC_GET(handle) \
    (((handle)->pc >> XCORO_PC_OFFSET((handle)->depth)) & XCORO_PC_SLOT_MASK)

#define XCORO_PC_SET(handle, value)                                         \
    do                                                                      \
    {                                                                       \
        (handle)->pc =                                                      \
            (((handle)->pc &                                                \
              ~(XCORO_PC_SLOT_MASK << XCORO_PC_OFFSET((handle)->depth)))) | \
            ((((uint64_t)(value)) & XCORO_PC_SLOT_MASK)                     \
             << XCORO_PC_OFFSET((handle)->depth));                          \
    } while (0)

/* 清除指定 depth 槽位（置 0） */
#define XCORO_PC_CLEAR(handle) \
    ((handle)->pc &= ~(XCORO_PC_SLOT_MASK << XCORO_PC_OFFSET((handle)->depth)))

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

#define XCORO_CALL(handle, func)                   \
    do                                             \
    {                                              \
        XCORO_PC_SET(handle, __LINE__);            \
    case __LINE__:;                                \
        (handle)->depth++;                         \
        func(handle);                              \
        (handle)->depth--;                         \
        if (handle->state != XCORO_STATE_FINISHED) \
        {                                          \
            return;                                \
        }                                          \
    } while (0)

#define XCORO_USER_DATA(handle, type) ((type *)((handle)->user_data))

#define XCORO_WAIT_RESULT(handle)     ((handle)->wait_result)

void _wake_expired_sleepers(xcoro_manager_t *mgr);
xhal_tick_t _next_wakeup_delay_ms(xcoro_manager_t *mgr);
xcoro_handle_t *_get_next_ready(xcoro_manager_t *mgr);

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
void xcoro_sleep(xcoro_handle_t *handle, xhal_tick_t delay_ms);
void xcoro_yield(xcoro_handle_t *handle);

bool xcoro_is_running(xcoro_handle_t *handle);
void xcoro_schedule(xcoro_handle_t *handle);
void xcoro_finish(xcoro_handle_t *handle);

void xcoro_scheduler_run(xcoro_manager_t *mgr);

#endif /* __XHAL_XCORO_H */
