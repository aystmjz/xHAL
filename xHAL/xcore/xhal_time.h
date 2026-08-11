#ifndef __XHAL_TIME_H
#define __XHAL_TIME_H

#include "xhal_config.h"
#include "xhal_def.h"
#include <time.h>

#define XTIME_INVALID_TS          (0)

#define TIME_AFTER(a, b)          ((int32_t)((b) - (a)) < 0)
#define TIME_BEFOR(a, b)          TIME_AFTER(b, a)
#define TIME_AFTER_EQ(a, b)       ((int32_t)((a) - (b)) >= 0)
#define TIME_BEFOR_EQ(a, b)       TIME_AFTER_EQ(b, a)
#define TIME_DIFF(later, earlier) ((xhal_tick_t)((later) - (earlier)))

#if (XHAL_OS_SUPPORTING == 1)
    #if (XOS_TICK_RATE_HZ == 0)
        #error "XOS_TICK_RATE_HZ must not be 0"
    #endif

    #if (1000 % XOS_TICK_RATE_HZ) != 0
        #warning \
            "XOS_TICK_RATE_HZ does not evenly divide 1000, delay may lose precision, using 64-bit math"
    #endif

    #define XOS_MS_TO_TICKS(ms) \
        ((xhal_tick_t)(((uint64_t)(ms) * (uint64_t)XOS_TICK_RATE_HZ) / 1000ULL))
    #define XOS_TICKS_TO_MS(ticks)                     \
        ((xhal_tick_t)(((uint64_t)(ticks) * 1000ULL) / \
                       (uint64_t)XOS_TICK_RATE_HZ))
#endif

typedef struct xhal_time
{
    uint16_t year;   /* 年份 (2000-2099) */
    uint8_t month;   /* 月份 (1-12) */
    uint8_t day;     /* 日期 (1-31) */
    uint8_t hour;    /* 小时 (0-23) */
    uint8_t minute;  /* 分钟 (0-59) */
    uint8_t second;  /* 秒 (0-59) */
    uint8_t weekday; /* 星期 (0-6, 0=Sunday) */
} xhal_time_t;

typedef uint32_t xhal_tick_t;
typedef uint64_t xhal_uptime_t;
typedef time_t xhal_ts_t;

xhal_tick_t xtime_get_tick_ms(void);
xhal_uptime_t xtime_get_uptime_ms(void);

void xtime_delay_us(uint32_t delay_us);
void xtime_delay_ms(uint32_t delay_ms);
void xtime_delay_s(uint32_t delay_s);

xhal_ts_t xtime_get_ts(void);
xhal_err_t xtime_get_time(xhal_time_t *time);
xhal_err_t xtime_get_format_uptime(char *time_str, uint32_t buff_len);
xhal_err_t xtime_get_format_time(char *time_str, uint32_t buff_len);
xhal_err_t xtime_sync_time(xhal_ts_t ts);

uint8_t xtime_days_in_month(uint16_t year, uint8_t month);
bool xtime_is_valid_time(const xhal_time_t *time);
bool xtime_is_valid_date(const xhal_time_t *time);
xhal_err_t xtime_adjust_weekday(xhal_time_t *time);
xhal_err_t xtime_timestamp_to_time(xhal_ts_t ts, xhal_time_t *time);
xhal_err_t xtime_time_to_timestamp(xhal_time_t *time, xhal_ts_t *ts);

void xtime_ms_tick_handler(void);

#endif /* __XHAL_TIME_H */