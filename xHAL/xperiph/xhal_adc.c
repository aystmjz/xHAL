#include "xhal_adc.h"
#include "../xcore/xhal_assert.h"
#include "../xcore/xhal_log.h"
#include "../xcore/xhal_time.h"
#include <stdarg.h>

XLOG_TAG("xADC");

#define IS_XADC_MODE(MODE) \
    (((MODE) == XADC_MODE_REALTIME) || ((MODE) == XADC_MODE_CONTINUOUS))

#define IS_XADC_RESOLUTION(RES)                                              \
    (((RES) == XADC_RESOLUTION_8BIT) || ((RES) == XADC_RESOLUTION_10BIT) ||  \
     ((RES) == XADC_RESOLUTION_12BIT) || ((RES) == XADC_RESOLUTION_14BIT) || \
     ((RES) == XADC_RESOLUTION_16BIT))

#ifdef XHAL_OS_SUPPORTING
static const osEventFlagsAttr_t xadc_event_flag_attr = {
    .name      = "xadc_event_flag",
    .attr_bits = 0,
    .cb_mem    = NULL,
    .cb_size   = 0,
};
#endif

xhal_err_t xadc_inst(xhal_adc_t *self, const char *name,
                     const xhal_adc_ops_t *ops, const char *adc_name,
                     const xhal_adc_config_t *config, uint16_t channel_mask,
                     void *data_buff, uint32_t data_bufsz)
{
    xassert_not_null(self);
    xassert_not_null(name);
    xassert_not_null(adc_name);
    xassert_not_null(config);
    xassert_not_null(data_buff);
    xassert_ptr_struct_not_null(ops, name);
    xassert_name(IS_XADC_RESOLUTION(config->resolution), name);
    xassert_name(channel_mask != 0, name);
    xassert_name(config->reference_voltage > 0, name);

    xhal_err_t ret                   = XHAL_OK;
    xhal_adc_t *adc                  = self;
    xhal_periph_attr_t periph_config = {
        .name = name,
        .type = XHAL_PERIPH_ADC,
    };

    ret = xperiph_register(&adc->peri, &periph_config);
    if (ret != XHAL_OK)
    {
        return ret;
    }

    adc->ops               = ops;
    adc->data.channel_mask = channel_mask;
    adc->data.config       = *config;
    adc->data.name         = adc_name;

    xrbuf_init(&adc->data.data_rbuf, data_buff, data_bufsz);

#ifdef XHAL_OS_SUPPORTING
    adc->data.event_flag = osEventFlagsNew(&xadc_event_flag_attr);
    xassert_not_null(adc->data.event_flag);
#endif

    ret = adc->ops->init(adc);
    if (ret != XHAL_OK)
    {
        xperiph_unregister(&adc->peri);

#ifdef XHAL_OS_SUPPORTING
        osEventFlagsDelete(adc->data.event_flag);
#endif
        return ret;
    }

    adc->peri.is_inited = XPERIPH_INITED;

    return XHAL_OK;
}

