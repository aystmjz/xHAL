#include "drv_util.h"
#include "xhal_tim.h"
#include <ctype.h>
#include <string.h>

XLOG_TAG("xDriverTIM");

static xhal_tim_t *tim_p[4] = {NULL}; /* TIM1-TIM4 */

static xhal_err_t _init(xhal_tim_t *self);
static xhal_err_t _start(xhal_tim_t *self);
static xhal_err_t _stop(xhal_tim_t *self);
static xhal_err_t _get_count(xhal_tim_t *self, uint16_t *count);
static xhal_err_t _set_count(xhal_tim_t *self, uint16_t count);
static xhal_err_t _enable_irq(xhal_tim_t *self, xhal_tim_it_t it);
static xhal_err_t _disable_irq(xhal_tim_t *self, xhal_tim_it_t it);
static xhal_err_t _set_irq_callback(xhal_tim_t *self, xhal_tim_cb_t cb);

static xhal_err_t _pwm_set_duty_cycle(xhal_tim_t *self, uint8_t channel,
                                      uint16_t duty_cycle);
static xhal_err_t _pwm_set_period(xhal_tim_t *self, uint16_t period);
static xhal_err_t _pwm_set_prescaler(xhal_tim_t *self, uint16_t prescaler);
static xhal_err_t _pwm_enable_channel(xhal_tim_t *self, uint8_t channel);
static xhal_err_t _pwm_disable_channel(xhal_tim_t *self, uint8_t channel);

static xhal_err_t _encoder_get_position(xhal_tim_t *self, int16_t *position);
static xhal_err_t _encoder_get_delta(xhal_tim_t *self, int16_t *delta);
static xhal_err_t _encoder_clear_position(xhal_tim_t *self);

static xhal_err_t _normal_set_period(xhal_tim_t *self, uint16_t period);
static xhal_err_t _normal_set_prescaler(xhal_tim_t *self, uint16_t prescaler);
static xhal_err_t _normal_set_compare(xhal_tim_t *self, uint8_t channel,
                                      uint16_t compare);

const xhal_tim_ops_t tim_hw_ops_driver = {
    .init      = _init,
    .start     = _start,
    .stop      = _stop,
    .get_count = _get_count,
    .set_count = _set_count,

    .enable_irq       = _enable_irq,
    .disable_irq      = _disable_irq,
    .set_irq_callback = _set_irq_callback,

    .pwm_set_duty_cycle  = _pwm_set_duty_cycle,
    .pwm_set_period      = _pwm_set_period,
    .pwm_set_prescaler   = _pwm_set_prescaler,
    .pwm_enable_channel  = _pwm_enable_channel,
    .pwm_disable_channel = _pwm_disable_channel,

    .encoder_get_position = _encoder_get_position,
    .encoder_reset_count  = _encoder_clear_position,
    .encoder_get_delta    = _encoder_get_delta,

    .normal_set_period    = _normal_set_period,
    .normal_set_prescaler = _normal_set_prescaler,
    .normal_set_compare   = _normal_set_compare};

typedef struct tim_hw_info tim_hw_info_t;

static xhal_err_t _pwm_config(xhal_tim_t *self);
static xhal_err_t _encoder_config(xhal_tim_t *self);
static xhal_err_t _normal_config(xhal_tim_t *self);
static void _tim_gpio_msp_init(xhal_tim_t *self);
static void _tim_irq_msp_init(xhal_tim_t *self);
static bool _check_tim_name_valid(const char *name);
static const tim_hw_info_t *_find_tim_info(const char *name);
static uint16_t _get_oc_polarity(uint8_t polarity);
static uint16_t _get_oc_idle_state(uint8_t idle_state);
static uint16_t _get_ic_polarity(uint8_t polarity);

