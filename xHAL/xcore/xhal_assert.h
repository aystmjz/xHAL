#ifndef __XHAL_ASSERT_H
#define __XHAL_ASSERT_H

#include "xhal_config.h"
#include "xhal_def.h"

#ifndef XASSERT_ENABLE
#define XASSERT_ENABLE (1)
#endif

#ifndef XASSERT_FULL_PATH_ENABLE
#define XASSERT_FULL_PATH_ENABLE (1)
#endif

#ifndef XASSERT_FUNC_ENABLE
#define XASSERT_FUNC_ENABLE (1)
#endif

#ifndef XASSERT_BACKTRACE_ENABLE
#define XASSERT_BACKTRACE_ENABLE (1)
#endif

#if XASSERT_FULL_PATH_ENABLE != 0
#define XASSERT_FILE XHAL_FILEPATH
#else
#define XASSERT_FILE XHAL_FILENAME
#endif

#if XASSERT_FUNC_ENABLE != 0
#define XASSERT_FUNC XHAL_FUNCNAME
#else
#define XASSERT_FUNC "NULL"
#endif

#if XASSERT_BACKTRACE_ENABLE != 0
#include "xhal_trace.h"
#define XASSERT_GET_SP()      cmb_get_sp()
#define XASSERT_BACKTRACE(sp) cm_backtrace_assert(sp)
#else
#define XASSERT_GET_SP() (NULL)
#define XASSERT_BACKTRACE(sp)
#endif

void _xassert_func(void);
void _xassert(const char *str, uint32_t id, const char *tag, const char *file,
              const char *func, uint32_t line);

#if XASSERT_ENABLE != 0

#define XASSERT_INVALID_ID ((uint32_t)(-1))
/**
 * @brief  断言函数
 * @param  test   给定的条件
 */
#define xassert(test)                                              \
    do                                                             \
    {                                                              \
        if (!(test))                                               \
        {                                                          \
            xhal_pointer_t sp = XASSERT_GET_SP();                  \
            _xassert(#test, XASSERT_INVALID_ID, TAG, XASSERT_FILE, \
                     XASSERT_FUNC, XHAL_LINE);                     \
            XASSERT_BACKTRACE(sp);                                 \
            _xassert_func();                                       \
        }                                                          \
    } while (0)

#define xassert_tag(test, tag)                                     \
    do                                                             \
    {                                                              \
        if (!(test))                                               \
        {                                                          \
            xhal_pointer_t sp = XASSERT_GET_SP();                  \
            _xassert(#test, XASSERT_INVALID_ID, tag, XASSERT_FILE, \
                     XASSERT_FUNC, XHAL_LINE);                     \
            XASSERT_BACKTRACE(sp);                                 \
            _xassert_func();                                       \
        }                                                          \
    } while (0)

/**
 * @brief  带名称的断言函数
 * @param  test   给定的条件
 * @param  name   给定的名称
 */
#define xassert_name(test, name)                                  \
    do                                                            \
    {                                                             \
        if (!(test))                                              \
        {                                                         \
            xhal_pointer_t sp = XASSERT_GET_SP();                 \
            _xassert(name, XASSERT_INVALID_ID, TAG, XASSERT_FILE, \
                     XASSERT_FUNC, XHAL_LINE);                    \
            XASSERT_BACKTRACE(sp);                                \
            _xassert_func();                                      \
        }                                                         \
    } while (0)

/**
 * @brief  带ID的断言函数
 * @param  test   给定的条件
 * @param  id     给定的ID
 */
#define xassert_id(test, id)                                               \
    do                                                                     \
    {                                                                      \
        if (!(test))                                                       \
        {                                                                  \
            xhal_pointer_t sp = XASSERT_GET_SP();                          \
            _xassert(#test, (uint32_t)id, TAG, XASSERT_FILE, XASSERT_FUNC, \
                     XHAL_LINE);                                           \
            XASSERT_BACKTRACE(sp);                                         \
            _xassert_func();                                               \
        }                                                                  \
    } while (0)

/**
 * @brief  断言函数，用于检查给定指针不为NULL
 * @param  ptr    给定的数据指针
 */
#define xassert_not_null(ptr) xassert_name((ptr != NULL), #ptr " is NULL")

#define xassert_ptr_struct_not_null(pstruct, name)           \
    do                                                       \
    {                                                        \
        xassert_not_null(pstruct);                           \
        void **p      = (void **)(pstruct);                  \
        xhal_size_t n = sizeof(*(pstruct)) / sizeof(void *); \
        for (xhal_size_t i = 0; i < n; i++)                  \
        {                                                    \
            xassert_name(p[i] != NULL, name);                \
        }                                                    \
    } while (0)

#else

#define xassert(test)
#define xassert_tag(test, tag)
#define xassert_id(test, id)
#define xassert_name(test, name)
#define xassert_not_null(ptr)
#define xassert_ptr_struct_not_null(pstruct, name)

#endif /* XASSERT_ENABLE != 0 */

#endif /* __XHAL_ASSERT_H */