uint32_t xadc_read_raw(xhal_periph_t *self, uint16_t samples,
                       uint16_t channel_mask, uint32_t timeout_ms,
                       uint16_t *buffer, ...)
{
    xassert_not_null(self);
    xassert_not_null(buffer);
    XPERIPH_CHECK_INIT(self, 0);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_ADC);

    if (samples == 0 || channel_mask == 0)
        return 0;

    if ((XADC_CAST(self)->data.channel_mask & channel_mask) != channel_mask)
        return 0;

    uint8_t channel_count = 0;
    for (uint8_t i = 0; i < 16; i++)
    {
        if (channel_mask & (1U << i))
        {
            channel_count++;
        }
    }

    uint16_t *buffers[channel_count];
    buffers[0] = buffer;

    va_list args;
    va_start(args, buffer);
    for (uint8_t i = 1; i < channel_count; i++)
    {
        buffers[i] = va_arg(args, uint16_t *);
        xassert_not_null(buffers[i]);
    }
    va_end(args);

    xhal_adc_t *adc           = XADC_CAST(self);
    xhal_tick_t start_tick_ms = xtime_get_tick_ms();
    uint32_t samples_read     = 0;

    xperiph_lock(self);
    if (adc->data.config.mode == XADC_MODE_REALTIME)
    {
        xhal_err_t ret = adc->ops->trigger_single(adc, samples);
        if (ret != XHAL_OK)
            goto exit;
    }

    while (1)
    {
        uint16_t *current_buffers[channel_count];
        for (uint8_t i = 0; i < channel_count; i++)
        {
            current_buffers[i] = buffers[i] + samples_read;
        }

        uint32_t r = adc->ops->read_sample(adc, samples - samples_read,
                                           channel_mask, current_buffers);

        samples_read += r;
        if (samples_read >= samples)
            break;

        uint32_t elapsed_ms = TIME_DIFF(xtime_get_tick_ms(), start_tick_ms);
        if (elapsed_ms >= timeout_ms)
            break;

#ifdef XHAL_OS_SUPPORTING
        uint32_t wait_ms = timeout_ms - elapsed_ms;
        osEventFlagsWait(adc->data.event_flag, XADC_EVENT_DATA_READY,
                         osFlagsWaitAll, XOS_MS_TO_TICKS(wait_ms));
#endif
    }
exit:
    xperiph_unlock(self);

    return samples_read;
}

uint32_t xadc_read_voltage(xhal_periph_t *self, uint16_t samples,
                           uint16_t channel_mask, uint32_t timeout_ms,
                           float *buffer, ...)
{
    xassert_not_null(self);
    xassert_not_null(buffer);
    xassert_name(sizeof(float) == sizeof(uint32_t), "float size not support");
    XPERIPH_CHECK_INIT(self, 0);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_ADC);

    if (samples == 0 || channel_mask == 0)
        return 0;

    if ((XADC_CAST(self)->data.channel_mask & channel_mask) != channel_mask)
        return 0;

    uint8_t channel_count = 0;
    for (uint8_t i = 0; i < 16; i++)
    {
        if (channel_mask & (1 << i))
        {
            channel_count++;
        }
    }

    uint16_t *buffers[channel_count];
    buffers[0] = (uint16_t *)buffer;

    va_list args;
    va_start(args, buffer);
    for (uint8_t i = 1; i < channel_count; i++)
    {
        buffers[i] = (uint16_t *)va_arg(args, float *);
    }
    va_end(args);

    xhal_adc_t *adc           = XADC_CAST(self);
    xhal_tick_t start_tick_ms = xtime_get_tick_ms();
    uint32_t samples_read     = 0;

    xperiph_lock(self);
    float scale_factor = adc->data.config.reference_voltage /
                         ((1 << adc->data.config.resolution) - 1);

    if (adc->data.config.mode == XADC_MODE_REALTIME)
    {
        xhal_err_t ret = adc->ops->trigger_single(adc, samples);
        if (ret != XHAL_OK)
            goto exit;
    }

    while (1)
    {
        uint16_t *current_buffers[channel_count];
        for (uint8_t i = 0; i < channel_count; i++)
        {
            current_buffers[i] = buffers[i] + samples + samples_read;
        }

        uint32_t r = adc->ops->read_sample(adc, samples - samples_read,
                                           channel_mask, current_buffers);

        samples_read += r;
        if (samples_read >= samples)
            break;

        uint32_t elapsed_ms = TIME_DIFF(xtime_get_tick_ms(), start_tick_ms);
        if (elapsed_ms >= timeout_ms)
            break;

#ifdef XHAL_OS_SUPPORTING
        uint32_t wait_ms = timeout_ms - elapsed_ms;
        osEventFlagsWait(adc->data.event_flag, XADC_EVENT_DATA_READY,
                         osFlagsWaitAll, XOS_MS_TO_TICKS(wait_ms));
#endif
    }
