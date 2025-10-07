#include "xhal_time.h"
#include "xhal_assert.h"
#include "xhal_export.h"
#include "xhal_log.h"
#include <stdio.h>

XLOG_TAG("xTime");

#define XTIME_INVALID_TS    (0)
#define XTIME_ALLOWED_DRIFT (5)

#ifndef XTIME_AUTO_SYNC_ENABLE
#define XTIME_AUTO_SYNC_ENABLE (1)
#endif

#ifndef XTIME_AUTO_SYNC_TIME
#define XTIME_AUTO_SYNC_TIME (60 * 60 * 60)
#endif

#ifdef XHAL_OS_SUPPORTING
#include "../xos/xhal_os.h"

static osMutexId_t _xtime_mutex(void);

static osMutexId_t xtime_mutex              = NULL;
static const osMutexAttr_t xtime_mutex_attr = {
    .name      = "xtime_mutex",
    .attr_bits = osMutexPrioInherit | osMutexRecursive,
    .cb_mem    = NULL,
    .cb_size   = 0,
};
#endif

static xhal_ts_t _default_rtc_get_ts(void);

static volatile xhal_tick_ms_t sys_base_ms = 0; /* 系统基准毫秒计数器 */

static xhal_ts_t startup_ts = XTIME_INVALID_TS; /* 系统启动时的时间戳 */
static xhal_ts_t base_ts = XTIME_INVALID_TS;    /* 基准时间戳 */

/* 函数指针变量，初始指向默认函数 */
static rtc_get_ts_func_t _rtc_get_ts_func = _default_rtc_get_ts;

/**
 * @brief  设置RTC获取时间戳函数
 * @param  func RTC时间戳获取函数指针
 */
void xtime_set_rtc_get_ts_func(rtc_get_ts_func_t func)
{
    if (func != NULL)
        _rtc_get_ts_func = func;
    else
        _rtc_get_ts_func = _default_rtc_get_ts;
}

xhal_tick_ms_t xtime_get_tick_ms(void)
{
#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret_os = osOK;
    osMutexId_t mutex = _xtime_mutex();
    ret_os            = osMutexAcquire(mutex, osWaitForever);
    xassert(ret_os == osOK);
#endif
    xhal_tick_ms_t ret;

    if (startup_ts == XTIME_INVALID_TS)
    {
        ret = sys_base_ms;
    }
    else
    {
        ret = ((xhal_tick_ms_t)(base_ts - startup_ts) * 1000ULL) + sys_base_ms;
    }

#ifdef XHAL_OS_SUPPORTING
    ret_os = osMutexRelease(mutex);
    xassert(ret_os == osOK);
#endif

    return ret;
}

xhal_tick_sec_t xtime_get_tick_sec(void)
{
#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret_os = osOK;
    osMutexId_t mutex = _xtime_mutex();
    ret_os            = osMutexAcquire(mutex, osWaitForever);
    xassert(ret_os == osOK);
#endif
    xhal_tick_sec_t ret;
    xhal_tick_sec_t sys_base_sec = (xhal_tick_sec_t)(sys_base_ms / 1000);

    if (startup_ts == XTIME_INVALID_TS)
    {
        ret = sys_base_sec;
    }
    else
    {
        ret = (xhal_tick_sec_t)(base_ts - startup_ts) + sys_base_sec;
    }
#ifdef XHAL_OS_SUPPORTING
    ret_os = osMutexRelease(mutex);
    xassert(ret_os == osOK);
#endif
    return ret;
}

void xtime_delay_ms(uint32_t delay_ms)
{
    xhal_tick_ms_t start = xtime_get_tick_ms();
    while ((xtime_get_tick_ms() - start) < delay_ms)
    {
    }
}

void xtime_delay_s(uint32_t delay_s)
{
    xtime_delay_ms((xhal_tick_ms_t)delay_s * 1000ULL);
}

/**
 * @brief  获取格式化的系统运行时间字符串
 * @param  time_str 输出的时间字符串缓冲区
 * @param  buff_len 缓冲区长度
 * @retval 错误码
 */
xhal_err_t xtime_get_format_uptime(char *time_str, uint8_t buff_len)
{
    xassert_not_null(time_str);

    xhal_tick_sec_t total_seconds = xtime_get_tick_sec();
    uint32_t hours                = total_seconds / 3600;
    uint32_t minutes              = (total_seconds % 3600) / 60;
    uint32_t seconds              = total_seconds % 60;

    /* 格式化时间字符串，包含小时、分钟、秒和毫秒 */
    uint32_t count = snprintf(time_str, buff_len, "%01u:%02u:%02u.%03u", hours,
                              minutes, seconds, (uint32_t)(sys_base_ms % 1000));

    if (count >= buff_len)
    {
        return XHAL_ERR_NO_MEMORY;
    }

    return XHAL_OK;
}