typedef struct tim_hw_info
{
    uint8_t id;       /* 定时器编号 */
    TIM_TypeDef *tim; /* 定时器基地址 */

    /* PWM模式引脚配置 */
    struct
    {
        GPIO_TypeDef *port;
        uint16_t pin;
        uint32_t clk;
    } channels[4]; /* 4个通道的GPIO配置 */

    /* 编码器模式引脚配置 */
    struct
    {
        GPIO_TypeDef *port;
        uint16_t pin;
        uint32_t clk;
    } encoder_ch1, encoder_ch2;

    uint32_t tim_clk;        /* 定时器时钟 */
    IRQn_Type irq_tim_up;    /* 定时器更新中断号 */
    IRQn_Type irq_tim_cc;    /* 定时器捕获比较中断号 */
    uint8_t irq_tim_up_prio; /* 定时器更新中断优先级 */
    uint8_t irq_tim_cc_prio; /* 时器捕获比较中断优先级 */
} tim_hw_info_t;

static const tim_hw_info_t tim_table[] = {
    /* TIM1 - 高级定时器 */
    {
        .id              = 0,
        .tim             = TIM1,
        .channels        = {{GPIOA, GPIO_Pin_8, RCC_APB2Periph_GPIOA},
                            {GPIOA, GPIO_Pin_9, RCC_APB2Periph_GPIOA},
                            {GPIOA, GPIO_Pin_10, RCC_APB2Periph_GPIOA},
                            {GPIOA, GPIO_Pin_11, RCC_APB2Periph_GPIOA}},
        .encoder_ch1     = {GPIOA, GPIO_Pin_8, RCC_APB2Periph_GPIOA},
        .encoder_ch2     = {GPIOA, GPIO_Pin_9, RCC_APB2Periph_GPIOA},
        .tim_clk         = RCC_APB2Periph_TIM1,
        .irq_tim_up      = TIM1_UP_IRQn, /* 更新中断 */
        .irq_tim_cc      = TIM1_CC_IRQn, /* 新增：捕获比较中断 */
        .irq_tim_up_prio = 6,
        .irq_tim_cc_prio = 6 /* 新增：CC中断优先级 */
    },
    /* TIM2 - 通用定时器 */
    {.id              = 1,
     .tim             = TIM2,
     .channels        = {{GPIOA, GPIO_Pin_0, RCC_APB2Periph_GPIOA},
                         {GPIOA, GPIO_Pin_1, RCC_APB2Periph_GPIOA},
                         {GPIOA, GPIO_Pin_2, RCC_APB2Periph_GPIOA},
                         {GPIOA, GPIO_Pin_3, RCC_APB2Periph_GPIOA}},
     .encoder_ch1     = {GPIOA, GPIO_Pin_0, RCC_APB2Periph_GPIOA},
     .encoder_ch2     = {GPIOA, GPIO_Pin_1, RCC_APB2Periph_GPIOA},
     .tim_clk         = RCC_APB1Periph_TIM2,
     .irq_tim_up      = TIM2_IRQn,
     .irq_tim_cc      = TIM2_IRQn, /* 通用定时器使用同一个中断 */
     .irq_tim_up_prio = 6,
     .irq_tim_cc_prio = 6},
    /* TIM3 - 通用定时器 */
    {.id              = 2,
     .tim             = TIM3,
     .channels        = {{GPIOA, GPIO_Pin_6, RCC_APB2Periph_GPIOA},
                         {GPIOA, GPIO_Pin_7, RCC_APB2Periph_GPIOA},
                         {GPIOB, GPIO_Pin_0, RCC_APB2Periph_GPIOB},
                         {GPIOB, GPIO_Pin_1, RCC_APB2Periph_GPIOB}},
     .encoder_ch1     = {GPIOA, GPIO_Pin_6, RCC_APB2Periph_GPIOA},
     .encoder_ch2     = {GPIOA, GPIO_Pin_7, RCC_APB2Periph_GPIOA},
     .tim_clk         = RCC_APB1Periph_TIM3,
     .irq_tim_up      = TIM3_IRQn,
     .irq_tim_cc      = TIM3_IRQn,
     .irq_tim_up_prio = 6,
     .irq_tim_cc_prio = 6},
    /* TIM4 - 通用定时器 */
    {.id              = 3,
     .tim             = TIM4,
     .channels        = {{GPIOB, GPIO_Pin_6, RCC_APB2Periph_GPIOB},
                         {GPIOB, GPIO_Pin_7, RCC_APB2Periph_GPIOB},
                         {GPIOB, GPIO_Pin_8, RCC_APB2Periph_GPIOB},
                         {GPIOB, GPIO_Pin_9, RCC_APB2Periph_GPIOB}},
     .encoder_ch1     = {GPIOB, GPIO_Pin_6, RCC_APB2Periph_GPIOB},
     .encoder_ch2     = {GPIOB, GPIO_Pin_7, RCC_APB2Periph_GPIOB},
     .tim_clk         = RCC_APB1Periph_TIM4,
     .irq_tim_up      = TIM4_IRQn,
     .irq_tim_cc      = TIM4_IRQn,
     .irq_tim_up_prio = 6,
     .irq_tim_cc_prio = 6}};