exit:
    xperiph_unlock(self);

    if (samples_read > 0)
    {
        for (uint8_t ch = 0; ch < channel_count; ch++)
        {
            uint16_t *raw_buffers  = buffers[ch] + samples;
            float *voltage_buffers = (float *)buffers[ch];

            for (uint32_t i = 0; i < samples_read; i++)
            {

                voltage_buffers[i] = raw_buffers[i] * scale_factor;
            }
        }
    }

    return samples_read;
}

xhal_err_t xadc_start_continuous(xhal_periph_t *self)
{
    xassert_not_null(self);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_ADC);

    xhal_adc_t *adc = XADC_CAST(self);

    xperiph_lock(self);
    xhal_err_t ret = adc->ops->start_continuous(adc);
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xadc_stop_continuous(xhal_periph_t *self)
{
    xassert_not_null(self);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_ADC);

    xhal_adc_t *adc = XADC_CAST(self);

    xperiph_lock(self);
    xhal_err_t ret = adc->ops->stop_continuous(adc);
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xadc_get_status(xhal_periph_t *self, xadc_status_t *status)
{
    xassert_not_null(self);
    xassert_not_null(status);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_ADC);

    xhal_adc_t *adc = XADC_CAST(self);

    xperiph_lock(self);
    status->cache_used     = xrbuf_get_full(&adc->data.data_rbuf);
    status->cache_free     = xrbuf_get_free(&adc->data.data_rbuf);
    status->overflow_count = adc->data.overflow_count;
    status->sample_count   = adc->data.sample_count;
    xperiph_unlock(self);

    return XHAL_OK;
}

xhal_err_t xadc_get_config(xhal_periph_t *self, xhal_adc_config_t *config)
{
    xassert_not_null(self);
    xassert_not_null(config);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_ADC);

    xhal_adc_t *adc = XADC_CAST(self);

    xperiph_lock(self);
    *config = adc->data.config;
    xperiph_unlock(self);

    return XHAL_OK;
}

xhal_err_t xadc_set_config(xhal_periph_t *self, xhal_adc_config_t *config)
{
    xassert_not_null(self);
    xassert_not_null(config);
    xassert_name(IS_XADC_RESOLUTION(config->resolution), self->attr.name);
    xassert_name(config->reference_voltage > 0, self->attr.name);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_ADC);

    xhal_adc_t *adc = XADC_CAST(self);
    xhal_err_t ret  = XHAL_OK;

    xperiph_lock(self);
    ret = adc->ops->set_config(adc, config);
    if (ret == XHAL_OK)
    {
        adc->data.config = *config;
    }
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xadc_set_mode(xhal_periph_t *self, uint8_t mode)
{
    xassert_not_null(self);
    xassert_name(IS_XADC_MODE(mode), self->attr.name);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_ADC);

    xhal_adc_t *adc = XADC_CAST(self);

    xperiph_lock(self);
    xhal_adc_config_t config = adc->data.config;
    xperiph_unlock(self);
    if (mode == config.mode)
    {
        return XHAL_OK;
    }
    config.mode = mode;

    return xadc_set_config(self, &config);
}

xhal_err_t xadc_set_reference_voltage(xhal_periph_t *self, float voltage)
{
    xassert_not_null(self);
    xassert_name(voltage > 0, self->attr.name);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_ADC);

    xhal_adc_t *adc = XADC_CAST(self);

    xperiph_lock(self);
    xhal_adc_config_t config = adc->data.config;
    xperiph_unlock(self);
    if (voltage == config.reference_voltage)
    {
        return XHAL_OK;
    }
    config.reference_voltage = voltage;

    return xadc_set_config(self, &config);
}

xhal_err_t xadc_calibrate(xhal_periph_t *self)
{
    xassert_not_null(self);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_ADC);

    xhal_adc_t *adc = XADC_CAST(self);

    xperiph_lock(self);
    xhal_err_t ret = adc->ops->calibrate(adc);
    xperiph_unlock(self);

    return ret;
}
