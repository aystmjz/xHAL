#include "xhal_time.h"
#include "xhal_assert.h"
#include "xhal_log.h"
#include <stdio.h>

XLOG_TAG("xTime");

#define XTIME_NOP()      __NOP()
#define XTIME_INVALID_TS (0)

#ifndef XTIME_USE_DWT_DELAY
#define XTIME_USE_DWT_DELAY (0)
#endif

#if XTIME_USE_DWT_DELAY == 0
#include XHAL_DEVICE_HEADER
#endif

#if XTIME_USE_DWT_DELAY != 0
#include XHAL_DEVICE_HEADER

#if !defined(DWT) || !defined(DWT_CTRL_CYCCNTENA_Msk)
#error \
    "DWT cycle counter is not available on this MCU, disable XTIME_USE_DWT_DELAY"
#endif

#endif

#ifndef XTIME_AUTO_SYNC_ENABLE
#define XTIME_AUTO_SYNC_ENABLE (1)
#endif

#ifndef XTIME_AUTO_SYNC_TIME
#define XTIME_AUTO_SYNC_TIME (60 * 60 * 60)
#endif

#ifndef XTIME_CPU_FREQ_HZ
#error "Please define XTIME_CPU_FREQ_HZ"
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

static volatile xhal_tick_t xtime_sys_tick_ms     = 0;
static volatile xhal_uptime_t xtime_sys_uptime_ms = 0;

static xhal_tick_t xtime_sync_tick_ms = 0;
static xhal_ts_t xtime_base_ts        = XTIME_INVALID_TS;

xhal_tick_t xtime_get_tick_ms(void)
{
    return xtime_sys_tick_ms;
}

xhal_uptime_t xtime_get_uptime_ms(void)
{
    return xtime_sys_uptime_ms;
}

void xtime_delay_us(uint32_t delay_us)
{
#if XTIME_USE_DWT_DELAY
    static uint8_t initialized = 0;

    if (!initialized)
    {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

        initialized = 1;
    }

    uint32_t clk   = (XTIME_CPU_FREQ_HZ / 1000000U);
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = delay_us * clk;

    while ((DWT->CYCCNT - start) < ticks)
    {
    }
#else
    uint32_t count = delay_us * (XTIME_CPU_FREQ_HZ / 8U / 1000000U);
    while (count--)
    {
        XTIME_NOP();
    }
#endif
}

void xtime_delay_ms(uint32_t delay_ms)
{
    if (delay_ms == 0)
    {
        return;
    }

#ifdef XHAL_OS_SUPPORTING
    osDelay(XOS_MS_TO_TICKS(delay_ms));
#else
    xhal_tick_t start = xtime_get_tick_ms();
    while (TIME_DIFF(xtime_get_tick_ms(), start) < delay_ms)
    {
        XTIME_NOP();
    }
#endif
}

void xtime_delay_s(uint32_t delay_s)
{
    if (delay_s == 0)
    {
        return;
    }

    uint64_t total_ms = (uint64_t)delay_s * 1000ULL;

    while (total_ms > UINT32_MAX)
    {
        xtime_delay_ms(UINT32_MAX);
        total_ms -= UINT32_MAX;
    }

    if (total_ms > 0)
    {
        xtime_delay_ms((uint32_t)total_ms);
    }
}

/**
 * @brief  获取格式化的系统运行时间字符串
 * @param  time_str 输出的时间字符串缓冲区
 * @param  buff_len 缓冲区长度
 * @retval 错误码
 */
xhal_err_t xtime_get_format_uptime(char *time_str, uint32_t buff_len)
{
    xassert_not_null(time_str);

    xhal_uptime_t uptime_ms = xtime_get_uptime_ms();

    uint32_t total_seconds = uptime_ms / 1000;
    uint32_t days          = total_seconds / (24 * 3600);
    uint32_t hours         = (total_seconds % (24 * 3600)) / 3600;
    uint32_t minutes       = (total_seconds % 3600) / 60;
    uint32_t seconds       = total_seconds % 60;
    uint32_t milliseconds  = uptime_ms % 1000;

    uint32_t count;

    if (days > 0)
    {
        /* "Dd HH:MM:SS.mmm" 或 "D HH:MM:SS.mmm" */
        count = snprintf(time_str, buff_len, "%ud %02u:%02u:%02u.%03u", days,
                         hours, minutes, seconds, milliseconds);
    }
    else
    {
        /* "HH:MM:SS.mmm" */
        count = snprintf(time_str, buff_len, "%02u:%02u:%02u.%03u", hours,
                         minutes, seconds, milliseconds);
    }

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
    xhal_ts_t ts;

    if (xtime_base_ts == XTIME_INVALID_TS)
    {
        ts = XTIME_INVALID_TS;
    }
    else
    {
        ts = xtime_base_ts +
             (xhal_ts_t)(TIME_DIFF(xtime_sys_tick_ms, xtime_sync_tick_ms) /
                         1000);
    }
#ifdef XHAL_OS_SUPPORTING
    ret_os = osMutexRelease(mutex);
    xassert(ret_os == osOK);
#endif
    return ts;
}

/**
 * @brief  获取格式化的当前时间字符串
 * @param  time_str 输出的时间字符串缓冲区
 * @param  buff_len 缓冲区长度
 * @retval 错误码
 */
xhal_err_t xtime_get_format_time(char *time_str, uint32_t buff_len)
{
    xassert_not_null(time_str);

    xhal_ts_t rawtime = xtime_get_ts();

    /* 检查时间戳是否有效 */
    if (rawtime == XTIME_INVALID_TS)
    {
        return XHAL_ERR_NO_INIT;
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
 * @brief  同步时间
 * @retval 错误码
 */
xhal_err_t xtime_sync_time(xhal_ts_t ts)
{
    if (ts == XTIME_INVALID_TS)
    {
        return XHAL_ERR_INVALID;
    }

#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret_os = osOK;
    osMutexId_t mutex = _xtime_mutex();
    ret_os            = osMutexAcquire(mutex, osWaitForever);
    xassert(ret_os == osOK);
#endif

    xtime_sync_tick_ms = xtime_sys_tick_ms;
    xtime_base_ts      = ts;

    XLOG_INFO("RTC time resync completed, timestamp: %u", ts);

#ifdef XHAL_OS_SUPPORTING
    ret_os = osMutexRelease(mutex);
    xassert(ret_os == osOK);
#endif
    return XHAL_OK;
}

/**
 * @brief  SysTick毫秒中断处理函数
 */
void xtime_ms_tick_handler(void)
{
    xtime_sys_tick_ms++;
    xtime_sys_uptime_ms++;
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
