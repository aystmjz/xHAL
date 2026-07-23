#ifndef __XRTC_H
#define __XRTC_H

#include "xhal_coro.h"
#include "xhal_def.h"
#include "xhal_os.h"
#include "xhal_ringbuf.h"

typedef enum
{
    XRTC_ALARM_EVERY_SECOND = 0,
    XRTC_ALARM_MATCH_SECOND,
    XRTC_ALARM_MATCH_MIN_SEC,
    XRTC_ALARM_MATCH_HOUR_MIN_SEC,
    XRTC_ALARM_MATCH_DATE_TIME,
    XRTC_ALARM_MATCH_WEEK_TIME,
} xrtc_alarm_mode_t;

typedef struct xrtc_alarm_time
{
    uint8_t second;  /* 0–59 */
    uint8_t minute;  /* 0–59 */
    uint8_t hour;    /* 0–23 */
    uint8_t day;     /* 1–31 */
    uint8_t weekday; /* 0–6, 0=Sunday */
} xrtc_alarm_time_t;

typedef struct xrtc_alarm_state
{
    uint8_t enable;     /* 闹钟是否启用 */
    uint8_t fired;      /* 是否已触发 */
} xrtc_alarm_state_t;

typedef struct xrtc_alarm
{
    xrtc_alarm_state_t state;
    xrtc_alarm_mode_t mode;
    xrtc_alarm_time_t time;
} xrtc_alarm_t;

typedef struct xrtc xrtc_t;

typedef struct xrtc_ops
{
    xhal_err_t (*init)(void *inst);
    xhal_err_t (*deinit)(void *inst);

    xhal_err_t (*set_time)(void *inst, const xhal_time_t *time,
                           uint32_t timeout_ms);
    xhal_err_t (*get_time)(void *inst, xhal_time_t *time, uint32_t timeout_ms);

    xhal_err_t (*set_alarm)(void *inst, const xrtc_alarm_t *alarm, uint8_t id,
                            uint32_t timeout_ms);
    xhal_err_t (*get_alarm)(void *inst, xrtc_alarm_t *alarm, uint8_t id,
                            uint32_t timeout_ms);
    xhal_err_t (*clear_alarm)(void *inst, uint8_t id, uint32_t timeout_ms);
} xrtc_ops_t;

typedef struct xrtc
{
    void *inst;
    const xrtc_ops_t *ops;

#ifdef XHAL_OS_SUPPORTING
    osMutexId_t mutex;
#endif
} xrtc_t;

xhal_err_t xrtc_init(xrtc_t *rtc, const xrtc_ops_t *ops, void *inst);
xhal_err_t xrtc_deinit(xrtc_t *rtc);

xhal_err_t xrtc_get_time(xrtc_t *rtc, xhal_time_t *time, uint32_t timeout_ms);
xhal_err_t xrtc_set_time(xrtc_t *rtc, const xhal_time_t *time,
                         uint32_t timeout_ms);
xhal_err_t xrtc_get_timestamp(xrtc_t *rtc, xhal_ts_t *ts, uint32_t timeout_ms);
xhal_err_t xrtc_set_timestamp(xrtc_t *rtc, xhal_ts_t ts, uint32_t timeout_ms);

xhal_err_t xrtc_set_alarm(xrtc_t *rtc, const xrtc_alarm_t *alarm, uint8_t id,
                          uint32_t timeout_ms);
xhal_err_t xrtc_get_alarm(xrtc_t *rtc, xrtc_alarm_t *alarm, uint8_t id,
                          uint32_t timeout_ms);
xhal_err_t xrtc_clear_alarm(xrtc_t *rtc, uint8_t id, uint32_t timeout_ms);
xhal_err_t xrtc_get_alarm_status(xrtc_t *rtc, uint8_t id,
                                 xrtc_alarm_state_t *state,
                                 uint32_t timeout_ms);
#endif /* __XRTC_H */
