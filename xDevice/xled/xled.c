#include "xled.h"
#include "xhal_assert.h"
#include "xhal_log.h"

XLOG_TAG("xLED");

#ifdef XHAL_OS_SUPPORTING
static const osMutexAttr_t xled_mutex_attr = {
    .name      = "xled_mutex",
    .attr_bits = osMutexPrioInherit | osMutexRecursive,
    .cb_mem    = NULL,
    .cb_size   = 0,
};
#endif

static inline void _lock(xled_t *led);
static inline void _unlock(xled_t *led);

xhal_err_t xled_init(xled_t *led, const xled_ops_t *ops, xled_state_t state)
{
    xassert_not_null(led);
    xassert_not_null(ops);
    xassert_ptr_struct_not_null(ops, "xled_ops is null");

    led->ops   = ops;
    led->state = state;

#ifdef XHAL_OS_SUPPORTING
    led->mutex = osMutexNew(&xled_mutex_attr);
    xassert_not_null(led->mutex);
#endif

    xhal_err_t ret = ops->set_state(led, state);
    if (ret != XHAL_OK)
    {
#ifdef XHAL_OS_SUPPORTING
        osMutexDelete(led->mutex);
#endif
        return ret;
    }

    return XHAL_OK;
}

xhal_err_t xled_deinit(xled_t *led)
{
    xassert_not_null(led);

    if (!led->ops)
    {
        return XHAL_ERR_NO_INIT;
    }

    led->ops = NULL;

#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret_os = osMutexDelete(led->mutex);
    if (ret_os != osOK)
    {
        return XHAL_ERROR;
    }
    led->mutex = NULL;
#endif

    return XHAL_OK;
}

xhal_err_t xled_on(xled_t *led)
{
    xassert_not_null(led);

    if (!led->ops)
    {
        return XHAL_ERR_NO_INIT;
    }

    xhal_err_t ret = XHAL_OK;

    _lock(led);
    if (led->state != XLED_STATE_ON)
    {
        ret = led->ops->set_state(led, XLED_STATE_ON);
        if (ret == XHAL_OK)
        {
            led->state = XLED_STATE_ON;
        }
    }
    _unlock(led);

    return ret;
}

xhal_err_t xled_off(xled_t *led)
{
    xassert_not_null(led);

    if (!led->ops)
    {
        return XHAL_ERR_NO_INIT;
    }

    xhal_err_t ret = XHAL_OK;

    _lock(led);
    if (led->state != XLED_STATE_OFF)
    {
        ret = led->ops->set_state(led, XLED_STATE_OFF);
        if (ret == XHAL_OK)
        {
            led->state = XLED_STATE_OFF;
        }
    }
    _unlock(led);

    return ret;
}

xhal_err_t xled_toggle(xled_t *led)
{
    xassert_not_null(led);

    if (!led->ops)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(led);
    xhal_err_t ret = led->ops->toggle(led);
    if (ret == XHAL_OK)
    {
        xled_state_t state;
        ret = led->ops->get_state(led, &state);
        if (ret == XHAL_OK)
        {
            led->state = state;
        }
    }
    _unlock(led);

    return ret;
}

xhal_err_t xled_get_state(xled_t *led, xled_state_t *state)
{
    xassert_not_null(led);
    xassert_not_null(state);

    if (!led->ops)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(led);
    xhal_err_t ret = led->ops->get_state(led, state);
    if (ret == XHAL_OK)
    {
        led->state = *state;
    }
    _unlock(led);

    return ret;
}

xhal_err_t xled_set_state(xled_t *led, xled_state_t state)
{
    if (state == XLED_STATE_ON)
    {
        return xled_on(led);
    }
    else
    {
        return xled_off(led);
    }
}

static inline void _lock(xled_t *led)
{
#ifdef XHAL_OS_SUPPORTING

    osStatus_t ret = osOK;
    ret            = osMutexAcquire(led->mutex, osWaitForever);
    xassert(ret == osOK);
#endif
}

static inline void _unlock(xled_t *led)
{
#ifdef XHAL_OS_SUPPORTING

    osStatus_t ret = osOK;
    ret            = osMutexRelease(led->mutex);
    xassert(ret == osOK);
#endif
}