#include "xhal_log.h"
#include "xhal_assert.h"
#include "xhal_malloc.h"
#include "xhal_serial.h"
#include "xhal_time.h"
#include <stdio.h>
#include <string.h>

XHAL_TAG(xLog);

#if (XHAL_SHELL == 1)
    #include "../xshell/xhal_shell.h"
extern Shell *shell_get(void);
#endif

#ifndef XLOG_DEFAULT_LEVEL
    #define XLOG_DEFAULT_LEVEL XLOG_LEVEL_DEBUG
#endif

#ifndef XLOG_DEFAULT_TIME_MODE
    #define XLOG_DEFAULT_TIME_MODE XLOG_TIME_MOD_RELATIVE
#endif

#ifndef XLOG_COLOR_ENABLE
    #define XLOG_COLOR_ENABLE 1 /* 日志颜色显示 */
#endif

#ifndef XLOG_NEWLINE_ENABLE
    #define XLOG_NEWLINE_ENABLE 1 /* 日志换行 */
#endif

#define XLOG_BUFF_SIZE 512 /* 日志缓冲区大小 */

/* 终端颜色控制序列 */
#define NONE           "\033[0;0m"  /* 默认颜色 */
#define LIGHT_RED      "\033[1;31m" /* 浅红色 */
#define YELLOW         "\033[0;33m" /* 黄色 */
#define LIGHT_BLUE     "\033[1;34m" /* 浅蓝色 */
#define GREEN          "\033[0;32m" /* 绿色 */

#if (XHAL_OS_SUPPORTING == 1)
    #include "../xos/xhal_os.h"
static osMutexId_t _get_xlog_mutex(void);
static char xlog_buff[XLOG_BUFF_SIZE];
static osMutexId_t xlog_mutex              = NULL;
static const osMutexAttr_t xlog_mutex_attr = {
    .name      = "xlog_mutex",
    .attr_bits = osMutexPrioInherit | osMutexRecursive,
};
#endif

#if (XLOG_COLOR_ENABLE == 1)
static const char *const xlog_color_table[XLOG_LEVEL_MAX] = {
    NONE, LIGHT_RED, YELLOW, LIGHT_BLUE, GREEN,
};
#endif

static const char xlog_level_lable[XLOG_LEVEL_MAX] = {
    ' ', 'E', 'W', 'I', 'D',
};

static uint8_t xlog_level    = XLOG_DEFAULT_LEVEL;
static uint8_t xlog_time_mod = XLOG_DEFAULT_TIME_MODE;

extern void xlog_output(char *data, uint32_t size);
XHAL_WEAK void xlog_output(char *data, uint32_t size)
{
#if (XHAL_SHELL == 1)
    Shell *shell = shell_get();
    shellRefreshLineStart(shell);
#endif
    xhal_periph_t *uart = xperiph_find(XHAL_DEFAULT_UART);
    if (uart != NULL)
    {
        xserial_write(uart, data, size, XHAL_WAIT_FOREVER);
    }
#if (XHAL_SHELL == 1)
    shellRefreshLineEnd(shell);
#endif
}

uint8_t xlog_get_level(void)
{
    return xlog_level;
}

xhal_err_t xlog_set_level(uint8_t level)
{

    if (level >= XLOG_LEVEL_MAX || level > XLOG_COMPILE_LEVEL)
    {
        return XHAL_ERR_INVALID;
    }
    xlog_level = level;

    return XHAL_OK;
}

uint8_t xlog_get_time_mod(void)
{
    return xlog_time_mod;
}

xhal_err_t xlog_set_time_mod(uint8_t mod)
{

    if (mod >= XLOG_TIME_MOD_MAX)
    {
        return XHAL_ERR_INVALID;
    }
    xlog_time_mod = mod;

    return XHAL_OK;
}

xhal_err_t _xlog_printf(const char *fmt, ...)
{
    xassert_not_null(fmt);

    xhal_err_t ret = XHAL_OK;

#if (XHAL_OS_SUPPORTING == 1)
    osStatus_t ret_os = osOK;
    osMutexId_t mutex = _get_xlog_mutex();
    ret_os            = osMutexAcquire(mutex, osWaitForever);
    if (ret_os != osOK)
    {
        return (xhal_err_t)ret_os;
    }
#else
    char xlog_buff[XLOG_BUFF_SIZE];
#endif

    va_list args;
    va_start(args, fmt);

    int32_t count = vsnprintf(xlog_buff, sizeof(xlog_buff), fmt, args);

    va_end(args);

    if (count < 0)
    {
        ret = XHAL_ERR_INVALID;
        goto exit;
    }
    if (count >= sizeof(xlog_buff))
    {
        count = (int32_t)sizeof(xlog_buff);
        ret   = XHAL_ERR_NO_MEMORY;
    }

    xlog_output(xlog_buff, count);

exit:
#if (XHAL_OS_SUPPORTING == 1)
    ret_os = osMutexRelease(mutex);
    xassert(ret_os == osOK);
#endif
    return ret;
}