static xhal_err_t _init(xhal_tim_t *self)
{
    xassert_name(_check_tim_name_valid(self->data.name), self->data.name);

    xhal_err_t ret            = XHAL_OK;
    const tim_hw_info_t *info = _find_tim_info(self->data.name);
    tim_p[info->id]           = self;

    if (info->tim_clk == RCC_APB2Periph_TIM1)
    {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    }
    else
    {
        RCC_APB1PeriphClockCmd(info->tim_clk, ENABLE);
    }

    _tim_gpio_msp_init(self);

    switch (self->data.config.mode)
    {
    case XTIM_MODE_PWM:
        ret = _pwm_config(self);
        break;
    case XTIM_MODE_ENCODER:
        ret = _encoder_config(self);
        break;
    case XTIM_MODE_NORMAL:
        ret = _normal_config(self);
        break;
    default:
        break;
    }

    _tim_irq_msp_init(self);

    _stop(self);

    return ret;
}

static xhal_err_t _pwm_config(xhal_tim_t *self)
{
    const tim_hw_info_t *info               = _find_tim_info(self->data.name);
    const xhal_tim_pwm_config_t *pwm_config = &self->data.config.setting.pwm;

    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_TimeBaseStructure.TIM_Period            = pwm_config->period;
    TIM_TimeBaseStructure.TIM_Prescaler         = pwm_config->prescaler;
    TIM_TimeBaseStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;

    TIM_TimeBaseInit(info->tim, &TIM_TimeBaseStructure);

    /* 配置PWM通道 */
    for (uint8_t i = 0; i < 4; i++)
    {
        const xhal_tim_pwm_channel_t *channel = &pwm_config->channels[i];

        if (pwm_config->channel_mask & (1U << i))
        {
            TIM_OCInitTypeDef TIM_OCInitStructure;
            TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
            TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
            TIM_OCInitStructure.TIM_Pulse =
                (channel->duty_cycle * pwm_config->period) / 10000;
            TIM_OCInitStructure.TIM_OCPolarity =
                _get_oc_polarity(channel->polarity);
            TIM_OCInitStructure.TIM_OCIdleState =
                _get_oc_idle_state(channel->idle_state);

            switch (i)
            {
            case 0:
                TIM_OC1Init(info->tim, &TIM_OCInitStructure);
                TIM_OC1PreloadConfig(info->tim, TIM_OCPreload_Enable);
                TIM_CCxCmd(info->tim, TIM_Channel_1, TIM_CCx_Disable);
                break;
            case 1:
                TIM_OC2Init(info->tim, &TIM_OCInitStructure);
                TIM_OC2PreloadConfig(info->tim, TIM_OCPreload_Enable);
                TIM_CCxCmd(info->tim, TIM_Channel_2, TIM_CCx_Disable);
                break;
            case 2:
                TIM_OC3Init(info->tim, &TIM_OCInitStructure);
                TIM_OC3PreloadConfig(info->tim, TIM_OCPreload_Enable);
                TIM_CCxCmd(info->tim, TIM_Channel_3, TIM_CCx_Disable);
                break;
            case 3:
                TIM_OC4Init(info->tim, &TIM_OCInitStructure);
                TIM_OC4PreloadConfig(info->tim, TIM_OCPreload_Enable);
                TIM_CCxCmd(info->tim, TIM_Channel_4, TIM_CCx_Disable);
                break;
            }
        }
    }

    TIM_ARRPreloadConfig(info->tim, ENABLE);

    return XHAL_OK;
}

