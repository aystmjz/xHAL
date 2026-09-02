/**
 ******************************************************************************
 * @file    xhal_def.h
 * @author  aystmjz
 * @brief   基础定义头文件，提供错误码、通用宏、数据类型及编译器相关定义
 * @version 2.3.0
 * @date    2026-08-23
 ******************************************************************************
 * Copyright (c) 2026 aystmjz. All rights reserved.
 * SPDX-License-Identifier: MIT
 ******************************************************************************
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __XHAL_DEF_H
#define __XHAL_DEF_H

/* Includes ------------------------------------------------------------------*/
#include "xhal_std.h"

/** @addtogroup XHAL
 * @{
 */

/** @defgroup XCORE
 * @{
 */

/** @defgroup XHAL_DEF
 * @brief  基础定义，提供错误码、通用宏、数据类型及编译器相关定义
 * @{
 */

/* Exported constants --------------------------------------------------------*/
/**
 * @brief  无限等待超时时间
 */
#define XHAL_WAIT_FOREVER (0xFFFFFFFFU)

/* 错误码定义列表 ------------------------------------------------------------*/
/**
 * @brief  错误码列表，每项由 ERR(code, value, desc) 展开
 * @note   通过 XHAL_ERR_LIST 宏一次定义所有错误码，供枚举与字符串表复用
 */
#define XHAL_ERR_LIST                                    \
    ERR(XHAL_OK, 0, "Success")                           \
    ERR(XHAL_ERROR, -1, "General error")                 \
    ERR(XHAL_ERR_TIMEOUT, -2, "Timeout")                 \
    ERR(XHAL_ERR_RESOURCE, -3, "Resource not available") \
    ERR(XHAL_ERR_INVALID, -4, "Invalid argument")        \
    ERR(XHAL_ERR_NO_MEMORY, -5, "No memory")             \
    ERR(XHAL_ERR_ISR, -6, "Not allowed in ISR context")  \
    ERR(XHAL_ERR_EMPTY, -7, "Empty")                     \
    ERR(XHAL_ERR_FULL, -8, "Full")                       \
    ERR(XHAL_ERR_BUSY, -9, "Busy")                       \
    ERR(XHAL_ERR_IO, -10, "IO error")                    \
    ERR(XHAL_ERR_MEM_OVERLAY, -11, "Memory overlap")     \
    ERR(XHAL_ERR_MALLOC, -12, "Malloc failed")           \
    ERR(XHAL_ERR_NOT_ENOUGH, -13, "Not enough")          \
    ERR(XHAL_ERR_NO_INIT, -14, "Not initialized")        \
    ERR(XHAL_ERR_BUS, -15, "Bus error")                  \
    ERR(XHAL_ERR_NOT_SUPPORT, -16, "Not supported")      \
    ERR(XHAL_ERR_NOT_FOUND, -17, "Not found")            \
    ERR(XHAL_ERR_CRC, -18, "CRC error")                  \
    ERR(XHAL_ERR_EXIST, -19, "Already exist")

/* Exported types ------------------------------------------------------------*/
/**
 * @brief  xHAL 错误码枚举
 */
typedef enum xhal_err
{
#define ERR(code, value, str) code = value, /*!< 错误码: str */
    XHAL_ERR_LIST
#undef ERR
} xhal_err_t;

/* Exported macros -----------------------------------------------------------*/
/**
 * @brief  执行表达式，若结果非 XHAL_OK 则直接返回该错误码
 * @param  ret:  错误码变量
 * @param  expr: 待执行的表达式
 */
#define XHAL_RETURN_IF_ERROR(ret, expr) \
    do                                  \
    {                                   \
        (ret) = expr;                   \
        if ((ret) != XHAL_OK)           \
        {                               \
            return (ret);               \
        }                               \
    } while (0)

/**
 * @brief  执行表达式，若结果非 XHAL_OK 则跳转到指定标签
 * @param  ret:   错误码变量
 * @param  expr:  待执行的表达式
 * @param  label: 跳转标签
 */
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

#define XTRY(ret, expr)             XHAL_TRY(ret, expr)
#define XTRY_GOTO(ret, expr, label) XHAL_TRY_GOTO(ret, expr, label)

/* 数学运算宏 ----------------------------------------------------------------*/
#define XHAL_MAX(a, b) (((a) > (b)) ? (a) : (b)) /*!< 取最大值 */
#define XHAL_MIN(a, b) (((a) < (b)) ? (a) : (b)) /*!< 取最小值 */
#define XHAL_ABS(x)    ((x) < 0 ? -(x) : (x))    /*!< 取绝对值 */

#define XHAL_BOOL(x) (!!(x)) /*!< 转换为布尔值(0/1) */

/**
 * @brief  向上取整到 m 的倍数: ceil(x / m) * m
 * @param  x: 待取整的值
 * @param  m: 对齐粒度(须为 >0 的整数)
 */
