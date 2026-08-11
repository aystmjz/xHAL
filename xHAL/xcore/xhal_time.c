#include "xhal_time.h"
#include "xhal_assert.h"
#include "xhal_log.h"
#include <stdio.h>
#include XHAL_DEVICE_HEADER

XHAL_TAG(xTime);

#ifndef XTIME_USE_DWT_DELAY
    #define XTIME_USE_DWT_DELAY 0
#endif

#if XTIME_USE_DWT_DELAY != 0
    #if !defined(DWT) || !defined(DWT_CTRL_CYCCNTENA_Msk)
        #error \
            "DWT cycle counter is not available on this MCU, disable XTIME_USE_DWT_DELAY"
    #endif
#endif

#ifndef XTIME_NOP
    #error "Please define XTIME_NOP() macro in xhal_config.h"
#endif

#ifndef XTIME_AUTO_SYNC_ENABLE
    #define XTIME_AUTO_SYNC_ENABLE 1
#endif

#ifndef XHAL_CPU_FREQ_HZ
    #error "Please define XHAL_CPU_FREQ_HZ"
#endif

#if (XHAL_OS_SUPPORTING == 1)
    #include "../xos/xhal_os.h"

static osMutexId_t _get_xtime_mutex(void);
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

static volatile uint32_t xtime_uptime_seq         = 0;

static xhal_tick_t xtime_sync_tick_ms = 0;
static xhal_ts_t xtime_base_ts        = XTIME_INVALID_TS;

xhal_tick_t xtime_get_tick_ms(void)
{
    return xtime_sys_tick_ms;
}

