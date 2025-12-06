#ifndef __XHAL_EXPORT_H
#define __XHAL_EXPORT_H

#include "../xos/xhal_os.h"
#include "xhal_config.h"
#include "xhal_def.h"
#include "xhal_std.h"

#define EXPORT_ID_INIT (0xabababab)
#define EXPORT_ID_EXIT (0xcdcdcdcd)
#define EXPORT_ID_POLL (0xefefefef)

typedef enum xport_level
{
    EXPORT_LEVEL_NULL    = -3,
    EXPORT_LEVEL_TEST    = -2,
    EXPORT_LEVEL_POLL    = -1,
    
    EXPORT_LEVEL_DEBUG   = 0,
    EXPORT_LEVEL_CORE    = 1,
    EXPORT_LEVEL_PERIPH  = 2,
    EXPORT_LEVEL_DRIVER  = 3,
    EXPORT_LEVEL_MIDWARE = 4,
    EXPORT_LEVEL_APP     = 5,
    EXPORT_LEVEL_USER    = 6,

    EXPORT_LEVEL_MAX
} xport_level_t;

/* 轮询导出数据结构 */
typedef struct xhal_export_poll_data
{
    uint64_t timeout_ms;
} xhal_export_poll_data_t;

/* 导出项结构体 */
typedef struct xhal_export
{
    uint32_t magic_head; /* 头部魔数 */
    const char *name;    /* 导出函数名称 */
    void *func;          /* 导出函数 */
    void *data;          /* 导出函数数据 */
    uint16_t type;        /* 导出类型（保留字段） */
    int16_t level;       /* 导出级别 */
    uint32_t period_ms;  /* 轮询周期 */
#ifdef XHAL_OS_SUPPORTING
    osPriority_t priority;
    uint32_t stack_size;
#endif
    uint32_t magic_tail; /* 尾部魔数 */
} xhal_export_t;

extern bool xhal_shutdown_req;
extern osEventFlagsId_t xhal_poll_exit_event;

void xhal_run(void);
void xhal_exit(void);

/*
 * @brief  初始化函数导出宏
 * @param  _func   初始化函数
 * @param  _level  导出级别，范围[0, 127]
 * @retval 无
 */
#define INIT_EXPORT(_func, _level)                           \
    XHAL_USED const xhal_export_t init_##_func XHAL_SECTION( \
        ".xhal_init_export") = {                             \
        .name       = #_func,                                \
        .func       = (void *)&_func,                        \
        .level      = (int16_t)(_level),                     \
        .magic_head = EXPORT_ID_INIT,                        \
        .magic_tail = EXPORT_ID_INIT,                        \
    }

/*
 * @brief  退出函数导出宏
 * @param  _func   轮询函数
 * @param  _level  导出级别，范围[0, 127]
 * @retval 无
 */
#define EXIT_EXPORT(_func, _level)                           \
    XHAL_USED const xhal_export_t exit_##_func XHAL_SECTION( \
        ".xhal_exit_export") = {                             \
        .name       = #_func,                                \
        .func       = (void *)&_func,                        \
        .level      = (int16_t)(_level),                     \
        .magic_head = EXPORT_ID_EXIT,                        \
        .magic_tail = EXPORT_ID_EXIT,                        \
    }

/*
 * @brief  单元测试函数导出宏
 * @param  _func   单元测试函数
 * @retval 无
 */
#ifdef XHAL_UNIT_TEST
#define UNIT_TEST_EXPORT(_func) INIT_EXPORT(_func, EXPORT_LEVEL_TEST)
#else
#define UNIT_TEST_EXPORT(_func)
#endif

/*
 * @brief  轮询函数导出宏
 * @param  _func       轮询函数
 * @param  _period_ms  轮询周期，单位毫秒
 * @retval 无
 */
#define POLL_EXPORT(_func, _period_ms)                       \
    static xhal_export_poll_data_t poll_##_func##_data = {   \
        .timeout_ms = 0,                                     \
    };                                                       \
    XHAL_USED const xhal_export_t poll_##_func XHAL_SECTION( \
        ".xhal_poll_export") = {                             \
        .name       = #_func,                                \
        .func       = (void *)&_func,                        \
        .data       = (void *)&poll_##_func##_data,          \
        .level      = (int16_t)(EXPORT_LEVEL_POLL),          \
        .period_ms  = (uint32_t)(_period_ms),                \
        .magic_head = EXPORT_ID_POLL,                        \
        .magic_tail = EXPORT_ID_POLL,                        \
    }

#ifdef XHAL_OS_SUPPORTING
#define POLL_EXPORT_OS(_func, _period_ms, _priority, _stack_size) \
    static xhal_export_poll_data_t poll_##_func##_data = {        \
        .timeout_ms = 0,                                          \
    };                                                            \
    XHAL_USED const xhal_export_t poll_##_func XHAL_SECTION(      \
        ".xhal_poll_export") = {                                  \
        .name       = #_func,                                     \
        .func       = (void *)&_func,                             \
        .data       = (void *)&poll_##_func##_data,               \
        .level      = (int16_t)(EXPORT_LEVEL_POLL),               \
        .period_ms  = (uint32_t)(_period_ms),                     \
        .priority   = (osPriority_t)(_priority),                  \
        .stack_size = (uint32_t)(_stack_size),                    \
        .magic_head = EXPORT_ID_POLL,                             \
        .magic_tail = EXPORT_ID_POLL,                             \
    }
#else
#define POLL_EXPORT_OS(_func, _period_ms, _priority, _stack_size) \
    POLL_EXPORT(_func, _period_ms)
#endif /* XHAL_OS_SUPPORTING */

#endif /* __XHAL_EXPORT_H */
