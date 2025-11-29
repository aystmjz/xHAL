#include "xhal_exit.h"
#include "../xcore/xhal_assert.h"
#include "../xcore/xhal_log.h"

XLOG_TAG("xExit");

#define IS_XEXIT_MODE(MODE) \
    (((MODE) == XEXIT_MODE_INTERRUPT) || ((MODE) == XEXIT_MODE_EVENT))

#define IS_XEXIT_TRIGGER(TRIGGER)            \
    (((TRIGGER) == XEXIT_TRIGGER_RISING) ||  \
     ((TRIGGER) == XEXIT_TRIGGER_FALLING) || \
     ((TRIGGER) == XEXIT_TRIGGER_BOTH))

xhal_err_t xexit_inst(xhal_exit_t *self, const char *name,
                      const xhal_exit_ops_t *ops, const char *exit_name,
                      const xhal_exit_config_t *config)
{
    xassert_not_null(self);
    xassert_not_null(name);
    xassert_not_null(ops);
    xassert_not_null(config);
    xassert_name(IS_XEXIT_MODE(config->mode), name);
    xassert_name(IS_XEXIT_TRIGGER(config->trigger), name);
    xassert_ptr_struct_not_null(ops, name);

    xhal_err_t ret                 = XHAL_OK;
    xhal_exit_t *exit              = self;
    xhal_periph_attr_t periph_attr = {
        .name = name,
        .type = XHAL_PERIPH_EXIT,
    };

    ret = xperiph_register(&exit->peri, &periph_attr);
    if (ret != XHAL_OK)
    {
        return ret;
    }

    exit->ops               = ops;
    exit->data.config       = *config;
    exit->data.irq_callback = NULL;
    exit->data.name         = exit_name;

    ret = exit->ops->init(exit);
    if (ret != XHAL_OK)
    {
        xperiph_unregister(&exit->peri);
        return ret;
    }
    exit->peri.is_inited = XPERIPH_INITED;

    return XHAL_OK;
}

xhal_err_t xexit_enable_irq(xhal_periph_t *self)
{
    xassert_not_null(self);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_EXIT);

    xhal_exit_t *exit = XEXIT_CAST(self);

    xperiph_lock(self);
    xhal_err_t ret = exit->ops->enable_irq(exit);
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xexit_disable_irq(xhal_periph_t *self)
{
    xassert_not_null(self);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_EXIT);

    xhal_exit_t *exit = XEXIT_CAST(self);

    xperiph_lock(self);
    xhal_err_t ret = exit->ops->disable_irq(exit);
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xexit_set_irq_callback(xhal_periph_t *self, xhal_exit_cb_t cb)
{
    xassert_not_null(self);
    xassert_not_null(cb);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_EXIT);

    xhal_err_t ret    = XHAL_OK;
    xhal_exit_t *exit = XEXIT_CAST(self);

    xperiph_lock(self);
    if (cb != exit->data.irq_callback)
    {
        ret = exit->ops->set_irq_callback(exit, cb);
        if (ret == XHAL_OK)
        {
            exit->data.irq_callback = cb;
        }
    }
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xexit_set_config(xhal_periph_t *self, xhal_exit_config_t *config)
{
    xassert_not_null(self);
    xassert_not_null(config);
    xassert_name(IS_XEXIT_MODE(config->mode), self->attr.name);
    xassert_name(IS_XEXIT_TRIGGER(config->trigger), self->attr.name);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_EXIT);

    xhal_err_t ret    = XHAL_OK;
    xhal_exit_t *exit = XEXIT_CAST(self);

    xperiph_lock(self);
    ret = exit->ops->config(exit, config);
    if (ret == XHAL_OK)
    {
        exit->data.config = *config;
    }
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xexit_get_config(xhal_periph_t *self, xhal_exit_config_t *config)
{
    xassert_not_null(self);
    xassert_not_null(config);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_EXIT);

    xhal_exit_t *exit = XEXIT_CAST(self);

    xperiph_lock(self);
    *config = exit->data.config;
    xperiph_unlock(self);

    return XHAL_OK;
}