/**
 * @brief  获取当前时间戳
 * @retval 当前时间戳，如果未设置基准时间则返回无效时间戳
 */
xhal_ts_t xtime_get_ts(void)
{
#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret_os = osOK;
    osMutexId_t mutex = _xtime_mutex();
    ret_os            = osMutexAcquire(mutex, osWaitForever);
    xassert(ret_os == osOK);
#endif
    xhal_ts_t ret;

    if (base_ts == XTIME_INVALID_TS)
    {
        ret = XTIME_INVALID_TS; // 未设置基准时间
    }
    else
    {
        ret = base_ts + (xhal_ts_t)(sys_base_ms / 1000);
    }
#ifdef XHAL_OS_SUPPORTING
    ret_os = osMutexRelease(mutex);
    xassert(ret_os == osOK);
#endif
    return ret;
}

/**
 * @brief  获取格式化的当前时间字符串
 * @param  time_str 输出的时间字符串缓冲区
 * @param  buff_len 缓冲区长度
 * @retval 错误码
 */
xhal_err_t xtime_get_format_time(char *time_str, uint8_t buff_len)
{
    xassert_not_null(time_str);

    xhal_ts_t rawtime = xtime_get_ts();

    /* 检查时间戳是否有效 */
    if (rawtime == XTIME_INVALID_TS)
    {
        return XHAL_ERR_NO_SYSTEM;
    }

    struct tm timeinfo;
    localtime_r(&rawtime, &timeinfo);

    uint32_t count =
        snprintf(time_str, buff_len, "%4d-%02d-%02d %02d:%02d:%02d",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    if (count >= buff_len)
    {
        return XHAL_ERR_NO_MEMORY;
    }

    return XHAL_OK;
}

/**
 * @brief  从RTC同步时间
 * @retval 错误码
 */
xhal_err_t xtime_sync_time_from_rtc(void)
{
    xhal_ts_t rtc_ts = _rtc_get_ts_func();

    /* 检查RTC时间戳是否有效 */
    if (rtc_ts == XTIME_INVALID_TS)
    {
#ifdef XDEBUG
        XLOG_ERROR("XHAL_ERR_NO_SYSTEM");
#endif
        return XHAL_ERR_NO_SYSTEM;
    }

    /* 检查RTC时间是否有效 */
    if (base_ts != XTIME_INVALID_TS && (rtc_ts + XTIME_ALLOWED_DRIFT) < base_ts)
    {
#ifdef XDEBUG
        XLOG_ERROR("XHAL_ERR_INVALID");
#endif
        return XHAL_ERR_INVALID;
    }
#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret_os = osOK;
    osMutexId_t mutex = _xtime_mutex();
    ret_os            = osMutexAcquire(mutex, osWaitForever);
    xassert(ret_os == osOK);
#endif
    /* 首次同步时间 */
    if (startup_ts == XTIME_INVALID_TS)
    {
        startup_ts = rtc_ts - (sys_base_ms / 1000);
        base_ts    = startup_ts;
        XLOG_INFO("RTC time first sync completed, base timestamp: %u", base_ts);
    }
    else
    {
        /* 重新同步时间 */
        sys_base_ms = 0;
        base_ts     = rtc_ts;
        XLOG_INFO("RTC time resync completed, timestamp: %u", rtc_ts);
    }
#ifdef XHAL_OS_SUPPORTING
    ret_os = osMutexRelease(mutex);
    xassert(ret_os == osOK);
#endif
    return XHAL_OK;
}
/* 根据配置决定是否启用自动时间同步 */
#if XTIME_AUTO_SYNC_ENABLE != 0
#ifdef XHAL_OS_SUPPORTING
POLL_EXPORT_OS(xtime_sync_time_from_rtc, XTIME_AUTO_SYNC_TIME, osPriorityHigh,
               512);
#else
POLL_EXPORT(xtime_sync_time_from_rtc, XTIME_AUTO_SYNC_TIME);
#endif
#endif

/**
 * @brief  SysTick毫秒中断处理函数
 */
void xtime_ms_tick_handler(void)
{
    sys_base_ms++;
}

/**
 * @brief  默认RTC获取时间戳函数
 * @retval 无效时间戳
 */
static xhal_ts_t _default_rtc_get_ts(void)
{
    return XTIME_INVALID_TS;
}

#ifdef XHAL_OS_SUPPORTING
static osMutexId_t _xtime_mutex(void)
{
    if (xtime_mutex == NULL)
    {
        xtime_mutex = osMutexNew(&xtime_mutex_attr);
        xassert_not_null(xtime_mutex);
    }

    return xtime_mutex;
}
#endif
