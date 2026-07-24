#ifndef __XHAL_DEF_H
#define __XHAL_DEF_H

#include "xhal_std.h"

#define XHAL_WAIT_FOREVER (0xFFFFFFFFU)

#define XHAL_ERR_LIST                               \
    ERR(XHAL_OK, 0, "Success")                      \
    ERR(XHAL_ERROR, -1, "General error")            \
    ERR(XHAL_ERR_EMPTY, -2, "Empty")                \
    ERR(XHAL_ERR_FULL, -3, "Full")                  \
    ERR(XHAL_ERR_TIMEOUT, -4, "Timeout")            \
    ERR(XHAL_ERR_BUSY, -5, "Busy")                  \
    ERR(XHAL_ERR_NO_MEMORY, -6, "No memory")        \
    ERR(XHAL_ERR_IO, -7, "IO error")                \
    ERR(XHAL_ERR_INVALID, -8, "Invalid argument")   \
    ERR(XHAL_ERR_MEM_OVERLAY, -9, "Memory overlap") \
    ERR(XHAL_ERR_MALLOC, -10, "Malloc failed")      \
    ERR(XHAL_ERR_NOT_ENOUGH, -11, "Not enough")     \
    ERR(XHAL_ERR_NO_INIT, -12, "Not initialized")   \
    ERR(XHAL_ERR_BUS, -13, "Bus error")             \
    ERR(XHAL_ERR_NOT_SUPPORT, -14, "Not supported") \
    ERR(XHAL_ERR_NOT_FOUND, -15, "Not found")       \
    ERR(XHAL_ERR_CRC, -16, "CRC error")
typedef enum xhal_err
{
#define ERR(code, value, str) code = value,
    XHAL_ERR_LIST
#undef ERR
} xhal_err_t;

#define XHAL_RETURN_IF_ERROR(ret, expr) \
    do                                  \
    {                                   \
        (ret) = expr;                   \
        if ((ret) != XHAL_OK)           \
        {                               \
            return (ret);               \
        }                               \
    } while (0)

#define XHAL_GOTO_IF_ERROR(ret, expr, label) \
    do                                       \
    {                                        \
        (ret) = expr;                        \
        if ((ret) != XHAL_OK)                \
        {                                    \
            goto label;                      \
        }                                    \
    } while (0)

#define XHAL_TRY(ret, expr)             XHAL_RETURN_IF_ERROR(ret, expr)
#define XHAL_TRY_GOTO(ret, expr, label) XHAL_GOTO_IF_ERROR(ret, expr, label)

#define XTRY(ret, expr)                 XHAL_TRY(ret, expr)
#define XTRY_GOTO(ret, expr, label)     XHAL_TRY_GOTO(ret, expr, label)

#define XHAL_MAX(a, b)                  (((a) > (b)) ? (a) : (b))
#define XHAL_MIN(a, b)                  (((a) < (b)) ? (a) : (b))
#define XHAL_ABS(x)                     ((x) < 0 ? -(x) : (x))

#define XHAL_BOOL(x)                    (!!(x))

/* 向上取整到 m 的倍数: ceil(x / m) * m */
#define XHAL_CEIL(x, m)                 ((((x) + (m) - 1) / (m)) * (m))

/* 向下取整到 m 的倍数: floor(x / m) * m */
#define XHAL_FLOOR(x, m)                (((x) / (m)) * (m))

/* 四舍五入到 m 的倍数: round(x / m) * m */
#define XHAL_ROUND(x, m)                ((((x) + ((m) / 2)) / (m)) * (m))

/* 限定范围 [low, high] */
#define XHAL_CLAMP(val, low, high) \
    ((val) <= (low) ? (low) : ((val) >= (high) ? (high) : (val)))

/* 限定范围 [low, high] 且循环*/
#define XHAL_LOOP(val, low, high) \
    ((val) < (low) ? (high) : ((val) > (high) ? (low) : (val)))

#define XHAL_ADD_SATURATE_MAX(val, add, max) \
    (((val) + (add)) < (max) ? ((val) + (add)) : (max))

#define XHAL_SUB_SATURATE_MIN(val, sub, min) \
    (((val) > (sub) + (min)) ? ((val) - (sub)) : (min))

#if defined(__linux__)
#define XHAL_STR_ENTER "\n"
#else
#define XHAL_STR_ENTER "\r\n"
#endif

/**
 * @brief ARRAY_SIZE - 获取数组 arr 中的元素数量。
 *
 * @param arr 待求的数组。(必须是数组，此处不进行判断)
 *
 * @return 数组元素个数。
 */
#define XHAL_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/**
 * @brief 未使用的变量通过 UNUSED 防止编译器警告。
 */
#define XHAL_UNUSED(x) \
    do                 \
    {                  \
        (void)(x);     \
    } while (0)

#if defined(__x86_64__) || defined(__aarch64__)
typedef int64_t xhal_offset_t;
typedef uint64_t xhal_pointer_t;
typedef uint64_t xhal_size_t;
#elif defined(__i386__) || defined(__arm__)
typedef int32_t xhal_offset_t;
typedef uint32_t xhal_pointer_t;
typedef uint32_t xhal_size_t;
#else
#error The currnet CPU is NOT supported!
#endif

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

/**
 * Cast a member of a structure out to the containing structure.
 * @ptr:    the pointer to the member.
 * @type:   the type of the container struct this is embedded in.
 * @member: the name of the member within the struct.
 */
#define xhal_container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define xhal_offsetof(type, member) ((xhal_pointer_t) & ((type *)0)->member)

static inline const char *__basename(const char *path)
{
    const char *p    = path;
    const char *last = path;
    while (*p)
    {
        if (*p == '/' || *p == '\\')
            last = p + 1;
        p++;
    }
    return last;
}

#define XHAL_LINE            __LINE__
#define XHAL_FUNCNAME        __func__
#define XHAL_FILENAME        __basename(__FILE__)
#define XHAL_FILEPATH        __FILE__

#define XHAL_UNIQUE_ID(base) base##__COUNTER__

#define XHAL_STR(x)          #x
/**
 * @brief 字符串化。
 */
#define XHAL_XSTR(x)         XHAL_STR(x)

/* Compiler Related Definitions */
#if defined(__CC_ARM) || defined(__CLANG_ARM) /* ARM Compiler */

#include <stdarg.h>
#define XHAL_SECTION(x) __attribute__((section(x)))
#define XHAL_USED       __attribute__((used))
#define XHAL_ALIGN(n)   __attribute__((aligned(n)))
#define XHAL_WEAK       __attribute__((weak))
#define xhal_inline     static __inline

#elif defined(__IAR_SYSTEMS_ICC__) /* for IAR Compiler */

#include <stdarg.h>
#define XHAL_SECTION(x) @x
#define XHAL_USED       __root
#define XHAL_PRAGMA(x)  _Pragma(#x)
#define XHAL_ALIGN(n)   XHAL_PRAGMA(data_alignment = n)
#define XHAL_WEAK       __weak
#define xhal_inline     static inline

#elif defined(__GNUC__) /* GNU GCC Compiler */

#include <stdarg.h>
#define XHAL_SECTION(x) __attribute__((section(x)))
#define XHAL_USED       __attribute__((used))
#define XHAL_ALIGN(n)   __attribute__((aligned(n)))
#define XHAL_WEAK       __attribute__((weak))
#define xhal_inline     static inline

#else
#error The current compiler is NOT supported!
#endif /* defined(__CC_ARM) || defined(__CLANG_ARM) */

#endif /* __XHAL_DEF_H */