static xhal_err_t _encoder_config(xhal_tim_t *self)
{
    const tim_hw_info_t *info = _find_tim_info(self->data.name);
    const xhal_tim_encoder_config_t *encoder_config =
        &self->data.config.setting.encoder;

    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_TimeBaseStructure.TIM_Period            = 0xFFFF;
    TIM_TimeBaseStructure.TIM_Prescaler         = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;

    TIM_TimeBaseInit(info->tim, &TIM_TimeBaseStructure);

    TIM_EncoderInterfaceConfig(
        info->tim, TIM_EncoderMode_TI12,
        _get_ic_polarity(encoder_config->channel1.polarity),
        _get_ic_polarity(encoder_config->channel2.polarity));

    TIM_ICInitTypeDef TIM_ICInitStructure;
    TIM_ICInitStructure.TIM_ICFilter = encoder_config->channel1.filter;
    TIM_ICInitStructure.TIM_Channel  = TIM_Channel_1;
    TIM_ICInit(info->tim, &TIM_ICInitStructure);

    TIM_ICInitStructure.TIM_ICFilter = encoder_config->channel2.filter;
    TIM_ICInitStructure.TIM_Channel  = TIM_Channel_2;
    TIM_ICInit(info->tim, &TIM_ICInitStructure);

    return XHAL_OK;
}

static xhal_err_t _normal_config(xhal_tim_t *self)
{
    const tim_hw_info_t *info = _find_tim_info(self->data.name);
    const xhal_tim_normal_config_t *normal_config =
        &self->data.config.setting.normal;

    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_TimeBaseStructure.TIM_Period            = normal_config->period;
    TIM_TimeBaseStructure.TIM_Prescaler         = normal_config->prescaler;
    TIM_TimeBaseStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;

    TIM_TimeBaseInit(info->tim, &TIM_TimeBaseStructure);

    TIM_ARRPreloadConfig(info->tim, ENABLE);

    return XHAL_OK;
}

static xhal_err_t _start(xhal_tim_t *self)
{
    const tim_hw_info_t *info = _find_tim_info(self->data.name);
    TIM_Cmd(info->tim, ENABLE);

    return XHAL_OK;
}

static xhal_err_t _stop(xhal_tim_t *self)
{
    const tim_hw_info_t *info = _find_tim_info(self->data.name);
    TIM_Cmd(info->tim, DISABLE);

    return XHAL_OK;
}

static xhal_err_t _get_count(xhal_tim_t *self, uint16_t *count)
{
    const tim_hw_info_t *info = _find_tim_info(self->data.name);
    *count                    = TIM_GetCounter(info->tim);

    return XHAL_OK;
}

static xhal_err_t _set_count(xhal_tim_t *self, uint16_t count)
{
    const tim_hw_info_t *info = _find_tim_info(self->data.name);
    TIM_SetCounter(info->tim, count);

    return XHAL_OK;
}

static xhal_err_t _enable_irq(xhal_tim_t *self, xhal_tim_it_t it)
{
    const tim_hw_info_t *info = _find_tim_info(self->data.name);

    switch (it)
    {
    case XTIM_IT_UPDATE:
        TIM_ITConfig(info->tim, TIM_IT_Update, ENABLE);
        break;
    case XTIM_IT_CC1:
        TIM_ITConfig(info->tim, TIM_IT_CC1, ENABLE);
        break;
    case XTIM_IT_CC2:
        TIM_ITConfig(info->tim, TIM_IT_CC2, ENABLE);
        break;
    case XTIM_IT_CC3:
        TIM_ITConfig(info->tim, TIM_IT_CC3, ENABLE);
        break;
    case XTIM_IT_CC4:
        TIM_ITConfig(info->tim, TIM_IT_CC4, ENABLE);
        break;
    default:
        break;
    }

    return XHAL_OK;
}

