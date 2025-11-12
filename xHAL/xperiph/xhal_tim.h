#ifndef __XHAL_TIM_H
#define __XHAL_TIM_H

#include "xhal_periph.h"

/* ------------------------普通模式配置宏------------------------- */

#define XTIM_CONFIG_NORMAL(_period, _prescaler, ...) \
    {                                                \
        .mode = XTIM_MODE_NORMAL, .config.normal = { \
            .period    = _period,                    \
            .prescaler = _prescaler,                 \
            .compare   = {__VA_ARGS__}               \
        }                                            \
    }

#define XTIM_CONFIG_NORMAL_SIMPLE(_period, _prescaler) \
    XTIM_CONFIG_NORMAL(_period, _prescaler, 0, 0, 0, 0)

/* ------------------------PWM模式配置宏------------------------- */

#define XTIM_PWM_CHANNEL(_ch, _duty, _polarity, _idle) \
    [_ch] = {.duty_cycle = _duty, .polarity = _polarity, .idle_state = _idle}

#define XTIM_PWM_CH1(_duty, _polarity, _idle) \
    XTIM_PWM_CHANNEL(0, _duty, _polarity, _idle)
#define XTIM_PWM_CH2(_duty, _polarity, _idle) \
    XTIM_PWM_CHANNEL(1, _duty, _polarity, _idle)
#define XTIM_PWM_CH3(_duty, _polarity, _idle) \
    XTIM_PWM_CHANNEL(2, _duty, _polarity, _idle)
#define XTIM_PWM_CH4(_duty, _polarity, _idle) \
    XTIM_PWM_CHANNEL(3, _duty, _polarity, _idle)

#define XTIM_CONFIG_PWM(_period, _prescaler, _channel_mask, ...) \
    {                                                            \
        .mode = XTIM_MODE_PWM, .config.pwm = {                   \
            .period       = _period,                             \
            .prescaler    = _prescaler,                          \
            .channels     = {__VA_ARGS__},                       \
            .channel_mask = _channel_mask                        \
        }                                                        \
    }

#define XTIM_CONFIG_PWM_SIMPLE(_period, _prescaler, _channel_mask) \
    XTIM_CONFIG_PWM(_period, _prescaler, _channel_mask, {}, {}, {}, {})

/* ------------------------编码器模式配置宏------------------------- */

#define XTIM_CONFIG_ENCODER(_ch1_pol, _ch1_filt, _ch2_pol, _ch2_filt) \
    {                                                                 \
        .mode = XTIM_MODE_ENCODER, .config.encoder = {                \
            .channel1 = {.polarity = _ch1_pol, .filter = _ch1_filt},  \
            .channel2 = {.polarity = _ch2_pol, .filter = _ch2_filt}   \
        }                                                             \
    }

#define XTIM_CONFIG_ENCODER_SIMPLE(_ch1_polarity, _ch2_polarity) \
    XTIM_CONFIG_ENCODER(_ch1_polarity, 0xF, _ch2_polarity, 0xF)

#define XTIM_CONFIG_ENCODER_DEFAULT \
    XTIM_CONFIG_ENCODER_SIMPLE(XTIM_IC_POLARITY_RISING, XTIM_IC_POLARITY_RISING)

#define XTIM_CHANNEL_1         ((uint8_t)(1U << 0)) /* 通道 1 */
#define XTIM_CHANNEL_2         ((uint8_t)(1U << 1)) /* 通道 2 */
#define XTIM_CHANNEL_3         ((uint8_t)(1U << 2)) /* 通道 3 */
#define XTIM_CHANNEL_4         ((uint8_t)(1U << 3)) /* 通道 4 */
#define XTIM_CHANNEL_MASK(...) (0U | __VA_ARGS__)

enum xhal_tim_oc_idle
{
    XTIM_OC_IDLE_LOW = 1,
    XTIM_OC_IDLE_HIGH
};

enum xhal_tim_oc_polarity
{
    XTIM_OC_POLARITY_HIGH = 1,
    XTIM_OC_POLARITY_LOW
};

/* 编码器输入极性枚举 */
enum xhal_tim_ic_polarity
{
    XTIM_IC_POLARITY_RISING = 0,
    XTIM_IC_POLARITY_FALLING,
    XTIM_IC_POLARITY_BOTH
};

typedef enum xhal_tim_mode
{
    XTIM_MODE_NORMAL = 0, /* 普通模式 */
    XTIM_MODE_PWM,        /* PWM模式 */
    XTIM_MODE_ENCODER     /* 编码器模式 */
} xhal_tim_mode_t;

typedef enum xhal_tim_it
{
    XTIM_IT_UPDATE = 0,
    XTIM_IT_CC1,
    XTIM_IT_CC2,
    XTIM_IT_CC3,
    XTIM_IT_CC4
} xhal_tim_it_t;

typedef void (*xhal_tim_cb_t)(xhal_tim_it_t it);

typedef struct xhal_tim_pwm_channel
{
    uint16_t duty_cycle; /* 占空比 (0-10000 表示 0.00%-100.00%) */
    uint8_t polarity;
    uint8_t idle_state;
} xhal_tim_pwm_channel_t;

typedef struct xhal_tim_pwm_config
{
    uint16_t period;                    /* 周期值 */
    uint16_t prescaler;                 /* 预分频值 */
    xhal_tim_pwm_channel_t channels[4]; /* 4个通道的配置 */
    uint8_t channel_mask; /* 通道启用掩码，每bit对应一个通道 */
} xhal_tim_pwm_config_t;

