#include "xhal_pin.h"
#include "../xcore/xhal_assert.h"
#include "../xcore/xhal_log.h"

XLOG_TAG("xPin");

xhal_err_t xpin_inst(xhal_pin_t *const self, const char *name,
                     const xhal_pin_ops_t *ops, const char *pin_name,
                     pin_mode_t mode)
{
    xassert_not_null(self);
    xassert_not_null(name);
    xassert_not_null(pin_name);
    xassert_ptr_struct_not_null(ops, name);
    xassert_name(mode < XPIN_MODE_MAX, name);

    xhal_err_t ret = XHAL_OK;

    xhal_periph_attr_t attr = {
        .name = name,
        .type = XHAL_PERIPH_PIN,
    };
    xperiph_register(&self->peri, &attr);

    self->ops       = ops;
    self->data.name = pin_name;
    self->data.mode = mode;

    ret = self->ops->init(self);
    if (ret == XHAL_OK)
    {
        self->peri.is_inited = XPERIPH_INITED;
    }
    else
    {
        xperiph_unregister(&self->peri);
    }

    return ret;
}

xhal_err_t xpin_set_mode(xhal_periph_t *const self, pin_mode_t mode)
{
    xassert_not_null(self);
    xassert_name(mode < XPIN_MODE_MAX, self->attr.name);

    xhal_err_t ret  = XHAL_OK;
    xhal_pin_t *pin = XPIN_CAST(self);

    xperiph_lock(self);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_SYSTEM);
    if (pin->data.mode != mode)
    {
        ret = pin->ops->set_mode(pin, mode);
        if (ret == XHAL_OK)
        {
            pin->data.mode = mode;
        }
    }
    xperiph_unlock(self);

    return ret;
}

pin_state_t xpin_get_status(xhal_periph_t *const self)
{
    xassert_not_null(self);
    xassert_name(XPIN_CAST(self)->data.mode < XPIN_MODE_MAX, self->attr.name);

    xhal_pin_t *pin = XPIN_CAST(self);

    xperiph_lock(self);
    XPERIPH_CHECK_INIT(self, pin->data.status);
    if (pin->data.mode <= XPIN_MODE_INPUT_PULLDOWN)
    {
        pin_state_t status;
        xhal_err_t ret = pin->ops->get_status(pin, &status);
        if (ret == XHAL_OK)
        {
            pin->data.status = status;
        }
    }
    xperiph_unlock(self);

    return pin->data.status;
}

xhal_err_t xpin_set_status(xhal_periph_t *const self, pin_state_t status)
{
    xassert_not_null(self);
    xassert_name(XPIN_CAST(self)->data.mode == XPIN_MODE_OUTPUT_PP ||
                     XPIN_CAST(self)->data.mode == XPIN_MODE_OUTPUT_OD,
                 self->attr.name);

    xhal_err_t ret  = XHAL_OK;
    xhal_pin_t *pin = XPIN_CAST(self);

    xperiph_lock(self);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_SYSTEM);
    if (status != pin->data.status)
    {
        ret = pin->ops->set_status(pin, status);
        if (ret == XHAL_OK)
        {
            pin->data.status = status;
        }
    }
    xperiph_unlock(self);

    return ret;
}
