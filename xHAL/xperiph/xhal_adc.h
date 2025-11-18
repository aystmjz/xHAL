#ifndef __XHAL_ADC_H
#define __XHAL_ADC_H

#include "../xlib/xhal_ringbuf.h"
#include "xhal_periph.h"

#define XADC_EVENT_DATA_READY (1U << 0)

#define XADC_CONFIG_DEFAULT                      \
    {.reference_voltage = 3.3f,                  \
     .resolution        = XADC_RESOLUTION_12BIT, \
     .mode              = XADC_MODE_REALTIME}

#define XADC_CHANNEL_0  ((uint16_t)(1U << 0))  /* 通道 0 */
#define XADC_CHANNEL_1  ((uint16_t)(1U << 1))  /* 通道 1 */
#define XADC_CHANNEL_2  ((uint16_t)(1U << 2))  /* 通道 2 */
#define XADC_CHANNEL_3  ((uint16_t)(1U << 3))  /* 通道 3 */
#define XADC_CHANNEL_4  ((uint16_t)(1U << 4))  /* 通道 4 */
#define XADC_CHANNEL_5  ((uint16_t)(1U << 5))  /* 通道 5 */
#define XADC_CHANNEL_6  ((uint16_t)(1U << 6))  /* 通道 6 */
#define XADC_CHANNEL_7  ((uint16_t)(1U << 7))  /* 通道 7 */
#define XADC_CHANNEL_8  ((uint16_t)(1U << 8))  /* 通道 8 */
#define XADC_CHANNEL_9  ((uint16_t)(1U << 9))  /* 通道 9 */
#define XADC_CHANNEL_10 ((uint16_t)(1U << 10)) /* 通道 10 */
#define XADC_CHANNEL_11 ((uint16_t)(1U << 11)) /* 通道 11 */
#define XADC_CHANNEL_12 ((uint16_t)(1U << 12)) /* 通道 12 */
#define XADC_CHANNEL_13 ((uint16_t)(1U << 13)) /* 通道 13 */
#define XADC_CHANNEL_14 ((uint16_t)(1U << 14)) /* 通道 14 */
#define XADC_CHANNEL_15 ((uint16_t)(1U << 15)) /* 通道 15 */

enum xhal_adc_resolution
{
    XADC_RESOLUTION_8BIT  = 8,
    XADC_RESOLUTION_10BIT = 10,
    XADC_RESOLUTION_12BIT = 12,
    XADC_RESOLUTION_14BIT = 14,
    XADC_RESOLUTION_16BIT = 16,
};

enum xhal_adc_mode
{
    XADC_MODE_REALTIME = 0, /* 实时响应模式 */
    XADC_MODE_CONTINUOUS,   /* 连续采集模式 */
};

typedef struct xhal_adc_config
{
    float reference_voltage;
    uint8_t resolution; /* 分辨率 */
    uint8_t mode;       /* 采样模式 */
} xhal_adc_config_t;

typedef struct
{
    uint32_t cache_used;     /* 当前缓存已使用字节数 */
    uint32_t cache_free;     /* 当前缓存剩余可用字节数 */
    uint32_t overflow_count; /* 缓存溢出次数 */
    uint32_t sample_count;   /* 累计采样次数 */
} xadc_status_t;

typedef struct xhal_adc xhal_adc_t;

typedef struct xhal_adc_ops
{
    xhal_err_t (*init)(xhal_adc_t *self);
    xhal_err_t (*trigger_single)(xhal_adc_t *self, uint32_t samples);
    uint32_t (*read_sample)(xhal_adc_t *self, uint32_t samples,
                            uint16_t channel_mask, uint16_t **buffers);
    xhal_err_t (*set_config)(xhal_adc_t *self, const xhal_adc_config_t *config);
    xhal_err_t (*start_continuous)(xhal_adc_t *self);
    xhal_err_t (*stop_continuous)(xhal_adc_t *self);
    xhal_err_t (*calibrate)(xhal_adc_t *self);
} xhal_adc_ops_t;

typedef struct xhal_adc_data
{
    xhal_adc_config_t config;
    uint16_t channel_mask;

    xrbuf_t data_rbuf;
    uint32_t overflow_count;
    uint32_t sample_count;

#ifdef XHAL_OS_SUPPORTING
    osEventFlagsId_t event_flag;
#endif
    const char *name;
} xhal_adc_data_t;

typedef struct xhal_adc
{
    xhal_periph_t peri;

    const xhal_adc_ops_t *ops;
    xhal_adc_data_t data;
} xhal_adc_t;

#define XADC_CAST(_dev) ((xhal_adc_t *)_dev)

xhal_err_t xadc_inst(xhal_adc_t *self, const char *name,
                     const xhal_adc_ops_t *ops, const char *adc_name,
                     const xhal_adc_config_t *config, uint16_t channel_mask,
                     void *data_buff, uint32_t data_bufsz);

uint32_t xadc_read_raw(xhal_periph_t *self, uint16_t samples,
                       uint16_t channel_mask, uint32_t timeout_ms,
                       uint16_t *buffer, ...);
uint32_t xadc_read_voltage(xhal_periph_t *self, uint16_t samples,
                           uint16_t channel_mask, uint32_t timeout_ms,
                           float *buffer, ...);

xhal_err_t xadc_start_continuous(xhal_periph_t *self);
xhal_err_t xadc_stop_continuous(xhal_periph_t *self);

xhal_err_t xadc_get_status(xhal_periph_t *self, xadc_status_t *status);
xhal_err_t xadc_get_config(xhal_periph_t *self, xhal_adc_config_t *config);
xhal_err_t xadc_set_config(xhal_periph_t *self, xhal_adc_config_t *config);
xhal_err_t xadc_set_mode(xhal_periph_t *self, uint8_t mode);
xhal_err_t xadc_set_reference_voltage(xhal_periph_t *self, float voltage);

xhal_err_t xadc_calibrate(xhal_periph_t *self);

#endif /* __XHAL_ADC_H */
