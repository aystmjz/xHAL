#include "xhal_log.h"
#include "xhal_malloc.h"
#include "xhal_time.h"
#include <stdio.h>
#include <string.h>

#define XLOG_BUFF_SIZE     (256) /* 日志缓冲区大小 */
#define XLOG_DEFAULT_LEVEL (XLOG_LEVEL_DEBUG)

#define XLOG_TIME_MILLIS   (1)
#define XLOG_TIME_RELATIVE (2)
#define XLOG_TIME_ABSOLUTE (3)

#ifndef XLOG_COLOR_ENABLE
#define XLOG_COLOR_ENABLE (1) /* 日志颜色显示 */
#endif

#ifndef XLOG_NEWLINE_ENABLE
#define XLOG_NEWLINE_ENABLE (1) /* 日志换行 */
#endif

#ifndef XLOG_TIME_ENABLE
#define XLOG_TIME_ENABLE (1)
#endif

#ifndef XLOG_TIME_MODE
#define XLOG_TIME_MODE (XLOG_TIME_ABSOLUTE)
#endif

/* 终端颜色控制序列 */
#define NONE       "\033[0;0m"  /* 默认颜色 */
#define LIGHT_RED  "\033[1;31m" /* 浅红色 */
#define YELLOW     "\033[0;33m" /* 黄色 */
#define LIGHT_BLUE "\033[1;34m" /* 浅蓝色 */
#define GREEN      "\033[0;32m" /* 绿色 */

#ifdef XHAL_OS_SUPPORTING
#include "../xos/xhal_os.h"

static osMutexId_t _xlog_mutex(void);

static char buff[XLOG_BUFF_SIZE];

static osMutexId_t xlog_mutex              = NULL;
static const osMutexAttr_t xlog_mutex_attr = {
    .name      = "xlog_mutex",
    .attr_bits = osMutexPrioInherit | osMutexRecursive,
    .cb_mem    = NULL,
    .cb_size   = 0,
};
#endif

#if (XLOG_COLOR_ENABLE != 0)
static const char *const xlog_color_table[XLOG_LEVEL_MAX] = {
    NONE, LIGHT_RED, YELLOW, LIGHT_BLUE, GREEN,
};
#endif

static const char xlog_level_lable[XLOG_LEVEL_MAX] = {
    ' ', 'E', 'W', 'I', 'D',
};

static uint8_t xlog_level = XLOG_DEFAULT_LEVEL;

void xlog_default_output(const void *data, uint32_t size)
{
    const uint8_t *p = (const uint8_t *)data;

    for (uint32_t i = 0; i < size; i++)
    {
        putchar(p[i]);
    }
}

uint8_t xlog_get_level(void)
{
    return xlog_level;
}

xhal_err_t xlog_set_level(uint8_t level)
{

    if (xlog_level >= XLOG_LEVEL_MAX)
    {
        return XHAL_ERR_INVALID;
    }
    xlog_level = level;

    return XHAL_OK;
}

xhal_err_t _xlog_printf(xlog_output_t write, const char *fmt, ...)
{
    if (fmt == NULL)
    {
        return XHAL_ERR_INVALID;
    }

    xhal_err_t ret = XHAL_OK;

#ifdef XHAL_OS_SUPPORTING
    osMutexId_t mutex = _xlog_mutex();
    osMutexAcquire(mutex, osWaitForever);
#else
    char buff[XLOG_BUFF_SIZE];
#endif

    va_list args;
    va_start(args, fmt);

    int32_t count = vsnprintf(buff, sizeof(buff), fmt, args);

    va_end(args);

    if (count < 0)
    {
        ret = XHAL_ERR_INVALID;
        goto exit;
    }
    if (count >= sizeof(buff))
    {
        ret = XHAL_ERR_NO_MEMORY;
    }

    write(buff, count);

exit:
#ifdef XHAL_OS_SUPPORTING
    osMutexRelease(mutex);
#endif

    return ret;
}

