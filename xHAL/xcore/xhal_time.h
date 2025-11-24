#ifndef __XHAL_TIME_H
#define __XHAL_TIME_H

#include "xhal_config.h"
#include "xhal_def.h"
#include "xhal_std.h"
#include <time.h>

#ifdef XHAL_OS_SUPPORTING
#if (XOS_TICK_RATE_HZ == 0)
#error "XOS_TICK_RATE_HZ must not be 0"
#endif

#if (1000 % XOS_TICK_RATE_HZ) != 0
#warning \
    "XOS_TICK_RATE_HZ does not evenly divide 1000, delay may lose precision, using 64-bit math"
#endif

#define XOS_MS_TO_TICKS(ms) \
    ((uint32_t)(((uint64_t)(ms) * (uint64_t)XOS_TICK_RATE_HZ) / 1000ULL))
#define XOS_TICKS_TO_MS(ticks) \
    ((uint32_t)(((uint64_t)(ticks) * 1000ULL) / (uint64_t)XOS_TICK_RATE_HZ))
#endif

typedef uint64_t xhal_tick_ms_t;  // Tick 类型，毫秒
typedef uint32_t xhal_tick_sec_t; // Tick 类型，秒
typedef time_t xhal_ts_t;         // 时间戳类型，绝对时间
typedef xhal_ts_t (*rtc_get_ts_func_t)(void);

xhal_tick_ms_t xtime_get_tick_ms(void);
xhal_tick_sec_t xtime_get_tick_sec(void);

void xtime_delay_us(uint32_t delay_us);
void xtime_delay_ms(uint32_t delay_ms);
void xtime_delay_s(uint32_t delay_s);

xhal_ts_t xtime_get_ts(void);
xhal_err_t xtime_get_format_uptime(char *time_str, uint8_t str_len);
xhal_err_t xtime_get_format_time(char *time_str, uint8_t str_len);
xhal_err_t xtime_sync_time(xhal_ts_t ts);

void xtime_ms_tick_handler(void);

#endif /* __XHAL_TIME_H */