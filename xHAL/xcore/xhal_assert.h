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

#if XASSERT_FULL_PATH_ENABLE != 0
#define __XASSERT_FILE__ __XHAL_FILEPATH__
#else
#define __XASSERT_FILE__ __XHAL_FILENAME__
#endif

#define XASSERT_INVALID_ID (-1)

void _assert(const char *str, uint32_t id, const char *tag, const char *file,
             uint32_t line);

#if XASSERT_ENABLE != 0

/**
 * @brief  断言函数
 * @param  test   给定的条件
 */
#define xassert(test)                                         \
    do                                                        \
    {                                                         \
        if (!(test))                                          \
            _assert(#test, (uint32_t)XASSERT_INVALID_ID, TAG, \
                    __XASSERT_FILE__, __XHAL_LINE__);         \
    } while (0)

#define xassert_tag(test, tag)                                \
    do                                                        \
    {                                                         \
        if (!(test))                                          \
            _assert(#test, (uint32_t)XASSERT_INVALID_ID, tag, \
                    __XASSERT_FILE__, __XHAL_LINE__);         \
    } while (0)

/**
 * @brief  带名称的断言函数
 * @param  test   给定的条件
 * @param  name   给定的名称
 */
#define xassert_name(test, name)                                               \
    do                                                                         \
    {                                                                          \
        if (!(test))                                                           \
            _assert(name, (uint32_t)XASSERT_INVALID_ID, TAG, __XASSERT_FILE__, \
                    __XHAL_LINE__);                                            \
    } while (0)

/**
 * @brief  带ID的断言函数
 * @param  test   给定的条件
 * @param  id     给定的ID
 */
#define xassert_id(test, id)                                    \
    do                                                          \
    {                                                           \
        if (!(test))                                            \
            _assert(#test, (uint32_t)id, TAG, __XASSERT_FILE__, \
                    __XHAL_LINE__);                             \
    } while (0)

/**
 * @brief  断言函数，用于检查给定指针不为NULL
 * @param  ptr    给定的数据指针
 */
#define xassert_not_null(ptr) xassert_name((ptr != NULL), #ptr " is NULL")

#define xassert_ptr_struct_not_null(pstruct, name)      \
    do                                                  \
    {                                                   \
        xassert_not_null(pstruct);                      \
        void **p = (void **)(pstruct);                  \
        size_t n = sizeof(*(pstruct)) / sizeof(void *); \
        for (size_t i = 0; i < n; i++)                  \
        {                                               \
            xassert_name(p[i] != NULL, name);           \
        }                                               \
    } while (0)

#else

#define xassert(test)
#define xassert_id(test, id)
#define xassert_name(test, name)
#define xassert_not_null(ptr)
#define xassert_ptr_struct_not_null(pstruct, name)

#endif /* XASSERT_ENABLE != 0 */

#endif /* __XHAL_ASSERT_H */
