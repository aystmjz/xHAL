#include "xhal_i2c.h"
#include "../xcore/xhal_assert.h"
#include "../xcore/xhal_log.h"
#include "../xcore/xhal_time.h"

XLOG_TAG("xI2C");

#ifdef XHAL_OS_SUPPORTING
static const osEventFlagsAttr_t xi2c_event_flag_attr = {
    .name      = "xi2c_event_flag",
    .attr_bits = 0,
    .cb_mem    = NULL,
    .cb_size   = 0,
};
#endif

xhal_err_t xi2c_inst(xhal_i2c_t *self, const char *name,
                     const xhal_i2c_ops_t *ops, const char *i2c_name,
                     const char *sda_name, const char *scl_name,
                     const xhal_i2c_config_t *config)
{
    xassert_not_null(self);
    xassert_not_null(name);
    xassert_not_null(i2c_name);
    xassert_not_null(config);
    xassert_ptr_struct_not_null(ops, name);

    xhal_i2c_t *i2c                  = self;
    xhal_periph_attr_t periph_config = {
        .name = name,
        .type = XHAL_PERIPH_I2C,
    };
    xperiph_register(&i2c->peri, &periph_config);

    i2c->ops         = ops;
    i2c->data.config = *config;

    i2c->data.i2c_name = i2c_name;
    i2c->data.sda_name = sda_name;
    i2c->data.scl_name = scl_name;

#ifdef XHAL_OS_SUPPORTING
    i2c->data.event_flag = osEventFlagsNew(&xi2c_event_flag_attr);
    xassert_not_null(i2c->data.event_flag);
#endif
    xhal_err_t ret = i2c->ops->init(i2c);
    if (ret != XHAL_OK)
    {
        xperiph_unregister(&i2c->peri);

#ifdef XHAL_OS_SUPPORTING
        osEventFlagsDelete(i2c->data.event_flag);
#endif
        return ret;
    }

    i2c->peri.is_inited = XPERIPH_INITED;

    return XHAL_OK;
}

xhal_err_t xi2c_transfer(xhal_periph_t *self, xhal_i2c_msg_t *msgs,
                         uint32_t num, uint32_t timeout_ms)
{
    xassert_not_null(self);
    xassert_not_null(msgs);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_I2C);
    if (num == 0)
        return XHAL_OK;

    xhal_err_t ret               = XHAL_OK;
    xhal_i2c_t *i2c              = XHAL_I2C_CAST(self);
    xhal_tick_ms_t start_tick_ms = xtime_get_tick_ms();

    xperiph_lock(self);

    for (uint32_t i = 0; i < num; i++)
    {
        if (msgs[i].buf == NULL || msgs[i].len == 0)
        {
            ret = XHAL_ERR_INVALID;
            goto exit;
        }
    }

    for (uint32_t i = 0; i < num; i++)
    {
        if (i == 0)
        {
            msgs[i].flags &= ~XI2C_NOSTART;
        }
        if (i == num - 1)
        {
            msgs[i].flags |= XI2C_STOP;
        }
        ret = i2c->ops->transfer(i2c, &msgs[i]);
        if (ret != XHAL_OK)
        {
            goto exit;
        }

        uint32_t elapsed_ms = xtime_get_tick_ms() - start_tick_ms;
        if (elapsed_ms >= timeout_ms)
        {
            ret = XHAL_ERR_TIMEOUT;
            goto exit;
        }

#ifdef XHAL_OS_SUPPORTING
        uint32_t wait_ms  = timeout_ms - elapsed_ms;
        osStatus_t ret_os = (osStatus_t)osEventFlagsWait(
            i2c->data.event_flag, XI2C_EVENT_DONE, osFlagsWaitAny,
            XOS_MS_TO_TICKS(wait_ms));
        if (ret_os == osErrorTimeout)
        {
            ret = XHAL_ERR_TIMEOUT;
            goto exit;
        }
#endif
    }

exit:
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xi2c_read(xhal_periph_t *self, uint16_t addr, uint8_t *buff,
                     uint16_t len, uint16_t flags, uint32_t timeout)
{
    xassert_not_null(self);
    xassert_not_null(buff);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_I2C);
    if (len == 0)
        return XHAL_OK;

    xhal_i2c_msg_t msg;
    msg.addr  = addr;
    msg.flags = XI2C_RD | flags;
    msg.buf   = buff;
    msg.len   = len;

    return xi2c_transfer(self, &msg, 1, timeout);
}

xhal_err_t xi2c_write(xhal_periph_t *self, uint16_t addr, uint8_t *buff,
                      uint16_t len, uint16_t flags, uint32_t timeout)
{
    xassert_not_null(self);
    xassert_not_null(buff);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_I2C);
    if (len == 0)
        return XHAL_OK;

    xhal_i2c_msg_t msg;
    msg.addr  = addr;
    msg.flags = XI2C_WD | flags;
    msg.buf   = buff;
    msg.len   = len;

    return xi2c_transfer(self, &msg, 1, timeout);
}

xhal_err_t xi2c_write_read(xhal_periph_t *self, uint16_t addr, uint8_t *wbuf,
                           uint16_t wlen, uint8_t *rbuf, uint16_t rlen,
                           uint16_t flags, uint32_t timeout)
{
    xassert_not_null(self);
    xassert_not_null(wbuf);
    xassert_not_null(rbuf);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_I2C);
    if (wlen == 0 || rlen == 0)
        return XHAL_OK;

    xhal_i2c_msg_t msgs[2];

    msgs[0].addr  = addr;
    msgs[0].flags = XI2C_WD | flags;
    msgs[0].buf   = wbuf;
    msgs[0].len   = wlen;

    msgs[1].addr  = addr;
    msgs[1].flags = XI2C_RD | flags;
    msgs[1].buf   = rbuf;
    msgs[1].len   = rlen;

    return xi2c_transfer(self, msgs, 2, timeout);
}

xhal_err_t xi2c_set_config(xhal_periph_t *self, xhal_i2c_config_t *config)
{
    xassert_not_null(self);
    xassert_not_null(config);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_I2C);

    xhal_err_t ret  = XHAL_OK;
    xhal_i2c_t *i2c = XHAL_I2C_CAST(self);

    xperiph_lock(self);
    ret = i2c->ops->config(i2c, config);
    if (ret == XHAL_OK)
    {
        i2c->data.config = *config;
    }
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xi2c_get_config(xhal_periph_t *self, xhal_i2c_config_t *config)
{

    xassert_not_null(self);
    xassert_not_null(config);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_I2C);

    xhal_i2c_t *i2c = XHAL_I2C_CAST(self);

    xperiph_lock(self);
    *config = i2c->data.config;
    xperiph_unlock(self);

    return XHAL_OK;
}