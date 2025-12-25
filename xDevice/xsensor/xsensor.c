#include "xsensor.h"
#include "xhal_assert.h"
#include "xhal_log.h"

XLOG_TAG("xSENSOR");

#define XSENSOR_EVENT (1U << 0)

#ifdef XHAL_OS_SUPPORTING
static const osMutexAttr_t xsensor_mutex_attr = {
    .name      = "xsensor_mutex",
    .attr_bits = osMutexPrioInherit | osMutexRecursive,
    .cb_mem    = NULL,
    .cb_size   = 0,
};
#endif

static xhal_err_t _post_event(xsensor_t *sensor, xsensor_event_type_t type,
                              xsensor_cb_t cb, uint32_t timeout_ms);
static inline void _lock(xsensor_t *sensor);
static inline void _unlock(xsensor_t *sensor);

xhal_err_t xsensor_init(xsensor_t *sensor, const xsensor_ops_t *ops, void *inst)
{
    xassert_not_null(sensor);
    xassert_not_null(ops);
    xassert_not_null(inst);
    xassert_ptr_struct_not_null(ops, "xsensor_ops is null");

    xcoro_event_init(&sensor->event);
    xrbuf_init(&sensor->evt_rb, sensor->evt_buff, sizeof(sensor->evt_buff));

    sensor->ops  = ops;
    sensor->inst = inst;

#ifdef XHAL_OS_SUPPORTING
    sensor->mutex = osMutexNew(&xsensor_mutex_attr);
    xassert_not_null(sensor->mutex);
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

xhal_err_t xsensor_deinit(xsensor_t *sensor)
{
    xassert_not_null(sensor);

    if (sensor->ops == NULL)
    {
        return XHAL_ERR_NO_INIT;
    }

    xhal_err_t ret = sensor->ops->deinit(sensor->inst);

    xrbuf_free(&sensor->evt_rb);
    sensor->ops  = NULL;
    sensor->inst = NULL;

#ifdef XHAL_OS_SUPPORTING
    osMutexDelete(sensor->mutex);
    sensor->mutex = NULL;
#endif

    return ret;
}

xhal_err_t xsensor_reset(xsensor_t *sensor, xsensor_cb_t cb,
                         uint32_t timeout_ms)
{
    xassert_not_null(sensor);

    if (sensor->ops == NULL)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(sensor);
    xhal_err_t ret = _post_event(sensor, XSENSOR_RESET, cb, timeout_ms);
    _lock(sensor);

    return ret;
}

xhal_err_t xsensor_read(xsensor_t *sensor, xsensor_cb_t cb, uint32_t timeout_ms)
{
    xassert_not_null(sensor);

    if (sensor->ops == NULL)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(sensor);
    xhal_err_t ret = _post_event(sensor, XSENSOR_READ, cb, timeout_ms);
    _unlock(sensor);

    return ret;
}

void xsensor_handler_thread(xcoro_handle_t *handle, xsensor_t *sensor)
{
    xassert_not_null(handle);
    xassert_not_null(sensor);

    static xsensor_event_t event;

    XCORO_BEGIN(handle);
    while (1)
    {
        XCORO_WAIT_EVENT(handle, &sensor->event, XSENSOR_EVENT,
                         XCORO_FLAGS_WAIT_ANY, XCORO_WAIT_FOREVER);

        if (XCORO_WAIT_RESULT(handle) & XSENSOR_EVENT)
        {

            while (xrbuf_get_full(&sensor->evt_rb) >= sizeof(event))
            {

                xrbuf_read(&sensor->evt_rb, &event, sizeof(event));

                if (event.type == XSENSOR_RESET)
                {
                    XCORO_CALL(handle, sensor->ops->reset, sensor->inst,
                               &event);
                }
                else if (event.type == XSENSOR_READ)
                {
                    XCORO_CALL(handle, sensor->ops->read, sensor->inst, &event);
                }
            } /*  while (xrbuf_get_full(&sensor->evt_rb) >= sizeof(event)) */
        } /* if (XCORO_WAIT_RESULT(handle) & XSENSOR_EVENT) */
    } /*  while (1) */
    XCORO_END(handle);
}

static xhal_err_t _post_event(xsensor_t *sensor, xsensor_event_type_t type,
                              xsensor_cb_t cb, uint32_t timeout_ms)
{
    xhal_err_t ret = XHAL_OK;

    xsensor_event_t event;
    event.type       = type;
    event.timeout_ms = timeout_ms;
    event.cb         = cb;

    if (xrbuf_get_free(&sensor->evt_rb) >= sizeof(event))
    {
        xrbuf_write(&sensor->evt_rb, &event, sizeof(event));
        XCORO_SET_EVENT(&sensor->event, XSENSOR_EVENT);
    }
    else
    {
        ret = XHAL_ERR_FULL;
    }

    return ret;
}

static inline void _lock(xsensor_t *sensor)
{
#ifdef XHAL_OS_SUPPORTING
    if (sensor->mutex)
    {
        osMutexAcquire(sensor->mutex, osWaitForever);
    }
#endif
}

static inline void _unlock(xsensor_t *sensor)
{
#ifdef XHAL_OS_SUPPORTING
    if (sensor->mutex)
    {
        osMutexRelease(sensor->mutex);
    }
#endif
}