typedef struct xhal_tim_encoder_channel_config
{
    uint8_t polarity; /* 通道极性 */
    uint8_t filter;   /* 输入滤波器值 */
} xhal_tim_encoder_channel_t;

typedef struct xhal_tim_encoder_config
{
    xhal_tim_encoder_channel_t channel1; /* 通道1配置 */
    xhal_tim_encoder_channel_t channel2; /* 通道2配置 */
} xhal_tim_encoder_config_t;

/* 普通定时器配置结构体 */
typedef struct xhal_tim_normal_config
{
    uint16_t period;
    uint16_t prescaler;
    uint16_t compare[4];
} xhal_tim_normal_config_t;

/* 统一的定时器配置结构体 */
typedef struct xhal_tim_config
{
    union
    {
        xhal_tim_normal_config_t normal;
        xhal_tim_pwm_config_t pwm;
        xhal_tim_encoder_config_t encoder;
    } setting;

    uint8_t mode;
} xhal_tim_config_t;

typedef struct xhal_tim xhal_tim_t;

typedef struct xhal_tim_ops
{
    xhal_err_t (*init)(xhal_tim_t *self);
    xhal_err_t (*start)(xhal_tim_t *self);
    xhal_err_t (*stop)(xhal_tim_t *self);
    xhal_err_t (*get_count)(xhal_tim_t *self, uint16_t *count);
    xhal_err_t (*set_count)(xhal_tim_t *self, uint16_t count);
    xhal_err_t (*enable_irq)(xhal_tim_t *self, xhal_tim_it_t it);
    xhal_err_t (*disable_irq)(xhal_tim_t *self, xhal_tim_it_t it);
    xhal_err_t (*set_irq_callback)(xhal_tim_t *self, xhal_tim_cb_t cb);

    xhal_err_t (*pwm_set_duty_cycle)(xhal_tim_t *self, uint8_t channel,
                                     uint16_t duty_cycle);
    xhal_err_t (*pwm_set_period)(xhal_tim_t *self, uint16_t period);
    xhal_err_t (*pwm_set_prescaler)(xhal_tim_t *self, uint16_t prescaler);
    xhal_err_t (*pwm_enable_channel)(xhal_tim_t *self, uint8_t channel);
    xhal_err_t (*pwm_disable_channel)(xhal_tim_t *self, uint8_t channel);

    xhal_err_t (*encoder_get_position)(xhal_tim_t *self, int16_t *position);
    xhal_err_t (*encoder_get_delta)(xhal_tim_t *self, int16_t *delta);
    xhal_err_t (*encoder_reset_count)(xhal_tim_t *self);

    xhal_err_t (*normal_set_period)(xhal_tim_t *self, uint16_t period);
    xhal_err_t (*normal_set_prescaler)(xhal_tim_t *self, uint16_t prescaler);
    xhal_err_t (*normal_set_compare)(xhal_tim_t *self, uint8_t channel,
                                     uint16_t compare);
} xhal_tim_ops_t;

typedef struct xhal_tim_data
{
    xhal_tim_config_t config;
    xhal_tim_cb_t irq_callback;

    const char *name;
} xhal_tim_data_t;

typedef struct xhal_tim
{
    xhal_periph_t peri;
    const xhal_tim_ops_t *ops;
    xhal_tim_data_t data;
} xhal_tim_t;

#define XTIM_CAST(_dev) ((xhal_tim_t *)_dev)

xhal_err_t xtim_inst(xhal_tim_t *self, const char *name,
                     const xhal_tim_ops_t *ops,const char *tim_name,
                     const xhal_tim_config_t *config);

xhal_err_t xtim_start(xhal_periph_t *self);
xhal_err_t xtim_stop(xhal_periph_t *self);

xhal_err_t xtim_get_count(xhal_periph_t *self, uint16_t *count);
xhal_err_t xtim_set_count(xhal_periph_t *self, uint16_t count);
xhal_err_t xtim_enable_irq(xhal_periph_t *self, xhal_tim_it_t it);
xhal_err_t xtim_disable_irq(xhal_periph_t *self, xhal_tim_it_t it);
xhal_err_t xtim_set_irq_callback(xhal_periph_t *self, xhal_tim_cb_t cb);

xhal_err_t xtim_normal_set_period(xhal_periph_t *self, uint16_t period);
xhal_err_t xtim_normal_set_prescaler(xhal_periph_t *self, uint16_t prescaler);
xhal_err_t xtim_normal_set_compare(xhal_periph_t *self, uint8_t channel,
                                   uint16_t compare);

xhal_err_t xtim_pwm_set_duty_cycle(xhal_periph_t *self, uint8_t channel,
                                   uint16_t duty_cycle);
xhal_err_t xtim_pwm_set_period(xhal_periph_t *self, uint16_t period);
xhal_err_t xtim_pwm_set_prescaler(xhal_periph_t *self, uint16_t prescaler);
xhal_err_t xtim_pwm_enable_channel(xhal_periph_t *self, uint8_t channel);
xhal_err_t xtim_pwm_disable_channel(xhal_periph_t *self, uint8_t channel);

xhal_err_t xtim_encoder_get_position(xhal_periph_t *self, int16_t *position);
int16_t xtim_encoder_get_delta(xhal_periph_t *self);
xhal_err_t xtim_encoder_clear_position(xhal_periph_t *self);

#endif /* __XHAL_TIM_H */