xhal_uptime_t xtime_get_uptime_ms(void)
{
    uint32_t seq1, seq2;
    xhal_uptime_t up;

    do
    {
        seq1 = xtime_uptime_seq;    /* 进入临界读区 */
        up   = xtime_sys_uptime_ms; /* 64 位非原子读取 */
        seq2 = xtime_uptime_seq;    /* 退出临界读区 */
    } while (((seq1 & 1U) != 0U) || (seq1 != seq2));

    return up;
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

    uint32_t clk   = (XHAL_CPU_FREQ_HZ / 1000000U);
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = delay_us * clk;

    while ((DWT->CYCCNT - start) < ticks)
    {
    }
#else
    uint32_t count = delay_us * (XHAL_CPU_FREQ_HZ / 8U / 1000000U);
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

#if (XHAL_OS_SUPPORTING == 1)
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

static inline bool _is_leap_year(uint16_t year)
{
    /* 公历闰年规则 */
    return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

uint8_t xtime_days_in_month(uint16_t year, uint8_t month)
{
    const uint8_t days_table[12] = {31, 28, 31, 30, 31, 30,
                                    31, 31, 30, 31, 30, 31};

    if (month == 0 || month > 12)
    {
        return 0;
    }

    if (month == 2 && _is_leap_year(year))
    {
        return 29;
    }

    return days_table[month - 1];
}

static inline uint8_t _calc_weekday(uint16_t year, uint8_t month, uint8_t day)
{
    if (month < 3)
    {
        month += 12;
        year -= 1;
    }

    uint8_t k = year % 100;
    uint8_t j = year / 100;

    /*  Zeller公式 */
    uint8_t h = (day + 13 * (month + 1) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;

    return (uint8_t)((h + 6) % 7);
}

bool xtime_is_valid_time(const xhal_time_t *time)
{
    if (time->hour > 23)
        return false;
    if (time->minute > 59)
        return false;
    if (time->second > 59)
        return false;
    return true;
}

bool xtime_is_valid_date(const xhal_time_t *time)
{
    if (time == NULL)
    {
        return false;
    }

    if (time->year < 1970 || time->year > 2099)
    {
        return false;
    }

    if (time->month < 1 || time->month > 12)
    {
        return false;
    }

    uint8_t max_day = xtime_days_in_month(time->year, time->month);
    if (time->day < 1 || time->day > max_day)
    {
        return false;
    }

    return true;
}

xhal_err_t xtime_adjust_weekday(xhal_time_t *time)
{
    xassert_not_null(time);

    if (!xtime_is_valid_date(time))
    {
        return XHAL_ERR_INVALID;
    }

    time->weekday = _calc_weekday(time->year, time->month, time->day);

    return XHAL_OK;
}

xhal_err_t xtime_timestamp_to_time(xhal_ts_t ts, xhal_time_t *time)
{
    xassert_not_null(time);

    struct tm timeinfo;

    if (localtime_r(&ts, &timeinfo) == NULL)
    {
        return XHAL_ERR_INVALID;
    }

    time->year    = (uint16_t)(timeinfo.tm_year + 1900);
    time->month   = (uint8_t)(timeinfo.tm_mon + 1);
    time->day     = (uint8_t)timeinfo.tm_mday;
    time->hour    = (uint8_t)timeinfo.tm_hour;
    time->minute  = (uint8_t)timeinfo.tm_min;
    time->second  = (uint8_t)timeinfo.tm_sec;
    time->weekday = (uint8_t)timeinfo.tm_wday; /* 0=Sunday */

    return XHAL_OK;
}

xhal_err_t xtime_time_to_timestamp(xhal_time_t *time, xhal_ts_t *ts)
{
    xassert_not_null(time);

    if (!xtime_is_valid_date(time) || !xtime_is_valid_time(time))
    {
        return XHAL_ERR_INVALID;
    }

    struct tm timeinfo;
    timeinfo.tm_year  = time->year - 1900;
    timeinfo.tm_mon   = time->month - 1;
    timeinfo.tm_mday  = time->day;
    timeinfo.tm_hour  = time->hour;
    timeinfo.tm_min   = time->minute;
    timeinfo.tm_sec   = time->second;
    timeinfo.tm_isdst = 0;

    time_t timestamp = mktime(&timeinfo);
    if (timestamp == (time_t)(-1))
    {
        return XHAL_ERR_INVALID;
    }

    time->weekday = (uint8_t)timeinfo.tm_wday;

    *ts = (xhal_ts_t)timestamp;

    return XHAL_OK;
}

xhal_err_t xtime_get_time(xhal_time_t *time)
{
    xassert_not_null(time);

    xhal_ts_t ts = xtime_get_ts();

    if (ts == XTIME_INVALID_TS)
    {
        return XHAL_ERR_NO_INIT;
    }

    return xtime_timestamp_to_time(ts, time);
}

/**
 * @brief  获取当前时间戳
 * @retval 当前时间戳，如果未设置基准时间则返回无效时间戳
 */
xhal_ts_t xtime_get_ts(void)
{
#if (XHAL_OS_SUPPORTING == 1)
    osStatus_t ret_os = osOK;
    osMutexId_t mutex = _get_xtime_mutex();
    ret_os            = osMutexAcquire(mutex, osWaitForever);
    if (ret_os != osOK)
    {
        return XTIME_INVALID_TS;
    }
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
#if (XHAL_OS_SUPPORTING == 1)
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

#if (XHAL_OS_SUPPORTING == 1)
    osStatus_t ret_os = osOK;
    osMutexId_t mutex = _get_xtime_mutex();
    ret_os            = osMutexAcquire(mutex, osWaitForever);
    if (ret_os != osOK)
    {
        return (xhal_err_t)ret_os;
    }
#endif

    xtime_sync_tick_ms = xtime_sys_tick_ms;
    xtime_base_ts      = ts;

    // XLOG_INFO("RTC time resync completed, timestamp: %lu", (unsigned
    // long)ts);

#if (XHAL_OS_SUPPORTING == 1)
    ret_os = osMutexRelease(mutex);
    xassert(ret_os == osOK);
#endif
    return XHAL_OK;
}

/**
 * @brief  SysTick毫秒中断处理函数
 *
 * @note   OS 模式下，SysTick 中断周期为 1000/XOS_TICK_RATE_HZ ms，
 *         并非固定 1ms，因此这里按实际周期累加毫秒计数；
 *         非 OS 模式下 SysTick 固定配置为 1ms，仍按 1ms 累加。
 */
void xtime_ms_tick_handler(void)
{
    xhal_tick_t delta_ms;

#if (XHAL_OS_SUPPORTING == 1)
    /* OS 模式：每次中断对应 1000/XOS_TICK_RATE_HZ ms */
    #if (1000 % XOS_TICK_RATE_HZ) == 0
    /* tick 周期为整数毫秒，直接累加 */
    delta_ms = (xhal_tick_t)(1000 / XOS_TICK_RATE_HZ);
    #else
    /* tick 周期不是整数毫秒：用余数累加保持长期精度。
     * 每次中断向余数累加器加 1000，整除 TICK_RATE_HZ 得到本次应增毫秒数，
     * 平均周期仍精确等于 1000/XOS_TICK_RATE_HZ ms。 */
    static uint32_t remainder_ms = 0;
    remainder_ms += 1000U;
    delta_ms = (xhal_tick_t)(remainder_ms / (uint32_t)XOS_TICK_RATE_HZ);
    remainder_ms %= (uint32_t)XOS_TICK_RATE_HZ;
    #endif
#else
    /* 非 OS 模式：SysTick 固定为 1ms */
    delta_ms = 1;
#endif

    xtime_sys_tick_ms += delta_ms;

    xtime_uptime_seq++;
    xtime_sys_uptime_ms += (xhal_uptime_t)delta_ms;
    xtime_uptime_seq++;
}

#if (XHAL_OS_SUPPORTING == 1)
static osMutexId_t _get_xtime_mutex(void)
{
    if (xtime_mutex == NULL)
    {
        xtime_mutex = osMutexNew(&xtime_mutex_attr);
    }

    return xtime_mutex;
}
#endif
