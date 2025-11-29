#include "xhal_spi.h"
#include "../xcore/xhal_assert.h"
#include "../xcore/xhal_log.h"
#include "../xcore/xhal_time.h"
#include "xhal_pin.h"

XLOG_TAG("xSPI");

#define IS_XSPI_MODE(MOD)                                \
    (((MOD) == XSPI_MODE_0) || ((MOD) == XSPI_MODE_1) || \
     ((MOD) == XSPI_MODE_2) || ((MOD) == XSPI_MODE_3))

#define IS_XSPI_DIRECTION(DIR)                                            \
    (((DIR) == XSPI_DIR_2LINE_FULL_DUPLEX) ||                             \
     ((DIR) == XSPI_DIR_2LINE_RX_ONLY) || ((DIR) == XSPI_DIR_1LINE_RX) || \
     ((DIR) == XSPI_DIR_1LINE_TX))

#define IS_XSPI_DATA_BITS(BITS) \
    (((BITS) == XSPI_DATA_BITS_8) || ((BITS) == XSPI_DATA_BITS_16))

#ifdef XHAL_OS_SUPPORTING
static const osEventFlagsAttr_t xspi_event_flag_attr = {
    .name      = "xspi_event_flag",
    .attr_bits = 0,
    .cb_mem    = NULL,
    .cb_size   = 0,
};
#endif

xhal_err_t xspi_inst(xhal_spi_t *self, const char *name,
                     const xhal_spi_ops_t *ops, const char *spi_name,
                     const char *sck_name, const char *mosi_name,
                     const char *miso_name, const xhal_spi_config_t *config)
{
    xassert_not_null(self);
    xassert_not_null(name);
    xassert_not_null(spi_name);
    xassert_not_null(config);
    xassert_ptr_struct_not_null(ops, name);
    xassert_name(IS_XSPI_MODE(config->mode), name);
    xassert_name(IS_XSPI_DIRECTION(config->direction), name);
    xassert_name(IS_XSPI_DATA_BITS(config->data_bits), name);

    xhal_err_t ret                   = XHAL_OK;
    xhal_spi_t *spi                  = self;
    xhal_periph_attr_t periph_config = {
        .name = name,
        .type = XHAL_PERIPH_SPI,
    };

    ret = xperiph_register(&spi->peri, &periph_config);
    if (ret != XHAL_OK)
    {
        return ret;
    }

    spi->ops            = ops;
    spi->data.config    = *config;
    spi->data.spi_name  = spi_name;
    spi->data.sck_name  = sck_name;
    spi->data.miso_name = miso_name;
    spi->data.mosi_name = mosi_name;

#ifdef XHAL_OS_SUPPORTING
    spi->data.event_flag = osEventFlagsNew(&xspi_event_flag_attr);
    xassert_not_null(spi->data.event_flag);
#endif
    ret = spi->ops->init(spi);
    if (ret != XHAL_OK)
    {
        xperiph_unregister(&spi->peri);

#ifdef XHAL_OS_SUPPORTING
        osEventFlagsDelete(spi->data.event_flag);
#endif
        return ret;
    }

    spi->peri.is_inited = XPERIPH_INITED;

    return XHAL_OK;
}

