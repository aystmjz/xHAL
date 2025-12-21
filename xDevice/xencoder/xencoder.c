#include "xencoder.h"
#include "xhal_assert.h"
#include "xhal_log.h"

XLOG_TAG("xENCODER");

#ifdef XHAL_OS_SUPPORTING
static const osMutexAttr_t xencoder_mutex_attr = {
    .name      = "xencoder_mutex",
    .attr_bits = osMutexPrioInherit | osMutexRecursive,
    .cb_mem    = NULL,
    .cb_size   = 0,
};
#endif

static inline void _lock(xencoder_t *encoder);
static inline void _unlock(xencoder_t *encoder);

xhal_err_t xencoder_init(xencoder_t *encoder, const xencoder_ops_t *ops)
{
    xassert_not_null(encoder);
    xassert_not_null(ops);
    xassert_ptr_struct_not_null(ops, "xencoder_ops is null");

    encoder->ops           = ops;
    encoder->last_position = 0;

#ifdef XHAL_OS_SUPPORTING
    encoder->mutex = osMutexNew(&xencoder_mutex_attr);
    xassert_not_null(encoder->mutex);
#endif

    xhal_err_t ret = ops->clear(encoder);
    if (ret != XHAL_OK)
    {
#ifdef XHAL_OS_SUPPORTING
        osMutexDelete(encoder->mutex);
#endif
        return ret;
    }

    return XHAL_OK;
}

xhal_err_t xencoder_deinit(xencoder_t *encoder)
{
    xassert_not_null(encoder);

    if (!encoder->ops)
    {
        return XHAL_ERR_NO_INIT;
    }

    encoder->ops = NULL;

#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret_os = osMutexDelete(encoder->mutex);
    if (ret_os != osOK)
    {
        return XHAL_ERROR;
    }
    encoder->mutex = NULL;
#endif

    return XHAL_OK;
}

xhal_err_t xencoder_get_position(xencoder_t *encoder, int16_t *position)
{
    xassert_not_null(encoder);
    xassert_not_null(position);

    if (!encoder->ops)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(encoder);
    xhal_err_t ret = encoder->ops->get_position(encoder, position);
    _unlock(encoder);

    return ret;
}

xhal_err_t xencoder_clear(xencoder_t *encoder)
{
    xassert_not_null(encoder);

    if (!encoder->ops)
    {
        return XHAL_ERR_NO_INIT;
    }

    _lock(encoder);
    xhal_err_t ret = encoder->ops->clear(encoder);
    if (ret == XHAL_OK)
    {
        encoder->last_position = 0;
    }
    _unlock(encoder);

    return ret;
}

xhal_err_t xencoder_get_delta(xencoder_t *encoder, int16_t *delta)
{
    xassert_not_null(encoder);
    xassert_not_null(delta);

    if (!encoder->ops)
    {
        return XHAL_ERR_NO_INIT;
    }

    xhal_err_t ret           = XHAL_OK;
    int16_t current_position = 0;

    _lock(encoder);
    ret = encoder->ops->get_position(encoder, &current_position);
    if (ret != XHAL_OK)
    {
        goto exit;
    }

    *delta = current_position - encoder->last_position;

    if (*delta)
    {
        encoder->last_position = current_position;
    }
exit:
    _unlock(encoder);

    return ret;
}

static inline void _lock(xencoder_t *encoder)
{
#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret = osMutexAcquire(encoder->mutex, osWaitForever);
    xassert(ret == osOK);
#endif
}

static inline void _unlock(xencoder_t *encoder)
{
#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret = osMutexRelease(encoder->mutex);
    xassert(ret == osOK);
#endif
}