static xhal_err_t _disable_irq(xhal_tim_t *self, xhal_tim_it_t it)
{
    const tim_hw_info_t *info = _find_tim_info(self->data.name);

    switch (it)
    {
    case XTIM_IT_UPDATE:
        TIM_ITConfig(info->tim, TIM_IT_Update, DISABLE);
        break;
    case XTIM_IT_CC1:
        TIM_ITConfig(info->tim, TIM_IT_CC1, DISABLE);
        break;
    case XTIM_IT_CC2:
        TIM_ITConfig(info->tim, TIM_IT_CC2, DISABLE);
        break;
    case XTIM_IT_CC3:
        TIM_ITConfig(info->tim, TIM_IT_CC3, DISABLE);
        break;
    case XTIM_IT_CC4:
        TIM_ITConfig(info->tim, TIM_IT_CC4, DISABLE);
        break;
    default:
        break;
    }

    return XHAL_OK;
}

static xhal_err_t _set_irq_callback(xhal_tim_t *self, xhal_tim_cb_t cb)
{
    self->data.irq_callback = cb;

    return XHAL_OK;
}

static xhal_err_t _pwm_set_duty_cycle(xhal_tim_t *self, uint8_t channel,
                                      uint16_t duty_cycle)
{
    const tim_hw_info_t *info               = _find_tim_info(self->data.name);
    const xhal_tim_pwm_config_t *pwm_config = &self->data.config.setting.pwm;

    uint16_t compare_value = (duty_cycle * pwm_config->period) / 10000;

    _normal_set_compare(self, channel, compare_value);

    return XHAL_OK;
}

static xhal_err_t _pwm_set_period(xhal_tim_t *self, uint16_t period)
{
    const tim_hw_info_t *info = _find_tim_info(self->data.name);

    TIM_SetAutoreload(info->tim, period);

    return XHAL_OK;
}
static xhal_err_t _pwm_set_prescaler(xhal_tim_t *self, uint16_t prescaler)
{
    const tim_hw_info_t *info = _find_tim_info(self->data.name);

    TIM_PrescalerConfig(info->tim, prescaler, TIM_PSCReloadMode_Update);

    return XHAL_OK;
}

static xhal_err_t _pwm_enable_channel(xhal_tim_t *self, uint8_t channel)
{
    static const uint16_t TIM_Channel_Map[4] = {TIM_Channel_1, TIM_Channel_2,
                                                TIM_Channel_3, TIM_Channel_4};

    const tim_hw_info_t *info = _find_tim_info(self->data.name);

    for (uint8_t i = 0; i < 4; i++)
    {
        if (channel & (1U << i))
        {
            TIM_CCxCmd(info->tim, TIM_Channel_Map[i], TIM_CCx_Enable);
        }
    }

    if (info->tim == TIM1)
    {
        TIM_CtrlPWMOutputs(info->tim, ENABLE);
    }

    return XHAL_OK;
}

static xhal_err_t _pwm_disable_channel(xhal_tim_t *self, uint8_t channel)
{
    static const uint16_t TIM_Channel_Map[4] = {TIM_Channel_1, TIM_Channel_2,
                                                TIM_Channel_3, TIM_Channel_4};

    const tim_hw_info_t *info = _find_tim_info(self->data.name);

    for (uint8_t i = 0; i < 4; i++)
    {
        if (channel & (1U << i))
        {
            TIM_CCxCmd(info->tim, TIM_Channel_Map[i], TIM_CCx_Disable);
        }
    }

    return XHAL_OK;
}

static xhal_err_t _encoder_get_position(xhal_tim_t *self, int16_t *position)
{
    return _get_count(self, (uint16_t *)position);
}

