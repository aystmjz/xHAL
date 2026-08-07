#ifndef __XHAL_EXPORT_H
#define __XHAL_EXPORT_H

#include "xhal_coro.h"
#include "xhal_def.h"

#define EXPORT_ID_INIT (0xabababab)
#define EXPORT_ID_EXIT (0xcdcdcdcd)

typedef enum export_level
{
    EXPORT_LEVEL_NULL = -2,
    EXPORT_LEVEL_TEST = -1,

    EXPORT_LEVEL_DEBUG   = 0,
    EXPORT_LEVEL_CORE    = 1,
    EXPORT_LEVEL_PERIPH  = 2,
    EXPORT_LEVEL_DRIVER  = 3,
    EXPORT_LEVEL_MIDWARE = 4,
    EXPORT_LEVEL_APP     = 5,
    EXPORT_LEVEL_USER    = 6,

    EXPORT_LEVEL_MAX
} export_level_t;

typedef xhal_err_t (*export_func_t)(void);

/* 导出项结构体 */
typedef struct xhal_export
{
    uint32_t magic_head; /* 头部魔数 */
    const char *name;    /* 导出函数名称 */
    export_func_t func;  /* 导出函数 */
    int32_t level;       /* 导出级别 */
    uint32_t magic_tail; /* 尾部魔数 */
} xhal_export_t;

#if (XHAL_OS_SUPPORTING == 0)
extern xcoro_manager_t g_coro_manager;
#endif

#if (XHAL_UNIT_TEST == 1)
void xhal_unit_test(void);
#endif

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
        .func       = (export_func_t)(_func),                \
        .level      = (int32_t)(_level),                     \
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
        .func       = (export_func_t)(_func),                \
        .level      = (int32_t)(_level),                     \
        .magic_head = EXPORT_ID_EXIT,                        \
        .magic_tail = EXPORT_ID_EXIT,                        \
    }

/*
 * @brief  单元测试函数导出宏
 * @param  _func   单元测试函数
 * @retval 无
 */
#if (XHAL_UNIT_TEST == 1)
    #define UNIT_TEST_EXPORT(_func) INIT_EXPORT(_func, EXPORT_LEVEL_TEST)
#else
    #define UNIT_TEST_EXPORT(_func)
#endif

#endif /* __XHAL_EXPORT_H */
