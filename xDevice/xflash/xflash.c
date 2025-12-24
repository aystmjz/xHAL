#include "xflash.h"
#include "xhal_assert.h"
#include "xhal_log.h"

XLOG_TAG("xFLASH");

#define XFLASH_EVENT (1U << 0)

#ifdef XHAL_OS_SUPPORTING
static const osMutexAttr_t xflash_mutex_attr = {
    .name      = "xflash_mutex",
    .attr_bits = osMutexPrioInherit | osMutexRecursive,
    .cb_mem    = NULL,
    .cb_size   = 0,
};
#endif

static inline void _lock(xflash_t *flash);
static inline void _unlock(xflash_t *flash);

xhal_err_t xflash_init(xflash_t *flash, const xflash_ops_t *ops, void *inst)
{
    xassert_not_null(flash);
    xassert_not_null(ops);
    xassert_not_null(inst);
    xassert_ptr_struct_not_null(ops, "xflash_ops is null");

    xcoro_event_init(&flash->event);
    xrbuf_init(&flash->evt_rb, flash->evt_buff, sizeof(flash->evt_buff));

    flash->ops  = ops;
    flash->inst = inst;

#ifdef XHAL_OS_SUPPORTING
    flash->mutex = osMutexNew(&xflash_mutex_attr);
    xassert_not_null(flash->mutex);
#endif

    xhal_err_t ret = ops->init(inst);
    if (ret != XHAL_OK)
    {
#ifdef XHAL_OS_SUPPORTING
        osMutexDelete(flash->mutex);
#endif
        return ret;
    }

    return XHAL_OK;
}

xhal_err_t xflash_deinit(xflash_t *flash)
{
    xassert_not_null(flash);

    if (flash->ops == NULL)
    {
        return XHAL_ERR_NO_INIT;
    }

    xhal_err_t ret = flash->ops->deinit(flash->inst);

    xrbuf_free(&flash->evt_rb);
    flash->ops  = NULL;
    flash->inst = NULL;

#ifdef XHAL_OS_SUPPORTING
    osMutexDelete(flash->mutex);
    flash->mutex = NULL;
#endif

    return ret;
}

xhal_err_t xflash_read_id(xflash_t *flash, uint8_t *manufacturer_id,
                          uint16_t *device_id, uint32_t timeout_ms)
{
    xassert_not_null(flash);
    xassert_not_null(manufacturer_id);
    xassert_not_null(device_id);

    if (flash->ops == NULL)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(flash);
    xhal_err_t ret = flash->ops->read_id(flash->inst, manufacturer_id,
                                         device_id, timeout_ms);
    _unlock(flash);

    return ret;
}

xhal_err_t xflash_read(xflash_t *flash, uint32_t address, uint8_t *buffer,
                       uint32_t size, uint32_t timeout_ms)
{
    xassert_not_null(flash);
    xassert_not_null(buffer);

    if (flash->ops == NULL)
    {
        return XHAL_ERR_NO_INIT;
    }

    if (size == 0)
    {
        return XHAL_OK;
    }

    _lock(flash);
    xhal_err_t ret =
        flash->ops->read(flash->inst, address, buffer, size, timeout_ms);
    _unlock(flash);

    return ret;
}

xhal_err_t xflash_write(xflash_t *flash, uint32_t address, const uint8_t *data,
                        uint32_t size, uint32_t timeout_ms)
{
    xassert_not_null(flash);
    xassert_not_null(data);

    if (flash->ops == NULL)
    {
        return XHAL_ERR_NO_INIT;
    }

    if (size == 0)
    {
        return XHAL_OK;
    }

    _lock(flash);
    xhal_err_t ret =
        flash->ops->write(flash->inst, address, data, size, timeout_ms);
    _unlock(flash);

    return ret;
}

xhal_err_t xflash_erase(xflash_t *flash, xflash_erase_type_t type,
                        uint32_t address, xflash_cb_t cb, uint32_t timeout_ms)
{
    xassert_not_null(flash);

    if (flash->ops == NULL)
    {
        return XHAL_ERR_NO_INIT;
    }

    xhal_err_t ret = XHAL_OK;

    _lock(flash);
    xflash_event_t event;
    event.type       = type;
    event.address    = address;
    event.timeout_ms = timeout_ms;
    event.cb         = cb;

    if (xrbuf_get_free(&flash->evt_rb) >= sizeof(event))
    {
        xrbuf_write(&flash->evt_rb, &event, sizeof(event));
        XCORO_SET_EVENT(&flash->event, XFLASH_EVENT);
    }
    else
    {
        ret = XHAL_ERR_FULL;
    }
    _unlock(flash);

    return ret;
}

void xflash_handler_thread(xcoro_handle_t *handle, xflash_t *flash)
{
    xassert_not_null(handle);
    xassert_not_null(flash);

    static xflash_event_t event;

    XCORO_BEGIN(handle);
    while (1)
    {
        XCORO_WAIT_EVENT(handle, &flash->event, XFLASH_EVENT,
                         XCORO_FLAGS_WAIT_ANY, XCORO_WAIT_FOREVER);

        if (XCORO_WAIT_RESULT(handle) & XFLASH_EVENT)
        {
            while (xrbuf_get_full(&flash->evt_rb) >= sizeof(event))
            {
                xrbuf_read(&flash->evt_rb, &event, sizeof(event));

                XCORO_CALL(handle, flash->ops->erase, flash->inst, &event);
            }
        }
    }
    XCORO_END(handle);
}

static inline void _lock(xflash_t *flash)
{
#ifdef XHAL_OS_SUPPORTING
    if (flash->mutex != NULL)
    {
        osStatus_t ret = osMutexAcquire(flash->mutex, osWaitForever);
        xassert(ret == osOK);
    }
#endif
}

static inline void _unlock(xflash_t *flash)
{
#ifdef XHAL_OS_SUPPORTING
    if (flash->mutex != NULL)
    {
        osStatus_t ret = osMutexRelease(flash->mutex);
        xassert(ret == osOK);
    }
#endif
}