static xhal_err_t _encoder_get_delta(xhal_tim_t *self, int16_t *delta)
{
    static int16_t last_count[4] = {0};

    const tim_hw_info_t *info = _find_tim_info(self->data.name);
    int16_t current_count     = 0;

    xhal_err_t ret = XHAL_OK;

    ret = _encoder_get_position(self, &current_count);

    *delta               = current_count - last_count[info->id];
    last_count[info->id] = current_count;

    return ret;
}

static xhal_err_t _encoder_clear_position(xhal_tim_t *self)
{
    return _set_count(self, 0);
}

static xhal_err_t _normal_set_period(xhal_tim_t *self, uint16_t period)
{
    const tim_hw_info_t *info = _find_tim_info(self->data.name);

    TIM_SetAutoreload(info->tim, period);

    return XHAL_OK;
}

static xhal_err_t _normal_set_prescaler(xhal_tim_t *self, uint16_t prescaler)
{
    const tim_hw_info_t *info = _find_tim_info(self->data.name);

    TIM_PrescalerConfig(info->tim, prescaler, TIM_PSCReloadMode_Update);

    return XHAL_OK;
}

static xhal_err_t _normal_set_compare(xhal_tim_t *self, uint8_t channel,
                                      uint16_t compare)
{
    const tim_hw_info_t *info = _find_tim_info(self->data.name);

    for (uint8_t i = 0; i < 4; i++)
    {
        if (channel & (1U << i))
        {
            switch (i)
            {
            case 0:
                TIM_SetCompare1(info->tim, compare);
                break;

            case 1:
                TIM_SetCompare2(info->tim, compare);
                break;

            case 2:
                TIM_SetCompare3(info->tim, compare);
                break;

            case 3:
                TIM_SetCompare4(info->tim, compare);
                break;
            }
        }
    }

    return XHAL_OK;
}

