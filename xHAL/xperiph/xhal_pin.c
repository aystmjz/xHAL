#include "xhal_pin.h"
#include "../xcore/xhal_assert.h"
#include "../xcore/xhal_log.h"

XLOG_TAG("xPin");

xhal_err_t xpin_inst(xhal_pin_t *self, const char *name,
                     const xhal_pin_ops_t *ops, const char *pin_name,
                     xhal_pin_mode_t mode)
{
    xassert_not_null(self);
    xassert_not_null(name);
    xassert_not_null(pin_name);
    xassert_ptr_struct_not_null(ops, name);
    xassert_name(mode < XPIN_MODE_MAX, name);

    xhal_pin_t *pin = self;

    xhal_periph_attr_t periph_attr = {
        .name = name,
        .type = XHAL_PERIPH_PIN,
    };
    xperiph_register(&pin->peri, &periph_attr);

    pin->ops       = ops;
    pin->data.name = pin_name;
    pin->data.mode = mode;

    xhal_err_t ret = pin->ops->init(pin);
    if (ret != XHAL_OK)
    {
        xperiph_unregister(&pin->peri);
    }
    pin->peri.is_inited = XPERIPH_INITED;

    return XHAL_OK;
}

xhal_err_t xpin_set_mode(xhal_periph_t *self, xhal_pin_mode_t mode)
{
    xassert_not_null(self);
    xassert_name(mode < XPIN_MODE_MAX, self->attr.name);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_PIN);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_SYSTEM);

    xhal_err_t ret  = XHAL_OK;
    xhal_pin_t *pin = XPIN_CAST(self);

    xperiph_lock(self);
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

xhal_pin_state_t xpin_get_status(xhal_periph_t *self)
{
    xassert_not_null(self);
    xassert_name(XPIN_CAST(self)->data.mode < XPIN_MODE_MAX, self->attr.name);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_PIN);
    XPERIPH_CHECK_INIT(self, XPIN_CAST(self)->data.status);

    xhal_pin_t *pin = XPIN_CAST(self);

    xperiph_lock(self);
    if (pin->data.mode <= XPIN_MODE_INPUT_PULLDOWN)
    {
        xhal_pin_state_t status;
        xhal_err_t ret = pin->ops->get_status(pin, &status);
        if (ret == XHAL_OK)
        {
            pin->data.status = status;
        }
    }
    xperiph_unlock(self);

    return pin->data.status;
}

xhal_err_t xpin_set_status(xhal_periph_t *self, xhal_pin_state_t status)
{
    xassert_not_null(self);
    xassert_name(XPIN_CAST(self)->data.mode == XPIN_MODE_OUTPUT_PP ||
                     XPIN_CAST(self)->data.mode == XPIN_MODE_OUTPUT_OD,
                 self->attr.name);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_PIN);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_SYSTEM);

    xhal_err_t ret  = XHAL_OK;
    xhal_pin_t *pin = XPIN_CAST(self);

    xperiph_lock(self);
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
