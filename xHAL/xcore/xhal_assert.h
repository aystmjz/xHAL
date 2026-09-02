/**
 ******************************************************************************
 * @file    xhal_assert.h
 * @author  aystmjz
 * @brief   断言模块头文件，提供断言宏及断言处理函数声明
 * @version 2.3.0
 * @date    2026-08-23
 ******************************************************************************
 * Copyright (c) 2026 aystmjz. All rights reserved.
 * SPDX-License-Identifier: MIT
 ******************************************************************************
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __XHAL_ASSERT_H
#define __XHAL_ASSERT_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#include "xhal_config.h"
#include "xhal_def.h"

/* Private define ------------------------------------------------------------*/
#ifndef XASSERT_ENABLE
    /**
     * @brief  断言总开关配置
     * @note   置 1 使能断言功能，置 0 时所有断言宏编译为空操作
     */
    #define XASSERT_ENABLE 1
#endif /* XASSERT_ENABLE */

#ifndef XASSERT_FULL_PATH_ENABLE
    /**
     * @brief  断言打印完整文件路径配置
     * @note   置 1 打印文件完整路径，置 0 仅打印文件名
     */
    #define XASSERT_FULL_PATH_ENABLE 1
#endif /* XASSERT_FULL_PATH_ENABLE */

#ifndef XASSERT_FUNC_ENABLE
    /**
     * @brief  断言打印函数名配置
     * @note   置 1 打印所在函数名，置 0 打印 "NULL"
     */
    #define XASSERT_FUNC_ENABLE 1
#endif /* XASSERT_FUNC_ENABLE */

#ifndef XASSERT_BACKTRACE_ENABLE
    /**
     * @brief  断言回溯(backtrace)功能配置
     * @note   置 1 且 XHAL_TRACE 使能时，断言失败后输出调用栈回溯信息
     */
    #define XASSERT_BACKTRACE_ENABLE 1
#endif /* XASSERT_BACKTRACE_ENABLE */

#ifndef XASSERT_USER_HOOK_ENABLE
    /**
     * @brief  断言用户钩子使能配置
     * @note   置 1 时断言失败将先调用用户钩子函数
     */
    #define XASSERT_USER_HOOK_ENABLE 1
#endif /* XASSERT_USER_HOOK_ENABLE */

/**
 * @brief  断言打印的文件名
 */
#if XASSERT_FULL_PATH_ENABLE != 0
    #define XASSERT_FILE XHAL_FILEPATH
#else
    #define XASSERT_FILE XHAL_FILENAME
#endif /* XASSERT_FULL_PATH_ENABLE != 0 */

/**
 * @brief  断言打印的函数名
 */
#if XASSERT_FUNC_ENABLE != 0
    #define XASSERT_FUNC XHAL_FUNCNAME
#else
    #define XASSERT_FUNC "NULL"
#endif /* XASSERT_FUNC_ENABLE != 0 */

/**
 * @brief  无效断言 ID 值，表示未指定断言 ID
 */
#define XASSERT_INVALID_ID ((uint32_t)(-1))

/* Private macro -------------------------------------------------------------*/
#if (XASSERT_BACKTRACE_ENABLE == 1) && XHAL_TRACE
    #include "xhal_trace.h"
    #define XASSERT_GET_SP()      cmb_get_sp()            /*!< 获取当前栈指针 */
    #define XASSERT_BACKTRACE(sp) cm_backtrace_assert(sp) /*!< 断言失败回溯 */
#else
    #define XASSERT_GET_SP()      (NULL)     /*!< 获取当前栈指针 */
    #define XASSERT_BACKTRACE(sp) (void)(sp) /*!< 断言失败回溯 */
#endif /* (XASSERT_BACKTRACE_ENABLE == 1) && XHAL_TRACE */

/* Private function prototypes -----------------------------------------------*/
void _xassert_func(void);
void _xassert(const char *condition, const char *extra, const char *tag,
              const char *file, const char *func, uint32_t line, uint32_t id);

/** @addtogroup XHAL
 * @{
 */

/** @defgroup XCORE
 * @{
 */

/** @defgroup XHAL_ASSERT
 * @brief  断言模块，提供断言宏及断言处理函数
 * @{
 */