xhal_err_t _xlog_print_log(const char *name, uint8_t level, const char *fmt,
                           ...)
{
    xassert_not_null(name);
    xassert_not_null(fmt);

    if (xlog_level < level)
        return XHAL_OK;

#if (XHAL_OS_SUPPORTING == 1)
    osStatus_t ret_os = osOK;
    osMutexId_t mutex = _get_xlog_mutex();
    ret_os            = osMutexAcquire(mutex, osWaitForever);
    if (ret_os != osOK)
    {
        return (xhal_err_t)ret_os;
    }
#else
    char xlog_buff[XLOG_BUFF_SIZE];
#endif

    xhal_err_t ret    = XHAL_OK;
    int32_t count     = 0;
    int32_t size      = 0;
    xhal_size_t len   = 0;
    char str_buff[32] = {0};

    switch (xlog_time_mod)
    {
    case XLOG_TIME_MOD_MILLIS:
        snprintf(str_buff, sizeof(str_buff), "%04ums", xtime_get_tick_ms());
        break;

    case XLOG_TIME_MOD_RELATIVE:
        ret = xtime_get_format_uptime(str_buff, sizeof(str_buff));
        break;

    case XLOG_TIME_MOD_ABSOLUTE:
        ret = xtime_get_format_time(str_buff, sizeof(str_buff));
        break;

    case XLOG_TIME_MOD_NONE:
        str_buff[0] = '\0';
        break;

    default:
        snprintf(str_buff, sizeof(str_buff), "%04ums", xtime_get_tick_ms());
        break;
    }

    if (ret != XHAL_OK)
    {
        snprintf(str_buff, sizeof(str_buff), "%04ums", xtime_get_tick_ms());
    }

#if (XLOG_COLOR_ENABLE == 1)
    if (xlog_time_mod == XLOG_TIME_MOD_NONE)
    {
        count =
            snprintf(xlog_buff, sizeof(xlog_buff), "%s[%c/%s] ",
                     xlog_color_table[level], xlog_level_lable[level], name);
    }
    else
    {
        count = snprintf(xlog_buff, sizeof(xlog_buff), "%s[%c/%s %s] ",
                         xlog_color_table[level], xlog_level_lable[level], name,
                         str_buff);
    }
#else
    if (xlog_time_mod == XLOG_TIME_MOD_NONE)
    {
        count = snprintf(xlog_buff, sizeof(xlog_buff), "[%c/%s %s] ",
                         xlog_level_lable[level], name, str_buff);
    }
    else
    {
        count = snprintf(xlog_buff, sizeof(xlog_buff), "[%c/%s] ",
                         xlog_level_lable[level], name);
    }
#endif /*(XLOG_COLOR_ENABLE == 1)*/

    if (count < 0)
    {
        ret = XHAL_ERR_INVALID;
        goto exit;
    }
    if (count >= sizeof(xlog_buff))
    {
        count = sizeof(xlog_buff);
        ret   = XHAL_ERR_NO_MEMORY;
        goto write;
    }

    va_list args;
    va_start(args, fmt);
    size = vsnprintf(xlog_buff + count, sizeof(xlog_buff) - count, fmt, args);
    va_end(args);

    if (size < 0)
    {
        ret = XHAL_ERR_INVALID;
        goto exit;
    }
    if (size >= (sizeof(xlog_buff) - count))
    {
        count = sizeof(xlog_buff);
        ret   = XHAL_ERR_NO_MEMORY;
        goto write;
    }
    count += size;

#if (XLOG_NEWLINE_ENABLE == 1)
    len = strlen(XHAL_STR_ENTER);
    if ((sizeof(xlog_buff) - count) >= len)
    {
        xmemcpy(xlog_buff + count, XHAL_STR_ENTER, len);
        count += len;
    }
#endif

#if (XLOG_COLOR_ENABLE == 1)
    len = strlen(NONE);
    if ((sizeof(xlog_buff) - count) >= len)
    {
        xmemcpy(xlog_buff + count, NONE, len);
        count += len;
    }
#endif

write:
    xlog_output(xlog_buff, count);

exit:
#if (XHAL_OS_SUPPORTING == 1)
    ret_os = osMutexRelease(mutex);
    xassert(ret_os == osOK);
#endif
    return ret;
}

#if (XHAL_OS_SUPPORTING == 1)
static osMutexId_t _get_xlog_mutex(void)
{
    if (xlog_mutex == NULL)
    {
        xlog_mutex = osMutexNew(&xlog_mutex_attr);
    }

    return xlog_mutex;
}
#endif