xhal_err_t _xlog_print_log(xlog_output_t write, const char *name, uint8_t level,
                           const char *fmt, ...)
{
    if (fmt == NULL || name == NULL)
        return XHAL_ERR_INVALID;
    if (xlog_level < level)
        return XHAL_OK;

    xhal_err_t ret = XHAL_OK;

#ifdef XHAL_OS_SUPPORTING
    osMutexId_t mutex = _xlog_mutex();
    osMutexAcquire(mutex, osWaitForever);
#endif

    char buff[XLOG_BUFF_SIZE];
    int32_t count = 0;
    int32_t size  = 0;
    uint32_t len  = 0;
    XHAL_UNUSED(len);

#if XLOG_TIME_MODE == XLOG_TIME_MILLIS
    char str_buff[sizeof("XXXXXXXXXXX")];
    snprintf(str_buff, sizeof(str_buff), "%04llums", xtime_get_tick_ms());
#elif XLOG_TIME_MODE == XLOG_TIME_RELATIVE
    char str_buff[sizeof("HHHHH:MM:SS.XXX")];
    xtime_get_format_uptime(str_buff, sizeof(str_buff));
#elif XLOG_TIME_MODE == XLOG_TIME_ABSOLUTE
    char str_buff[sizeof("YYYY-MM-DD HH:MM:SS")];
    ret = xtime_get_format_time(str_buff, sizeof(str_buff));
    if (ret != XHAL_OK)
        snprintf(str_buff, sizeof(str_buff), "%04llums", xtime_get_tick_ms());
#endif

#if XLOG_COLOR_ENABLE != 0
#if (XLOG_TIME_MODE == XLOG_TIME_MILLIS) ||   \
    (XLOG_TIME_MODE == XLOG_TIME_RELATIVE) || \
    (XLOG_TIME_MODE == XLOG_TIME_ABSOLUTE)
    count =
        snprintf(buff, sizeof(buff), "%s[%c/%s %s] ", xlog_color_table[level],
                 xlog_level_lable[level], name, str_buff);
#else
    count = snprintf(buff, sizeof(buff), "%s[%c/%s] ", xlog_color_table[level],
                     xlog_level_lable[level], name);
#endif
#else
#if (XLOG_TIME_MODE == XLOG_TIME_MILLIS) ||   \
    (XLOG_TIME_MODE == XLOG_TIME_RELATIVE) || \
    (XLOG_TIME_MODE == XLOG_TIME_ABSOLUTE)
    count = snprintf(buff, sizeof(buff), "[%c/%s %s] ", xlog_level_lable[level],
                     name, str_buff);
#else
    count =
        snprintf(buff, sizeof(buff), "[%c/%s] ", xlog_level_lable[level], name);
#endif
#endif /* XLOG_COLOR_ENABLE */

    if (count < 0)
    {
        ret = XHAL_ERR_INVALID;
        goto exit;
    }
    if (count >= sizeof(buff))
    {
        count = sizeof(buff);
        ret   = XHAL_ERR_NO_MEMORY;
        goto write;
    }

    va_list args;
    va_start(args, fmt);
    size = vsnprintf(buff + count, sizeof(buff) - count, fmt, args);
    va_end(args);

    if (size < 0)
    {
        ret = XHAL_ERR_INVALID;
        goto exit;
    }
    if (size >= (sizeof(buff) - count))
    {
        count = sizeof(buff);
        ret   = XHAL_ERR_NO_MEMORY;
        goto write;
    }
    count += size;

#if XLOG_NEWLINE_ENABLE != 0
    len = strlen(XHAL_STR_ENTER);
    if ((sizeof(buff) - count) >= len)
    {
        xmemcpy(buff + count, XHAL_STR_ENTER, len);
        count += len;
    }
#endif

#if XLOG_COLOR_ENABLE != 0
    len = strlen(NONE);
    if ((sizeof(buff) - count) >= len)
    {
        xmemcpy(buff + count, NONE, len);
        count += len;
    }
#endif

write:
    write(buff, count);

exit:
#ifdef XHAL_OS_SUPPORTING
    osMutexRelease(mutex);
#endif

    return ret;
}

#ifdef XHAL_OS_SUPPORTING
static osMutexId_t _xlog_mutex(void)
{
    if (xlog_mutex == NULL)
    {
        xlog_mutex = osMutexNew(&xlog_mutex_attr);
    }

    return xlog_mutex;
}
#endif