static void _tim_gpio_msp_init(xhal_tim_t *self)
{
    const tim_hw_info_t *info = _find_tim_info(self->data.name);

    switch (self->data.config.mode)
    {
    case XTIM_MODE_PWM:

        for (uint8_t i = 0; i < 4; i++)
        {
            const xhal_tim_pwm_config_t *pwm_config =
                &self->data.config.setting.pwm;
            if (pwm_config->channel_mask & (1U << i))
            {
                RCC_APB2PeriphClockCmd(info->channels[i].clk, ENABLE);

                GPIO_InitTypeDef GPIO_InitStructure;
                GPIO_InitStructure.GPIO_Pin   = info->channels[i].pin;
                GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
                GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
                GPIO_Init(info->channels[i].port, &GPIO_InitStructure);
            }
        }
        break;

    case XTIM_MODE_ENCODER:

        RCC_APB2PeriphClockCmd(info->encoder_ch1.clk, ENABLE);
        RCC_APB2PeriphClockCmd(info->encoder_ch2.clk, ENABLE);

        GPIO_InitTypeDef GPIO_InitStructure;
        GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

        GPIO_InitStructure.GPIO_Pin = info->encoder_ch1.pin;
        GPIO_Init(info->encoder_ch1.port, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = info->encoder_ch2.pin;
        GPIO_Init(info->encoder_ch2.port, &GPIO_InitStructure);
        break;

    case XTIM_MODE_NORMAL:
        break;
    default:
        break;
    }
}

static void _tim_irq_msp_init(xhal_tim_t *self)
{
    const tim_hw_info_t *info = _find_tim_info(self->data.name);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = info->irq_tim_up;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority =
        info->irq_tim_up_prio;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd         = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = info->irq_tim_cc;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority =
        info->irq_tim_cc_prio;
    NVIC_Init(&NVIC_InitStructure);
}

static bool _check_tim_name_valid(const char *name)
{
    uint8_t len = strlen(name);
    if (len != 4)
    {
        return false;
    }

    if (toupper(name[0]) != 'T' || toupper(name[1]) != 'I' ||
        toupper(name[2]) != 'M')
    {
        return false;
    }

    if (!(name[3] >= '1' && name[3] <= '4'))
    {
        return false;
    }

    return true;
}

static const tim_hw_info_t *_find_tim_info(const char *name)
{
    uint8_t len = strlen(name);

    return &tim_table[name[len - 1] - '1'];
}

static uint16_t _get_oc_polarity(uint8_t polarity)
{
    return (polarity == XTIM_OC_POLARITY_HIGH) ? TIM_OCPolarity_High
                                               : TIM_OCPolarity_Low;
}

static uint16_t _get_oc_idle_state(uint8_t idle_state)
{
    return (idle_state == XTIM_OC_IDLE_HIGH) ? TIM_OCIdleState_Set
                                             : TIM_OCIdleState_Reset;
}

static uint16_t _get_ic_polarity(uint8_t polarity)
{
    switch (polarity)
    {
    case XTIM_IC_POLARITY_RISING:
        return TIM_ICPolarity_Rising;
    case XTIM_IC_POLARITY_FALLING:
        return TIM_ICPolarity_Falling;
    case XTIM_IC_POLARITY_BOTH:
        return TIM_ICPolarity_BothEdge;
    default:
        return TIM_ICPolarity_Rising;
    }
}

void TIM1_UP_IRQHandler(void)
{
    const tim_hw_info_t *info = &tim_table[0];
    xhal_tim_t *tim           = tim_p[info->id];

    if (tim == NULL)
        return;

    if (TIM_GetITStatus(info->tim, TIM_IT_Update) == SET)
    {
        TIM_ClearITPendingBit(info->tim, TIM_IT_Update);
        if (tim->data.irq_callback)
        {
            tim->data.irq_callback(XTIM_IT_UPDATE);
        }
    }
}

void TIM1_CC_IRQHandler(void)
{
    const tim_hw_info_t *info = &tim_table[0];
    xhal_tim_t *tim           = tim_p[info->id];

    if (tim == NULL)
        return;

    if (TIM_GetITStatus(info->tim, TIM_IT_CC1) == SET)
    {
        TIM_ClearITPendingBit(info->tim, TIM_IT_CC1);
        if (tim->data.irq_callback)
        {
            tim->data.irq_callback(XTIM_IT_CC1);
        }
    }

    if (TIM_GetITStatus(info->tim, TIM_IT_CC2) == SET)
    {
        TIM_ClearITPendingBit(info->tim, TIM_IT_CC2);
        if (tim->data.irq_callback)
        {
            tim->data.irq_callback(XTIM_IT_CC2);
        }
    }

    if (TIM_GetITStatus(info->tim, TIM_IT_CC3) == SET)
    {
        TIM_ClearITPendingBit(info->tim, TIM_IT_CC3);
        if (tim->data.irq_callback)
        {
            tim->data.irq_callback(XTIM_IT_CC3);
        }
    }

    if (TIM_GetITStatus(info->tim, TIM_IT_CC4) == SET)
    {
        TIM_ClearITPendingBit(info->tim, TIM_IT_CC4);
        if (tim && tim->data.irq_callback)
        {
            tim->data.irq_callback(XTIM_IT_CC4);
        }
    }
}

void TIM2_IRQHandler(void)
{
    const tim_hw_info_t *info = &tim_table[1];
    xhal_tim_t *tim           = tim_p[info->id];

    if (tim == NULL)
        return;

    if (TIM_GetITStatus(info->tim, TIM_IT_Update) == SET)
    {
        TIM_ClearITPendingBit(info->tim, TIM_IT_Update);
        if (tim->data.irq_callback)
        {
            tim->data.irq_callback(XTIM_IT_UPDATE);
        }
    }

    if (TIM_GetITStatus(info->tim, TIM_IT_CC1) == SET)
    {
        TIM_ClearITPendingBit(info->tim, TIM_IT_CC1);
        if (tim->data.irq_callback)
        {
            tim->data.irq_callback(XTIM_IT_CC1);
        }
    }

    if (TIM_GetITStatus(info->tim, TIM_IT_CC2) == SET)
    {
        TIM_ClearITPendingBit(info->tim, TIM_IT_CC2);
        if (tim->data.irq_callback)
        {
            tim->data.irq_callback(XTIM_IT_CC2);
        }
    }

    if (TIM_GetITStatus(info->tim, TIM_IT_CC3) == SET)
    {
        TIM_ClearITPendingBit(info->tim, TIM_IT_CC3);
        if (tim->data.irq_callback)
        {
            tim->data.irq_callback(XTIM_IT_CC3);
        }
    }

    if (TIM_GetITStatus(info->tim, TIM_IT_CC4) == SET)
    {
        TIM_ClearITPendingBit(info->tim, TIM_IT_CC4);
        if (tim->data.irq_callback)
        {
            tim->data.irq_callback(XTIM_IT_CC4);
        }
    }
}

void TIM3_IRQHandler(void)
{
    const tim_hw_info_t *info = &tim_table[2];
    xhal_tim_t *tim           = tim_p[info->id];

    if (tim == NULL)
        return;

    if (TIM_GetITStatus(info->tim, TIM_IT_Update) == SET)
    {
        TIM_ClearITPendingBit(info->tim, TIM_IT_Update);
        if (tim->data.irq_callback)
        {
            tim->data.irq_callback(XTIM_IT_UPDATE);
        }
    }

    if (TIM_GetITStatus(info->tim, TIM_IT_CC1) == SET)
    {
        TIM_ClearITPendingBit(info->tim, TIM_IT_CC1);
        if (tim->data.irq_callback)
        {
            tim->data.irq_callback(XTIM_IT_CC1);
        }
    }

    if (TIM_GetITStatus(info->tim, TIM_IT_CC2) == SET)
    {
        TIM_ClearITPendingBit(info->tim, TIM_IT_CC2);
        if (tim->data.irq_callback)
        {
            tim->data.irq_callback(XTIM_IT_CC2);
        }
    }

    if (TIM_GetITStatus(info->tim, TIM_IT_CC3) == SET)
    {
        TIM_ClearITPendingBit(info->tim, TIM_IT_CC3);
        if (tim->data.irq_callback)
        {
            tim->data.irq_callback(XTIM_IT_CC3);
        }
    }

    if (TIM_GetITStatus(info->tim, TIM_IT_CC4) == SET)
    {
        TIM_ClearITPendingBit(info->tim, TIM_IT_CC4);
        if (tim->data.irq_callback)
        {
            tim->data.irq_callback(XTIM_IT_CC4);
        }
    }
}

void TIM4_IRQHandler(void)
{
    const tim_hw_info_t *info = &tim_table[3];
    xhal_tim_t *tim           = tim_p[info->id];

    if (tim == NULL)
        return;

    if (TIM_GetITStatus(info->tim, TIM_IT_Update) == SET)
    {
        TIM_ClearITPendingBit(info->tim, TIM_IT_Update);
        if (tim->data.irq_callback)
        {
            tim->data.irq_callback(XTIM_IT_UPDATE);
        }
    }

    if (TIM_GetITStatus(info->tim, TIM_IT_CC1) == SET)
    {
        TIM_ClearITPendingBit(info->tim, TIM_IT_CC1);
        if (tim->data.irq_callback)
        {
            tim->data.irq_callback(XTIM_IT_CC1);
        }
    }

    if (TIM_GetITStatus(info->tim, TIM_IT_CC2) == SET)
    {
        TIM_ClearITPendingBit(info->tim, TIM_IT_CC2);
        if (tim->data.irq_callback)
        {
            tim->data.irq_callback(XTIM_IT_CC2);
        }
    }

    if (TIM_GetITStatus(info->tim, TIM_IT_CC3) == SET)
    {
        TIM_ClearITPendingBit(info->tim, TIM_IT_CC3);
        if (tim->data.irq_callback)
        {
            tim->data.irq_callback(XTIM_IT_CC3);
        }
    }

    if (TIM_GetITStatus(info->tim, TIM_IT_CC4) == SET)
    {
        TIM_ClearITPendingBit(info->tim, TIM_IT_CC4);
        if (tim->data.irq_callback)
        {
            tim->data.irq_callback(XTIM_IT_CC4);
        }
    }
}
