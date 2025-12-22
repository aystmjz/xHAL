#include "xbuzzer.h"
#include "xhal_assert.h"
#include "xhal_log.h"

XLOG_TAG("xBUZZER");

#ifdef XHAL_OS_SUPPORTING
static const osMutexAttr_t xbuzzer_mutex_attr = {
    .name      = "xbuzzer_mutex",
    .attr_bits = osMutexPrioInherit | osMutexRecursive,
    .cb_mem    = NULL,
    .cb_size   = 0,
};
#endif

static inline void _lock(xbuzzer_t *buzzer);
static inline void _unlock(xbuzzer_t *buzzer);

xhal_err_t xbuzzer_init(xbuzzer_t *buzzer, const xbuzzer_ops_t *ops,
                        xbuzzer_state_t state, uint16_t frequency_hz,
                        uint16_t duty_percent)
{
    xassert_not_null(buzzer);
    xassert_not_null(ops);
    xassert_ptr_struct_not_null(ops, "xbuzzer_ops is null");

    buzzer->ops                = ops;
    buzzer->state              = state;
    buzzer->frequency_hz       = frequency_hz;
    buzzer->duty_cycle_percent = duty_percent;

#ifdef XHAL_OS_SUPPORTING
    buzzer->mutex = osMutexNew(&xbuzzer_mutex_attr);
    xassert_not_null(buzzer->mutex);
#endif

    xhal_err_t ret = XHAL_OK;

    ret = ops->set_frequency(buzzer, frequency_hz);
    if (ret != XHAL_OK)
    {
        goto exit;
    }

    ret = ops->set_duty_cycle(buzzer, duty_percent);
    if (ret != XHAL_OK)
    {
        goto exit;
    }

    ret = ops->set_state(buzzer, state);
    if (ret != XHAL_OK)
    {
        goto exit;
    }

    return ret;

exit:
#ifdef XHAL_OS_SUPPORTING
    osMutexDelete(buzzer->mutex);
#endif
    return ret;
}

xhal_err_t xbuzzer_deinit(xbuzzer_t *buzzer)
{
    xassert_not_null(buzzer);

    if (!buzzer->ops)
    {
        return XHAL_ERR_NO_INIT;
    }

    xbuzzer_off(buzzer);

    buzzer->ops = NULL;

#ifdef XHAL_OS_SUPPORTING
    if (buzzer->mutex)
    {
        osStatus_t ret_os = osMutexDelete(buzzer->mutex);
        if (ret_os != osOK)
        {
            return XHAL_ERROR;
        }
        buzzer->mutex = NULL;
    }
#endif

    return XHAL_OK;
}

xhal_err_t xbuzzer_on(xbuzzer_t *buzzer)
{
    return xbuzzer_set_state(buzzer, XBUZZER_STATE_ON);
}

xhal_err_t xbuzzer_off(xbuzzer_t *buzzer)
{
    return xbuzzer_set_state(buzzer, XBUZZER_STATE_OFF);
}

xhal_err_t xbuzzer_set_state(xbuzzer_t *buzzer, xbuzzer_state_t state)
{
    xassert_not_null(buzzer);

    if (!buzzer->ops)
    {
        return XHAL_ERR_NO_INIT;
    }

    xhal_err_t ret = XHAL_OK;

    _lock(buzzer);
    if (buzzer->state != state)
    {
        ret = buzzer->ops->set_state(buzzer, state);
        if (ret == XHAL_OK)
        {
            buzzer->state = state;
        }
    }
    _unlock(buzzer);

    return ret;
}

xhal_err_t xbuzzer_get_state(xbuzzer_t *buzzer, xbuzzer_state_t *state)
{
    xassert_not_null(buzzer);
    xassert_not_null(state);

    if (!buzzer->ops)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(buzzer);
    *state = buzzer->state;
    _unlock(buzzer);

    return XHAL_OK;
}

xhal_err_t xbuzzer_set_frequency(xbuzzer_t *buzzer, uint16_t frequency_hz)
{
    xassert_not_null(buzzer);

    if (!buzzer->ops)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(buzzer);
    xhal_err_t ret = buzzer->ops->set_frequency(buzzer, frequency_hz);
    if (ret == XHAL_OK)
    {
        buzzer->frequency_hz = frequency_hz;
    }
    _unlock(buzzer);

    return ret;
}

xhal_err_t xbuzzer_get_frequency(xbuzzer_t *buzzer, uint16_t *frequency_hz)
{
    xassert_not_null(buzzer);
    xassert_not_null(frequency_hz);

    if (!buzzer->ops)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(buzzer);
    *frequency_hz = buzzer->frequency_hz;
    _unlock(buzzer);

    return XHAL_OK;
}

xhal_err_t xbuzzer_set_duty_cycle(xbuzzer_t *buzzer, uint16_t duty_percent)
{
    xassert_not_null(buzzer);

    if (!buzzer->ops)
    {
        return XHAL_ERR_NO_INIT;
    }

    if (duty_percent > 100)
    {
        return XHAL_ERR_INVALID;
    }

    _lock(buzzer);
    xhal_err_t ret = buzzer->ops->set_duty_cycle(buzzer, duty_percent);
    if (ret == XHAL_OK)
    {
        buzzer->duty_cycle_percent = duty_percent;
    }
    _unlock(buzzer);

    return ret;
}

xhal_err_t xbuzzer_get_duty_cycle(xbuzzer_t *buzzer, uint16_t *duty_percent)
{
    xassert_not_null(buzzer);
    xassert_not_null(duty_percent);

    if (!buzzer->ops)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(buzzer);
    *duty_percent = buzzer->duty_cycle_percent;
    _unlock(buzzer);

    return XHAL_OK;
}

static inline void _lock(xbuzzer_t *buzzer)
{
#ifdef XHAL_OS_SUPPORTING
    if (buzzer->mutex)
    {
        osStatus_t ret = osMutexAcquire(buzzer->mutex, osWaitForever);
        xassert(ret == osOK);
    }
#endif
}

static inline void _unlock(xbuzzer_t *buzzer)
{
#ifdef XHAL_OS_SUPPORTING
    if (buzzer->mutex)
    {
        osStatus_t ret = osMutexRelease(buzzer->mutex);
        xassert(ret == osOK);
    }
#endif
}