/* Exported macros -----------------------------------------------------------*/
#if XASSERT_ENABLE != 0
    /**
     * @brief  断言函数，条件不满足时输出失败信息并进入死循环
     * @param  test: 给定的条件表达式
     */
    #define xassert(test)                                                     \
        do                                                                    \
        {                                                                     \
            if (!(test))                                                      \
            {                                                                 \
                xhal_pointer_t sp = XASSERT_GET_SP();                         \
                _xassert(#test, NULL, __xhal_tag, XASSERT_FILE, XASSERT_FUNC, \
                         XHAL_LINE, XASSERT_INVALID_ID);                      \
                XASSERT_BACKTRACE(sp);                                        \
                _xassert_func();                                              \
            }                                                                 \
        } while (0)

    /**
     * @brief  带模块标签的断言函数
     * @param  test: 给定的条件表达式
     * @param  tag: 模块标签
     */
    #define xassert_tag(test, tag)                                     \
        do                                                             \
        {                                                              \
            if (!(test))                                               \
            {                                                          \
                xhal_pointer_t sp = XASSERT_GET_SP();                  \
                _xassert(#test, NULL, tag, XASSERT_FILE, XASSERT_FUNC, \
                         XHAL_LINE, XASSERT_INVALID_ID);               \
                XASSERT_BACKTRACE(sp);                                 \
                _xassert_func();                                       \
            }                                                          \
        } while (0)

    /**
     * @brief  带附加信息的断言函数
     * @param  test: 给定的条件表达式
     * @param  info: 附加说明信息
     */
    #define xassert_info(test, info)                                          \
        do                                                                    \
        {                                                                     \
            if (!(test))                                                      \
            {                                                                 \
                xhal_pointer_t sp = XASSERT_GET_SP();                         \
                _xassert(#test, info, __xhal_tag, XASSERT_FILE, XASSERT_FUNC, \
                         XHAL_LINE, XASSERT_INVALID_ID);                      \
                XASSERT_BACKTRACE(sp);                                        \
                _xassert_func();                                              \
            }                                                                 \
        } while (0)

    /**
     * @brief  带 ID 的断言函数
     * @param  test: 给定的条件表达式
     * @param  id: 给定的断言 ID
     */
    #define xassert_id(test, id)                                              \
        do                                                                    \
        {                                                                     \
            if (!(test))                                                      \
            {                                                                 \
                xhal_pointer_t sp = XASSERT_GET_SP();                         \
                _xassert(#test, NULL, __xhal_tag, XASSERT_FILE, XASSERT_FUNC, \
                         XHAL_LINE, (uint32_t)id);                            \
                XASSERT_BACKTRACE(sp);                                        \
                _xassert_func();                                              \
            }                                                                 \
        } while (0)

    /**
     * @brief  指针非空断言
     * @param  ptr: 待检查的数据指针
     */
    #define xassert_not_null(ptr) xassert_info((ptr != NULL), #ptr " is NULL")

    /**
     * @brief  结构体成员指针非空断言
     * @param  pstruct: 待检查的结构体指针
     * @param  info: 附加说明信息
     */
    #define xassert_ptr_struct_not_null(pstruct, info)           \
        do                                                       \
        {                                                        \
            xassert_not_null(pstruct);                           \
            void **p      = (void **)(pstruct);                  \
            xhal_size_t n = sizeof(*(pstruct)) / sizeof(void *); \
            for (xhal_size_t i = 0; i < n; i++)                  \
            {                                                    \
                xassert_info(p[i] != NULL, info);                \
            }                                                    \
        } while (0)
#else /* XASSERT_ENABLE == 0 */
    #define xassert(test)            ((void)(test))
    #define xassert_tag(test, tag)   ((void)(test), (void)(tag))
    #define xassert_id(test, id)     ((void)(test), (void)(id))
    #define xassert_info(test, info) ((void)(test), (void)(info))
    #define xassert_not_null(ptr)    ((void)(ptr))
    #define xassert_ptr_struct_not_null(pstruct, info) \
        ((void)(pstruct), (void)(info))
#endif /* XASSERT_ENABLE != 0 */

/** @} */

/** @} */

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __XHAL_ASSERT_H */