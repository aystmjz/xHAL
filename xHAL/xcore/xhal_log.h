#ifndef __XLOG_H
#define __XLOG_H

#include "../xlib/xhal_bit.h"
#include "xhal_common.h"
#include "xhal_config.h"
#include "xhal_def.h"

#define XLOG_LEVEL_NULL        (0)
#define XLOG_LEVEL_ERROR       (1) /* 错误级别，只输出错误信息 */
#define XLOG_LEVEL_WARNING     (2) /* 警告级别，输出警告信息 */
#define XLOG_LEVEL_INFO        (3) /* 信息级别，输出一般信息 */
#define XLOG_LEVEL_DEBUG       (4) /* 调试级别，输出最详细的调试信息 */
#define XLOG_LEVEL_MAX         (5)

#define XLOG_TIME_MOD_NONE     (0) /* 不显示时间 */
#define XLOG_TIME_MOD_MILLIS   (1) /* 毫秒时间戳 */
#define XLOG_TIME_MOD_RELATIVE (2) /* 相对时间 */
#define XLOG_TIME_MOD_ABSOLUTE (3) /* 绝对时间 */
#define XLOG_TIME_MOD_MAX      (4)

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
#define __XLOG_FILE__ XHAL_FILEPATH
#else
#define __XLOG_FILE__ XHAL_FILENAME
#endif

#if XLOG_FILEINFO_ENABLE != 0
#define XLOG_FILE_INFO   __XLOG_FILE__, XHAL_LINE
#define XLOG_FILE_FORMAT "(%s:%d) "
#else
#define XLOG_FILE_INFO
#define XLOG_FILE_FORMAT
#endif

#ifndef XLOG_DEFAULT_OUTPUT
#define XLOG_DEFAULT_OUTPUT xlog_default_output
#endif

typedef void (*xlog_output_t)(const void *data, uint32_t size);

uint8_t xlog_get_level(void);
uint8_t xlog_get_time_mod(void);
xhal_err_t xlog_set_level(uint8_t level);
xhal_err_t xlog_set_time_mod(uint8_t mod);

xhal_err_t _xlog_printf(xlog_output_t write, const char *fmt, ...);
xhal_err_t _xlog_print_log(xlog_output_t write, const char *name, uint8_t level,
                           const char *fmt, ...);

#if XLOG_COMPILE_LEVEL >= XLOG_LEVEL_ERROR

#define XLOG_TAG_IMPL_TF(tag, write, ...)               \
    static const char *TAG = tag;                       \
    extern void write(const void *data, uint32_t size); \
    static const XHAL_USED xlog_output_t WRITE = write;

#define XLOG_TAG_IMPL_F(tag, write, ...)                \
    extern void write(const void *data, uint32_t size); \
    static const XHAL_USED xlog_output_t WRITE = write;

/**
 * @brief 定义模块标签
 * @param tag 模块标签名称
 */
#define XLOG_TAG(...) XLOG_TAG_IMPL_TF(__VA_ARGS__, XLOG_DEFAULT_OUTPUT)
#else
#define XLOG_TAG(...) XLOG_TAG_IMPL_F(__VA_ARGS__, XLOG_DEFAULT_OUTPUT)
#endif /* XLOG_COMPILE_LEVEL >= XLOG_LEVEL_ERROR */

#define xlog_printf(fmt, ...) _xlog_printf(WRITE, fmt, ##__VA_ARGS__)

#if XLOG_COMPILE_LEVEL >= XLOG_LEVEL_ERROR
#define XLOG_ERROR(fmt, ...)                                            \
    _xlog_print_log(WRITE, TAG, XLOG_LEVEL_ERROR, XLOG_FILE_FORMAT fmt, \
                    XLOG_FILE_INFO, ##__VA_ARGS__)
#else
#define XLOG_ERROR(fmt, ...)
#endif

#if XLOG_COMPILE_LEVEL >= XLOG_LEVEL_WARNING
#define XLOG_WARN(fmt, ...)                                               \
    _xlog_print_log(WRITE, TAG, XLOG_LEVEL_WARNING, XLOG_FILE_FORMAT fmt, \
                    XLOG_FILE_INFO, ##__VA_ARGS__)
#else
#define XLOG_WARN(fmt, ...)
#endif

#if XLOG_COMPILE_LEVEL >= XLOG_LEVEL_INFO
#define XLOG_INFO(fmt, ...) \
    _xlog_print_log(WRITE, TAG, XLOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#else
#define XLOG_INFO(fmt, ...)
#endif

#if XLOG_COMPILE_LEVEL >= XLOG_LEVEL_DEBUG
#define XLOG_DEBUG(fmt, ...)                                            \
    _xlog_print_log(WRITE, TAG, XLOG_LEVEL_DEBUG, XLOG_FILE_FORMAT fmt, \
                    XLOG_FILE_INFO, ##__VA_ARGS__)
#define XDEBUG
#else
#define XLOG_DEBUG(fmt, ...)
#endif

#define XLOG_PRINT_ERR(info, err) \
    XLOG_ERROR("%s failed in %s: %s", info, __func__, xhal_err_to_str(err))

#ifdef XDEBUG
#define XLOG_CHECK_RET(write)            \
    do                                   \
    {                                    \
        xhal_err_t ret;                  \
        ret = write;                     \
        if (ret != XHAL_OK)              \
            XLOG_PRINT_ERR(#write, ret); \
    } while (0)
#else
#define XLOG_CHECK_RET(write) write
#endif

#endif /* __XLOG_H */
