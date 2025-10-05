#ifndef __XHAL_DEF_H
#define __XHAL_DEF_H

#include "xhal_std.h"

#define XHAL_NAME_SIZE (32)

typedef enum xhal_err
{
    XHAL_OK        = 0,    /* 操作成功，无错误 */
    XHAL_ERROR     = -1,   /* 通用错误，未指定具体错误类型 */
    XHAL_ERR_EMPTY = -2,   /* 空错误，例如队列或缓冲区为空 */
    XHAL_ERR_FULL  = -3,   /* 满错误，例如队列或缓冲区已满 */
    XHAL_ERR_TIMEOUT = -4, /* 超时错误，操作在指定时间内未完成 */
    XHAL_ERR_BUSY      = -5, /* 忙错误，设备或资源正在使用中 */
    XHAL_ERR_NO_MEMORY = -6, /* 内存不足错误，无法分配所需内存 */
    XHAL_ERR_IO        = -7, /* IO错误，输入输出操作失败 */
    XHAL_ERR_INVALID   = -8, /* 无效参数错误，传入的参数不合法 */
    XHAL_ERR_MEM_OVERLAY = -9, /* 内存重叠错误，内存区域发生重叠 */
    XHAL_ERR_MALLOC = -10, /* 内存分配错误，malloc等内存分配函数失败 */
    XHAL_ERR_NOT_ENOUGH = -11, /* 不足错误，资源或数据不够 */
    XHAL_ERR_NO_SYSTEM  = -12, /* 系统错误，系统资源不可用 */
    XHAL_ERR_BUS        = -13, /* 总线错误，硬件总线通信失败 */
} xhal_err_t;


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
typedef int64_t xhal_pointer_t;
typedef int64_t xhal_size_t;
#elif defined(__i386__) || defined(__arm__)
typedef int32_t xhal_pointer_t;
typedef int32_t xhal_size_t;
#else
#error The currnet CPU is NOT supported!
#endif

/**
 * Cast a member of a structure out to the containing structure.
 * @ptr:    the pointer to the member.
 * @type:   the type of the container struct this is embedded in.
 * @member: the name of the member within the struct.
 */
#define xhal_container_of(pointer, type, member)        \
    ({                                                  \
        void *__pointer = (void *)(pointer);            \
        ((type *)(__pointer - offsetof(type, member))); \
    })

#define xhal_offsetof(type, member) ((xhal_pointer_t) & ((type *)0)->member)

#define __FILENAME__                (__basename(__FILE__))

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

#define __XHAL_LINE__     __LINE__
#define __XHAL_FILENAME__ (__basename(__FILE__))
#define __XHAL_FILEPATH__ __FILE__

#define XHAL_STR(x)       #x
/**
 * @brief 字符串化。
 */
#define XHAL_XSTR(x)      XHAL_STR(x)

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
