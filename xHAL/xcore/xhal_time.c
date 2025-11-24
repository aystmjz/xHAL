#include "xhal_time.h"
#include "xhal_assert.h"
#include "xhal_log.h"
#include <stdio.h>

XLOG_TAG("xTime");

#define XTIME_INVALID_TS    (0)
#define XTIME_ALLOWED_DRIFT (5)

#ifndef XTIME_USE_DWT_DELAY
#define XTIME_USE_DWT_DELAY (0)
#endif

#if XTIME_USE_DWT_DELAY != 0
#include XHAL_CMSIS_DEVICE_HEADER

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

static volatile xhal_tick_ms_t xtime_sys_base_ms = 0; /* 系统基准毫秒计数器 */

static xhal_ts_t xtime_startup_ts = XTIME_INVALID_TS; /* 系统启动时的时间戳 */
static xhal_ts_t xtime_base_ts = XTIME_INVALID_TS; /* 基准时间戳 */

xhal_tick_ms_t xtime_get_tick_ms(void)
{
#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret_os = osOK;
    osMutexId_t mutex = _xtime_mutex();
    ret_os            = osMutexAcquire(mutex, osWaitForever);
    xassert(ret_os == osOK);
#endif
    xhal_tick_ms_t ret;

    if (xtime_startup_ts == XTIME_INVALID_TS)
    {
        ret = xtime_sys_base_ms;
    }
    else
    {
        ret = ((xhal_tick_ms_t)(xtime_base_ts - xtime_startup_ts) * 1000ULL) +
              xtime_sys_base_ms;
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
    xhal_tick_sec_t sys_base_sec = (xhal_tick_sec_t)(xtime_sys_base_ms / 1000);

    if (xtime_startup_ts == XTIME_INVALID_TS)
    {
        ret = sys_base_sec;
    }
    else
    {
        ret =
            (xhal_tick_sec_t)(xtime_base_ts - xtime_startup_ts) + sys_base_sec;
    }
#ifdef XHAL_OS_SUPPORTING
    ret_os = osMutexRelease(mutex);
    xassert(ret_os == osOK);
#endif
    return ret;
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
        __NOP();
    }
#endif
}

void xtime_delay_ms(uint32_t delay_ms)
{
#ifdef XHAL_OS_SUPPORTING
    osDelay(XOS_MS_TO_TICKS(delay_ms));
#else
    xhal_tick_ms_t start = xtime_get_tick_ms();
    while ((xtime_get_tick_ms() - start) < delay_ms)
    {
        __NOP();
    }
#endif
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
    uint32_t count =
        snprintf(time_str, buff_len, "%01u:%02u:%02u.%03u", hours, minutes,
                 seconds, (uint32_t)(xtime_sys_base_ms % 1000));

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

    if (xtime_base_ts == XTIME_INVALID_TS)
    {
        ret = XTIME_INVALID_TS; // 未设置基准时间
    }
    else
    {
        ret = xtime_base_ts + (xhal_ts_t)(xtime_sys_base_ms / 1000);
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
    /* 检查RTC时间是否有效 */
    if (xtime_base_ts != XTIME_INVALID_TS &&
        (ts + XTIME_ALLOWED_DRIFT) < xtime_base_ts)
    {
        return XHAL_ERR_INVALID;
    }
#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret_os = osOK;
    osMutexId_t mutex = _xtime_mutex();
    ret_os            = osMutexAcquire(mutex, osWaitForever);
    xassert(ret_os == osOK);
#endif
    /* 首次同步时间 */
    if (xtime_startup_ts == XTIME_INVALID_TS)
    {
        xtime_startup_ts = ts - (xtime_sys_base_ms / 1000);
        xtime_base_ts    = xtime_startup_ts;
        XLOG_INFO("RTC time first sync completed, base timestamp: %u",
                  xtime_base_ts);
    }
    else
    {
        /* 重新同步时间 */
        xtime_sys_base_ms = 0;
        xtime_base_ts     = ts;
        XLOG_INFO("RTC time resync completed, timestamp: %u", ts);
    }
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
    xtime_sys_base_ms++;
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