xhal_err_t xspi_transfer(xhal_periph_t *self, xhal_spi_msg_t *msgs,
                         uint32_t num, uint32_t timeout_ms)
{
    xassert_not_null(self);
    xassert_not_null(msgs);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_SPI);
    if (num == 0)
        return XHAL_OK;

    xhal_err_t ret               = XHAL_OK;
    xhal_spi_t *spi              = XSPI_CAST(self);
    xhal_tick_ms_t start_tick_ms = xtime_get_tick_ms();

    xperiph_lock(self);

    for (uint32_t i = 0; i < num; i++)
    {
        if ((msgs[i].tx_buf == NULL && msgs[i].rx_buf == NULL) ||
            msgs[i].len == 0)
        {
            ret = XHAL_ERR_INVALID;
            goto exit;
        }
        if ((spi->data.config.direction == XSPI_DIR_2LINE_RX_ONLY ||
             spi->data.config.direction == XSPI_DIR_1LINE_RX) &&
            msgs[i].rx_buf == NULL)
        {
            ret = XHAL_ERR_INVALID;
            goto exit;
        }
        if (spi->data.config.direction == XSPI_DIR_1LINE_TX &&
            msgs[i].tx_buf == NULL)
        {
            ret = XHAL_ERR_INVALID;
            goto exit;
        }
    }

    for (uint32_t i = 0; i < num; i++)
    {
        ret = spi->ops->transfer(spi, &msgs[i]);
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
        uint32_t wait_ms    = timeout_ms - elapsed_ms;
        uint32_t wait_flags = 0;

        switch (spi->data.config.direction)
        {
        case XSPI_DIR_2LINE_FULL_DUPLEX:
            wait_flags = XSPI_EVENT_DONE;
            break;
        case XSPI_DIR_2LINE_RX_ONLY:
        case XSPI_DIR_1LINE_RX:
            wait_flags = XSPI_EVENT_RX_DONE;
            break;
        case XSPI_DIR_1LINE_TX:
            wait_flags = XSPI_EVENT_TX_DONE;
            break;
        default:
            break;
        }

        osStatus_t ret_os = (osStatus_t)osEventFlagsWait(
            spi->data.event_flag, wait_flags, osFlagsWaitAll,
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

xhal_err_t xspi_read(xhal_periph_t *self, uint8_t *buff, uint16_t len,
                     uint32_t timeout_ms)
{
    xassert_not_null(self);
    xassert_not_null(buff);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_SPI);
    if (len == 0)
        return XHAL_OK;

    xhal_spi_msg_t msg;
    msg.tx_buf = NULL;
    msg.rx_buf = buff;
    msg.len    = len;

    return xspi_transfer(self, &msg, 1, timeout_ms);
}

xhal_err_t xspi_write(xhal_periph_t *self, uint8_t *buff, uint16_t len,
                      uint32_t timeout_ms)
{
    xassert_not_null(self);
    xassert_not_null(buff);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_SPI);
    if (len == 0)
        return XHAL_OK;

    xhal_spi_msg_t msg;
    msg.tx_buf = buff;
    msg.rx_buf = NULL;
    msg.len    = len;

    return xspi_transfer(self, &msg, 1, timeout_ms);
}

xhal_err_t xspi_write_read(xhal_periph_t *self, uint8_t *wbuf, uint16_t wlen,
                           uint8_t *rbuf, uint16_t rlen, uint32_t timeout_ms)
{
    xassert_not_null(self);
    xassert_not_null(wbuf);
    xassert_not_null(rbuf);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_SPI);
    if (wlen == 0 || rlen == 0)
        return XHAL_OK;

    xhal_spi_msg_t msgs[2];

    msgs[0].tx_buf = wbuf;
    msgs[0].rx_buf = NULL;
    msgs[0].len    = wlen;

    msgs[1].tx_buf = NULL;
    msgs[1].rx_buf = rbuf;
    msgs[1].len    = rlen;

    return xspi_transfer(self, msgs, 2, timeout_ms);
}

xhal_err_t xspi_set_config(xhal_periph_t *self, xhal_spi_config_t *config)
{
    xassert_not_null(self);
    xassert_not_null(config);
    xassert_name(IS_XSPI_MODE(config->mode), self->attr.name);
    xassert_name(IS_XSPI_DIRECTION(config->direction), self->attr.name);
    xassert_name(IS_XSPI_DATA_BITS(config->data_bits), self->attr.name);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_SPI);

    xhal_err_t ret  = XHAL_OK;
    xhal_spi_t *spi = XSPI_CAST(self);

    xperiph_lock(self);
    ret = spi->ops->config(spi, config);
    if (ret == XHAL_OK)
    {
        spi->data.config = *config;
    }
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xspi_get_config(xhal_periph_t *self, xhal_spi_config_t *config)
{

    xassert_not_null(self);
    xassert_not_null(config);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_SPI);

    xhal_spi_t *spi = XSPI_CAST(self);

    xperiph_lock(self);
    *config = spi->data.config;
    xperiph_unlock(self);

    return XHAL_OK;
}

xhal_err_t xspi_set_direction(xhal_periph_t *self, uint8_t direction)
{

    xassert_not_null(self);
    xassert_name(IS_XSPI_DIRECTION(direction), self->attr.name);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_SPI);

    xhal_spi_t *spi = XSPI_CAST(self);

    xperiph_lock(self);
    xhal_spi_config_t config = spi->data.config;
    xperiph_unlock(self);
    config.direction = direction;

    return xspi_get_config(self, &config);
}