#define XHAL_CEIL(x, m) ((((x) + (m) - 1) / (m)) * (m))

/**
 * @brief  向下取整到 m 的倍数: floor(x / m) * m
 * @param  x: 待取整的值
 * @param  m: 对齐粒度(须为 >0 的整数)
 */
#define XHAL_FLOOR(x, m) (((x) / (m)) * (m))

/**
 * @brief  四舍五入到 m 的倍数: round(x / m) * m
 * @param  x: 待取整的值
 * @param  m: 对齐粒度(须为 >0 的整数)
 */
#define XHAL_ROUND(x, m) ((((x) + ((m) / 2)) / (m)) * (m))

/**
 * @brief  将 val 限定在 [low, high] 范围内
 * @param  val:  待限定的值
 * @param  low:  下限
 * @param  high: 上限
 */
#define XHAL_CLAMP(val, low, high) \
    ((val) <= (low) ? (low) : ((val) >= (high) ? (high) : (val)))

/**
 * @brief  将 val 限定在 [low, high] 范围内，越界时循环到另一端
 * @param  val:  待限定的值
 * @param  low:  下限
 * @param  high: 上限
 */
#define XHAL_LOOP(val, low, high) \
    ((val) < (low) ? (high) : ((val) > (high) ? (low) : (val)))

/**
 * @brief  饱和加法，结果不超过 max
 */
#define XHAL_ADD_SATURATE_MAX(val, add, max) \
    (((val) + (add)) < (max) ? ((val) + (add)) : (max))

/**
 * @brief  饱和减法，结果不小于 min
 */
#define XHAL_SUB_SATURATE_MIN(val, sub, min) \
    (((val) > (sub) + (min)) ? ((val) - (sub)) : (min))

/**
 * @brief  换行符字符串，Linux 下为 \n，其余平台为 \r\n
 */
#if defined(__linux__)
    #define XHAL_STR_ENTER "\n"
#else
    #define XHAL_STR_ENTER "\r\n"
#endif

/**
 * @brief  ARRAY_SIZE - 获取数组 arr 中的元素数量。
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

/* 基础数据类型 --------------------------------------------------------------*/
/**
 * @brief  有符号偏移量类型，32 位平台为 int32_t，64 位平台为 int64_t
 */
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

typedef uint32_t u32; /*!< 32 位无符号整数 */
typedef uint16_t u16; /*!< 16 位无符号整数 */
typedef uint8_t u8;   /*!< 8 位无符号整数 */

/**
 * Cast a member of a structure out to the containing structure.
 * @ptr:    the pointer to the member.
 * @type:   the type of the container struct this is embedded in.
 * @member: the name of the member within the struct.
 */
#define xhal_container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/**
 * @brief  获取结构体成员在结构体中的偏移量
 */
#define xhal_offsetof(type, member) ((xhal_pointer_t) & ((type *)0)->member)

/**
 * @brief  提取文件路径中的文件名部分(不含目录)
 * @param  path: 文件完整路径
 * @retval 文件名字符串指针
 */
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

/* 调试信息宏 ----------------------------------------------------------------*/
#define XHAL_LINE     __LINE__     /*!< 当前行号 */
#define XHAL_FUNCNAME __func__     /*!< 当前函数名 */
#define XHAL_FILENAME __basename(__FILE__) /*!< 当前文件名(不含路径) */
#define XHAL_FILEPATH __FILE__     /*!< 当前文件完整路径 */

/**
 * @brief  生成唯一标识符，以 base 为前缀拼接 __COUNTER__
 */
#define XHAL_UNIQUE_ID(base) base##__COUNTER__

#define XHAL_STR(x) #x /*!< 直接字符串化 */
/**
 * @brief 字符串化（先展开宏再字符串化）。
 */
#define XHAL_XSTR(x) XHAL_STR(x)

/**
 * @brief  定义模块标签，供日志/断言模块识别所属模块
 * @param  tag: 模块名称，如 xLog、xMalloc
 * @note   用法: XHAL_TAG(xLog); 生成静态常量 __xhal_tag[]
 */
#define XHAL_TAG(tag) static const char __xhal_tag[] = #tag

/* Compiler Related Definitions ----------------------------------------------*/
#if defined(__CC_ARM) || defined(__CLANG_ARM) /* ARM Compiler */

    #include <stdarg.h>
    #define XHAL_SECTION(x) __attribute__((section(x))) /*!< 指定段 */
    #define XHAL_USED       __attribute__((used))       /*!< 防止被链接器优化 */
    #define XHAL_ALIGN(n)   __attribute__((aligned(n))) /*!< 指定对齐 */
    #define XHAL_WEAK       __attribute__((weak))       /*!< 弱符号 */
    #define xhal_inline     static __inline             /*!< 内联函数 */

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

/** @} */

/** @} */

/** @} */

#endif /* __XHAL_DEF_H */
