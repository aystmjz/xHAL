#include "xrtc.h"
#include "xhal_assert.h"
#include "xhal_log.h"

XLOG_TAG("xRTC");

#ifdef XHAL_OS_SUPPORTING
static const osMutexAttr_t xrtc_mutex_attr = {
    .name      = "xrtc_mutex",
    .attr_bits = osMutexPrioInherit | osMutexRecursive,
    .cb_mem    = NULL,
    .cb_size   = 0,
};
#endif

static inline void _lock(xrtc_t *rtc);
static inline void _unlock(xrtc_t *rtc);

xhal_err_t xrtc_init(xrtc_t *rtc, const xrtc_ops_t *ops, void *inst)
{
    xassert_not_null(rtc);
    xassert_not_null(inst);
    xassert_ptr_struct_not_null(ops, "xrtc_ops is null");

    rtc->inst = inst;
    rtc->ops  = ops;

#ifdef XHAL_OS_SUPPORTING
    rtc->mutex = osMutexNew(&xrtc_mutex_attr);
    xassert_not_null(rtc->mutex);
#endif

    xhal_err_t ret = ops->init(inst);
    if (ret != XHAL_OK)
    {
#ifdef XHAL_OS_SUPPORTING
        osMutexDelete(sensor->mutex);
#endif
        return ret;
    }

    return XHAL_OK;
}

xhal_err_t xrtc_deinit(xrtc_t *rtc)
{
    xassert_not_null(rtc);

    if (rtc->ops == NULL)
    {
        return XHAL_ERR_NO_INIT;
    }

    xhal_err_t ret = rtc->ops->deinit(rtc->inst);

#ifdef XHAL_OS_SUPPORTING
    osMutexDelete(rtc->mutex);
    rtc->mutex = NULL;
#endif

    rtc->ops  = NULL;
    rtc->inst = NULL;

    return ret;
}

xhal_err_t xrtc_get_time(xrtc_t *rtc, xhal_time_t *time, uint32_t timeout_ms)
{
    xassert_not_null(rtc);
    xassert_not_null(time);

    if (rtc->ops == NULL)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(rtc);
    xhal_err_t ret = rtc->ops->get_time(rtc->inst, time, timeout_ms);
    _unlock(rtc);

    return ret;
}

xhal_err_t xrtc_set_time(xrtc_t *rtc, const xhal_time_t *time,
                         uint32_t timeout_ms)
{
    xassert_not_null(rtc);
    xassert_not_null(time);

    if (rtc->ops == NULL)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(rtc);
    xhal_err_t ret = rtc->ops->set_time(rtc->inst, time, timeout_ms);
    _unlock(rtc);

    return ret;
}

xhal_err_t xrtc_get_timestamp(xrtc_t *rtc, xhal_ts_t *ts, uint32_t timeout_ms)
{
    xassert_not_null(rtc);
    xassert_not_null(ts);

    xhal_time_t time;

    xhal_err_t ret = xrtc_get_time(rtc, &time, timeout_ms);
    if (ret != XHAL_OK)
    {
        return ret;
    }

    return xtime_time_to_timestamp(&time, ts);
}

xhal_err_t xrtc_set_timestamp(xrtc_t *rtc, xhal_ts_t ts, uint32_t timeout_ms)
{
    xassert_not_null(rtc);

    xhal_time_t time;
    xhal_err_t ret = xtime_timestamp_to_time(ts, &time);
    if (ret != XHAL_OK)
    {
        return ret;
    }

    return xrtc_set_time(rtc, &time, timeout_ms);
}

xhal_err_t xrtc_set_alarm(xrtc_t *rtc, const xrtc_alarm_t *alarm, uint8_t id,
                          uint32_t timeout_ms)
{
    xassert_not_null(rtc);
    xassert_not_null(alarm);

    if (rtc->ops == NULL)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(rtc);
    xhal_err_t ret = rtc->ops->set_alarm(rtc->inst, alarm, id, timeout_ms);
    _unlock(rtc);

    return ret;
}

xhal_err_t xrtc_get_alarm(xrtc_t *rtc, xrtc_alarm_t *alarm, uint8_t id,
                          uint32_t timeout_ms)
{
    xassert_not_null(rtc);
    xassert_not_null(alarm);

    if (rtc->ops == NULL)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(rtc);
    xhal_err_t ret = rtc->ops->get_alarm(rtc->inst, alarm, id, timeout_ms);
    _unlock(rtc);

    return ret;
}

xhal_err_t xrtc_clear_alarm(xrtc_t *rtc, uint8_t id, uint32_t timeout_ms)
{
    xassert_not_null(rtc);

    if (rtc->ops == NULL)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(rtc);
    xhal_err_t ret = rtc->ops->clear_alarm(rtc->inst, id, timeout_ms);
    _unlock(rtc);

    return ret;
}

xhal_err_t xrtc_get_alarm_status(xrtc_t *rtc, uint8_t id,
                                 xrtc_alarm_state_t *state, uint32_t timeout_ms)
{
    xassert_not_null(rtc);
    xassert_not_null(state);

    if (rtc->ops == NULL)
    {
        return XHAL_ERR_NO_INIT;
    }

    xrtc_alarm_t alarm;

    xhal_err_t ret = xrtc_get_alarm(rtc, &alarm, id, timeout_ms);
    if (ret == XHAL_OK)
    {
        *state = alarm.state;
    }

    return ret;
}

static inline void _lock(xrtc_t *rtc)
{
#ifdef XHAL_OS_SUPPORTING
    if (rtc->mutex != NULL)
    {
        osStatus_t ret = osMutexAcquire(rtc->mutex, osWaitForever);
        xassert(ret == osOK);
    }
#endif
}

static inline void _unlock(xrtc_t *rtc)
{
#ifdef XHAL_OS_SUPPORTING
    if (rtc->mutex != NULL)
    {
        osStatus_t ret = osMutexRelease(rtc->mutex);
        xassert(ret == osOK);
    }
#endif
}
