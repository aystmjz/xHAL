#ifndef __XHAL_TIME_H
#define __XHAL_TIME_H

#include "xhal_config.h"
#include "xhal_def.h"
#include "xhal_std.h"
#include <time.h>

#define XTIME_DELAY_US_SUPPORTING

typedef uint64_t xhal_tick_ms_t;  // Tick 类型，毫秒
typedef uint32_t xhal_tick_sec_t; // Tick 类型，秒
typedef time_t xhal_ts_t;         // 时间戳类型，绝对时间
typedef xhal_ts_t (*rtc_get_ts_func_t)(void);

xhal_tick_ms_t xtime_get_tick_ms(void);
xhal_tick_sec_t xtime_get_tick_sec(void);

void xtime_delay_ms(uint32_t delay_ms);
void xtime_delay_s(uint32_t delay_s);

void xtime_set_rtc_get_ts_func(rtc_get_ts_func_t func);

xhal_ts_t xtime_get_ts(void);
xhal_err_t xtime_get_format_uptime(char *time_str, uint8_t str_len);
xhal_err_t xtime_get_format_time(char *time_str, uint8_t str_len);
xhal_err_t xtime_sync_time_from_rtc(void);

void xtime_ms_tick_handler(void);

#endif /* __XHAL_TIME_H */