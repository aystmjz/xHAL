#ifndef __XLOG_H
#define __XLOG_H

#include "../xlib/xhal_bit.h"
#include "xhal_config.h"
#include "xhal_def.h"
#include "xhal_std.h"

#define XLOG_LEVEL_ERROR   (1) /* 错误级别，只输出错误信息 */
#define XLOG_LEVEL_WARNING (2) /* 警告级别，输出警告信息 */
#define XLOG_LEVEL_INFO    (3) /* 信息级别，输出一般信息 */
#define XLOG_LEVEL_DEBUG   (4) /* 调试级别，输出最详细的调试信息 */
#define XLOG_LEVEL_MAX     (5)

#ifndef XLOG_DUMP_ENABLE
#define XLOG_DUMP_ENABLE (1)
#endif

#ifndef XLOG_COMPILE_LEVEL
#define XLOG_COMPILE_LEVEL (XLOG_LEVEL_DEBUG)
#endif

#ifndef XLOG_FILEINFO_ENABLE
#define XLOG_FILEINFO_ENABLE (1)
#endif

#ifndef XLOG_FULL_PATH_ENABLE
#define XLOG_FULL_PATH_ENABLE (0)
#endif

#if XLOG_FULL_PATH_ENABLE != 0
#define __XLOG_FILE__ __XHAL_FILEPATH__
#else
#define __XLOG_FILE__ __XHAL_FILENAME__
#endif

#if XLOG_FILEINFO_ENABLE != 0
#define XLOG_FILE_INFO   __XLOG_FILE__, __XHAL_LINE__
#define XLOG_FILE_FORMAT "(%s:%d) "
#else
#define XLOG_FILE_INFO
#define XLOG_FILE_FORMAT
#endif

/* 内部用函数指针，默认绑定 */
typedef void (*xlog_output_char_t)(char c);

void xlog_set_putc_func(xlog_output_char_t func);
void xlog_set_level(uint8_t level);

void xlog_putc(char c);
void xlog_puts(const char *s);

xhal_err_t xlog_printf(const char *fmt, ...);
xhal_err_t xlog_print_log(const char *name, uint8_t level, const char *fmt,
                          ...);

#if XLOG_COMPILE_LEVEL >= XLOG_LEVEL_ERROR
/**
 * @brief 定义模块标签
 * @param _tag 模块标签名称
 */
#define XLOG_TAG(_tag) static const char *TAG = _tag
#else
#define XLOG_TAG(_tag)
#endif

#if XLOG_COMPILE_LEVEL >= XLOG_LEVEL_ERROR
#define XLOG_ERROR(fmt, ...)                                    \
    xlog_print_log(TAG, XLOG_LEVEL_ERROR, XLOG_FILE_FORMAT fmt, \
                   XLOG_FILE_INFO, ##__VA_ARGS__)
#else
#define XLOG_ERROR(fmt, ...)
#endif

#if XLOG_COMPILE_LEVEL >= XLOG_LEVEL_WARNING
#define XLOG_WARN(fmt, ...)                                       \
    xlog_print_log(TAG, XLOG_LEVEL_WARNING, XLOG_FILE_FORMAT fmt, \
                   XLOG_FILE_INFO, ##__VA_ARGS__)
#else
#define XLOG_WARN(fmt, ...)
#endif

#if XLOG_COMPILE_LEVEL >= XLOG_LEVEL_INFO
#define XLOG_INFO(fmt, ...) \
    xlog_print_log(TAG, XLOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#else
#define XLOG_INFO(fmt, ...)
#endif

#if XLOG_COMPILE_LEVEL >= XLOG_LEVEL_DEBUG
#define XLOG_DEBUG(fmt, ...)                                    \
    xlog_print_log(TAG, XLOG_LEVEL_DEBUG, XLOG_FILE_FORMAT fmt, \
                   XLOG_FILE_INFO, ##__VA_ARGS__)
#define XDEBUG
#else
#define XLOG_DEBUG(fmt, ...)
#endif

#if XLOG_DUMP_ENABLE != 0

#define XLOG_DUMP_HEAD_BIT       (0) // flags_mask 中表头的标志位
#define XLOG_DUMP_ASCII_BIT      (1) // flags_mask 中 ASCII 的标志位
#define XLOG_DUMP_ESCAPE_BIT     (2) // flags_mask 中带有转义字符的标志位
#define XLOG_DUMP_TAIL_BIT       (3) // flags_mask 中表尾的标志位

#define XLOG_DUMP_TABLE          (BIT(XLOG_DUMP_HEAD_BIT) | BIT(XLOG_DUMP_TAIL_BIT))

/* 只输出 16 进制格式数据 */
#define XLOG_DUMP_FLAG_HEX_ONLY  (XLOG_DUMP_TABLE)
/* 输出 16 进制格式数据并带有 ASCII 字符 */
#define XLOG_DUMP_FLAG_HEX_ASCII (BIT(XLOG_DUMP_ASCII_BIT) | XLOG_DUMP_TABLE)
/* 输出 16 进制格式数据、 ASCII 字符、转义字符，其余输出 16 进制原始值 */
#define XLOG_DUMP_FLAG_HEX_ASCII_ESCAPE \
    (XLOG_DUMP_FLAG_HEX_ASCII | BIT(XLOG_DUMP_ESCAPE_BIT))

xhal_err_t xlog_dump_mem(void *addr, xhal_size_t size, uint8_t flags_mask);

#define XF_LOG_BUFFER_HEX(buffer, buffer_len) \
    xlog_dump_mem(buffer, buffer_len, XLOG_DUMP_FLAG_HEX_ONLY)
#define XF_LOG_BUFFER_HEXDUMP(buffer, buffer_len) \
    xlog_dump_mem(buffer, buffer_len, XLOG_DUMP_FLAG_HEX_ASCII)
#define XF_LOG_BUFFER_HEXDUMP_ESCAPE(buffer, buffer_len) \
    xlog_dump_mem(buffer, buffer_len, XLOG_DUMP_FLAG_HEX_ASCII_ESCAPE)

#else

#define XF_LOG_BUFFER_HEX(buffer, buffer_len)
#define XF_LOG_BUFFER_HEXDUMP(buffer, buffer_len)
#define XF_LOG_BUFFER_HEXDUMP_ESCAPE(buffer, buffer_len)

#endif /* XLOG_DUMP_ENABLE != 0 */

#endif /* __XLOG_H */
