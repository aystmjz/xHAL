#ifndef __XHAL_TIME_H
#define __XHAL_TIME_H

#include "xhal_config.h"
#include "xhal_def.h"
#include "xhal_std.h"
#include <time.h>

#define TIME_AFTER(a, b)          ((int64_t)((b) - (a)) < 0)
#define TIME_BEFOR(a, b)          TIME_AFTER(b, a)
#define TIME_AFTER_EQ(a, b)       ((int64_t)((a) - (b)) >= 0)
#define TIME_BEFOR_EQ(a, b)       TIME_AFTER_EQ(b, a)
#define TIME_DIFF(later, earlier) ((xhal_tick_t)((later) - (earlier)))

#ifdef XHAL_OS_SUPPORTING
#if (XOS_TICK_RATE_HZ == 0)
#error "XOS_TICK_RATE_HZ must not be 0"
#endif

#if (1000 % XOS_TICK_RATE_HZ) != 0
#warning \
    "XOS_TICK_RATE_HZ does not evenly divide 1000, delay may lose precision, using 64-bit math"
#endif

#define XOS_MS_TO_TICKS(ms) \
    ((xhal_tick_t)(((uint64_t)(ms) * (uint64_t)XOS_TICK_RATE_HZ) / 1000ULL))
#define XOS_TICKS_TO_MS(ticks) \
    ((xhal_tick_t)(((uint64_t)(ticks) * 1000ULL) / (uint64_t)XOS_TICK_RATE_HZ))
#endif

typedef uint32_t xhal_tick_t;
typedef uint64_t xhal_uptime_t;
typedef time_t xhal_ts_t;

xhal_tick_t xtime_get_tick_ms(void);
xhal_uptime_t xtime_get_uptime_ms(void);

void xtime_delay_us(uint32_t delay_us);
void xtime_delay_ms(uint32_t delay_ms);
void xtime_delay_s(uint32_t delay_s);

xhal_ts_t xtime_get_ts(void);
xhal_err_t xtime_get_format_uptime(char *time_str, uint32_t buff_len);
xhal_err_t xtime_get_format_time(char *time_str, uint32_t buff_len);
xhal_err_t xtime_sync_time(xhal_ts_t ts);

void xtime_ms_tick_handler(void);

#endif /* __XHAL_TIME_H */