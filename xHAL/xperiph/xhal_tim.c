#include "xhal_tim.h"
#include "../xcore/xhal_assert.h"
#include "../xcore/xhal_log.h"

XLOG_TAG("xTim");

#define IS_XTIM_MODE(MODE)                                        \
    (((MODE) == XTIM_MODE_NORMAL) || ((MODE) == XTIM_MODE_PWM) || \
     ((MODE) == XTIM_MODE_ENCODER))

#define IS_XTIM_IT(IT)                                    \
    (((IT) == XTIM_IT_UPDATE) || ((IT) == XTIM_IT_CC1) || \
     ((IT) == XTIM_IT_CC2) || ((IT) == XTIM_IT_CC3) || ((IT) == XTIM_IT_CC4))

#define IS_XTIM_CHANNEL(CH)                                  \
    (((CH) == XTIM_CHANNEL_1) || ((CH) == XTIM_CHANNEL_2) || \
     ((CH) == XTIM_CHANNEL_3) || ((CH) == XTIM_CHANNEL_4))

#define IS_XTIM_OC_POLARITY(POL) \
    (((POL) == XTIM_OC_POLARITY_HIGH) || ((POL) == XTIM_OC_POLARITY_LOW))

#define IS_XTIM_OC_IDLE(IDLE) \
    (((IDLE) == XTIM_OC_IDLE_LOW) || ((IDLE) == XTIM_OC_IDLE_HIGH))

#define IS_XTIM_IC_POLARITY(POL)           \
    (((POL) == XTIM_IC_POLARITY_RISING) || \
     ((POL) == XTIM_IC_POLARITY_FALLING) || ((POL) == XTIM_IC_POLARITY_BOTH))

#define IS_XTIM_IC_FILTER(FILTER) ((FILTER) <= 0xF)

#define IS_XTIM_PWM_DUTY(DUTY)    ((DUTY) <= 10000)

#if defined(__GNUC__) || defined(__clang__)
#define XTIM_CHANNEL_TO_INDEX(ch) ((ch) ? (uint8_t)__builtin_ctz(ch) : 0)
#else
static inline uint8_t XTIM_CHANNEL_TO_INDEX(uint8_t ch)
{
    if (ch == 0)
        return 0;
    uint8_t i = 0;
    while ((ch >>= 1) != 0)
        i++;
    return i;
}
#endif

xhal_err_t xtim_inst(xhal_tim_t *self, const char *name,
                     const xhal_tim_ops_t *ops, const char *tim_name,
                     const xhal_tim_config_t *config)
{
    xassert_not_null(self);
    xassert_not_null(name);
    xassert_not_null(ops);
    xassert_not_null(config);
    xassert_name(IS_XTIM_MODE(config->mode), name);
    xassert_ptr_struct_not_null(ops, name);

    switch (config->mode)
    {
    case XTIM_MODE_PWM:

        for (uint8_t i = 0; i < 4; i++)
        {
            if (config->setting.pwm.channel_mask & (1U << i))
            {
                xassert_name(IS_XTIM_PWM_DUTY(
                                 config->setting.pwm.channels[i].duty_cycle),
                             name);
                xassert_name(IS_XTIM_OC_POLARITY(
                                 config->setting.pwm.channels[i].polarity),
                             name);
                xassert_name(
                    IS_XTIM_OC_IDLE(config->setting.pwm.channels[i].idle_state),
                    name);
            }
        }
        break;

    case XTIM_MODE_ENCODER:
        xassert_name(
            IS_XTIM_IC_POLARITY(config->setting.encoder.channel1.polarity),
            name);
        xassert_name(
            IS_XTIM_IC_POLARITY(config->setting.encoder.channel2.polarity),
            name);
        xassert_name(IS_XTIM_IC_FILTER(config->setting.encoder.channel1.filter),
                     name);
        xassert_name(IS_XTIM_IC_FILTER(config->setting.encoder.channel2.filter),
                     name);
        break;

    case XTIM_MODE_NORMAL:
        break;

    default:
        break;
    }

    xhal_tim_t *tim = self;

    xhal_periph_attr_t periph_attr = {
        .name = name,
        .type = XHAL_PERIPH_TIM,
    };
    xperiph_register(&tim->peri, &periph_attr);

    tim->ops               = ops;
    tim->data.config       = *config;
    tim->data.irq_callback = NULL;
    tim->data.name         = tim_name;

    xhal_err_t ret = tim->ops->init(tim);
    if (ret != XHAL_OK)
    {
        xperiph_unregister(&tim->peri);
        return ret;
    }
    tim->peri.is_inited = XPERIPH_INITED;

    return XHAL_OK;
}

xhal_err_t xtim_start(xhal_periph_t *self)
{
    xassert_not_null(self);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_TIM);

    xhal_tim_t *tim = XTIM_CAST(self);

    xperiph_lock(self);
    xhal_err_t ret = tim->ops->start(tim);
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xtim_stop(xhal_periph_t *self)
{
    xassert_not_null(self);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_TIM);

    xhal_tim_t *tim = XTIM_CAST(self);

    xperiph_lock(self);
    xhal_err_t ret = tim->ops->stop(tim);
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xtim_get_count(xhal_periph_t *self, uint16_t *count)
{
    xassert_not_null(self);
    xassert_not_null(count);

    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_TIM);

    xhal_tim_t *tim = XTIM_CAST(self);

    xperiph_lock(self);
    xhal_err_t ret = tim->ops->get_count(tim, count);
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xtim_set_count(xhal_periph_t *self, uint16_t count)
{
    xassert_not_null(self);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_TIM);

    xhal_tim_t *tim = XTIM_CAST(self);

    xperiph_lock(self);
    xhal_err_t ret = tim->ops->set_count(tim, count);
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xtim_enable_irq(xhal_periph_t *self, xhal_tim_it_t it)
{
    xassert_not_null(self);
    xassert_name(IS_XTIM_IT(it), self->attr.name);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_TIM);

    xhal_tim_t *tim = XTIM_CAST(self);

    xperiph_lock(self);
    xhal_err_t ret = tim->ops->enable_irq(tim, it);
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xtim_disable_irq(xhal_periph_t *self, xhal_tim_it_t it)
{
    xassert_not_null(self);
    xassert_name(IS_XTIM_IT(it), self->attr.name);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_TIM);

    xhal_tim_t *tim = XTIM_CAST(self);

    xperiph_lock(self);
    xhal_err_t ret = tim->ops->disable_irq(tim, it);
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xtim_set_irq_callback(xhal_periph_t *self, xhal_tim_cb_t cb)
{
    xassert_not_null(self);
    xassert_not_null(cb);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_TIM);

    xhal_err_t ret  = XHAL_OK;
    xhal_tim_t *tim = XTIM_CAST(self);

    xperiph_lock(self);
    if (cb != tim->data.irq_callback)
    {
        ret = tim->ops->set_irq_callback(tim, cb);
        if (ret == XHAL_OK)
        {
            tim->data.irq_callback = cb;
        }
    }
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xtim_normal_set_period(xhal_periph_t *self, uint16_t period)
{
    xassert_not_null(self);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_TIM);

    if (XTIM_CAST(self)->data.config.mode != XTIM_MODE_NORMAL)
        return XHAL_ERR_INVALID;

    xhal_err_t ret                          = XHAL_OK;
    xhal_tim_t *tim                         = XTIM_CAST(self);
    xhal_tim_normal_config_t *normal_config = &tim->data.config.setting.normal;

    xperiph_lock(self);
    if (period != normal_config->period)
    {
        ret = tim->ops->normal_set_period(tim, period);
        if (ret == XHAL_OK)
        {
            normal_config->period = period;
        }
    }
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xtim_normal_set_prescaler(xhal_periph_t *self, uint16_t prescaler)
{
    xassert_not_null(self);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_TIM);

    if (XTIM_CAST(self)->data.config.mode != XTIM_MODE_NORMAL)
        return XHAL_ERR_INVALID;

    xhal_err_t ret                          = XHAL_OK;
    xhal_tim_t *tim                         = XTIM_CAST(self);
    xhal_tim_normal_config_t *normal_config = &tim->data.config.setting.normal;

    xperiph_lock(self);
    if (prescaler != normal_config->prescaler)
    {
        ret = tim->ops->normal_set_prescaler(tim, prescaler);
        if (ret == XHAL_OK)
        {
            normal_config->prescaler = prescaler;
        }
    }
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xtim_normal_set_compare(xhal_periph_t *self, uint8_t channel,
                                   uint16_t compare)
{
    xassert_not_null(self);
    xassert_name(IS_XTIM_CHANNEL(channel), self->attr.name);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_TIM);

    if (XTIM_CAST(self)->data.config.mode != XTIM_MODE_NORMAL)
        return XHAL_ERR_INVALID;

    xhal_err_t ret                          = XHAL_OK;
    xhal_tim_t *tim                         = XTIM_CAST(self);
    xhal_tim_normal_config_t *normal_config = &tim->data.config.setting.normal;

    xperiph_lock(self);
    if (compare != normal_config->compare[XTIM_CHANNEL_TO_INDEX(channel)])
    {
        ret = tim->ops->normal_set_compare(tim, channel, compare);
        if (ret == XHAL_OK)
        {
            normal_config->compare[XTIM_CHANNEL_TO_INDEX(channel)] = compare;
        }
    }
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xtim_pwm_set_duty_cycle(xhal_periph_t *self, uint8_t channel,
                                   uint16_t duty_cycle)
{
    xassert_not_null(self);
    xassert_name(IS_XTIM_CHANNEL(channel), self->attr.name);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_TIM);

    if (XTIM_CAST(self)->data.config.mode != XTIM_MODE_PWM)
        return XHAL_ERR_INVALID;

    xhal_err_t ret                    = XHAL_OK;
    xhal_tim_t *tim                   = XTIM_CAST(self);
    xhal_tim_pwm_config_t *pwm_config = &tim->data.config.setting.pwm;

    xperiph_lock(self);
    if (!(pwm_config->channel_mask & channel))
    {
        ret = XHAL_ERR_INVALID;
        goto exit;
    }

    if (duty_cycle !=
        pwm_config->channels[XTIM_CHANNEL_TO_INDEX(channel)].duty_cycle)
    {
        ret = tim->ops->pwm_set_duty_cycle(tim, channel, duty_cycle);
        if (ret == XHAL_OK)
        {
            pwm_config->channels[XTIM_CHANNEL_TO_INDEX(channel)].duty_cycle =
                duty_cycle;
        }
    }

exit:
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xtim_pwm_set_period(xhal_periph_t *self, uint16_t period)
{
    xassert_not_null(self);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_TIM);

    if (XTIM_CAST(self)->data.config.mode != XTIM_MODE_PWM)
        return XHAL_ERR_INVALID;

    xhal_err_t ret                    = XHAL_OK;
    xhal_tim_t *tim                   = XTIM_CAST(self);
    xhal_tim_pwm_config_t *pwm_config = &tim->data.config.setting.pwm;

    xperiph_lock(self);
    if (period != pwm_config->period)
    {
        ret = tim->ops->pwm_set_period(tim, period);
        if (ret == XHAL_OK)
        {
            pwm_config->period = period;
        }
    }
    xperiph_unlock(self);

    return ret;
}
xhal_err_t xtim_pwm_set_prescaler(xhal_periph_t *self, uint16_t prescaler)
{
    xassert_not_null(self);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_TIM);

    if (XTIM_CAST(self)->data.config.mode != XTIM_MODE_PWM)
        return XHAL_ERR_INVALID;

    xhal_err_t ret                    = XHAL_OK;
    xhal_tim_t *tim                   = XTIM_CAST(self);
    xhal_tim_pwm_config_t *pwm_config = &tim->data.config.setting.pwm;

    xperiph_lock(self);
    if (prescaler != pwm_config->prescaler)
    {
        ret = tim->ops->pwm_set_prescaler(tim, prescaler);
        if (ret == XHAL_OK)
        {
            pwm_config->prescaler = prescaler;
        }
    }
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xtim_pwm_enable_channel(xhal_periph_t *self, uint8_t channel)
{
    xassert_not_null(self);
    xassert_name(IS_XTIM_CHANNEL(channel), self->attr.name);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_TIM);

    if (XTIM_CAST(self)->data.config.mode != XTIM_MODE_PWM)
        return XHAL_ERR_INVALID;

    xhal_err_t ret                    = XHAL_OK;
    xhal_tim_t *tim                   = XTIM_CAST(self);
    xhal_tim_pwm_config_t *pwm_config = &tim->data.config.setting.pwm;

    xperiph_lock(self);
    if (!(pwm_config->channel_mask & channel))
    {
        ret = XHAL_ERR_INVALID;
        goto exit;
    }

    ret = tim->ops->pwm_enable_channel(tim, channel);

exit:
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xtim_pwm_disable_channel(xhal_periph_t *self, uint8_t channel)
{
    xassert_not_null(self);
    xassert_name(IS_XTIM_CHANNEL(channel), self->attr.name);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_TIM);

    if (XTIM_CAST(self)->data.config.mode != XTIM_MODE_PWM)
        return XHAL_ERR_INVALID;

    xhal_err_t ret                    = XHAL_OK;
    xhal_tim_t *tim                   = XTIM_CAST(self);
    xhal_tim_pwm_config_t *pwm_config = &tim->data.config.setting.pwm;

    xperiph_lock(self);
    if (!(pwm_config->channel_mask & channel))
    {
        ret = XHAL_ERR_INVALID;
        goto exit;
    }

    ret = tim->ops->pwm_disable_channel(tim, channel);

exit:
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xtim_encoder_get_position(xhal_periph_t *self, int16_t *position)
{
    xassert_not_null(self);
    xassert_not_null(position);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_TIM);

    if (XTIM_CAST(self)->data.config.mode != XTIM_MODE_ENCODER)
        return XHAL_ERR_INVALID;

    xhal_tim_t *tim = XTIM_CAST(self);

    xperiph_lock(self);
    xhal_err_t ret = tim->ops->encoder_get_position(tim, position);
    xperiph_unlock(self);

    return ret;
}

int16_t xtim_encoder_get_delta(xhal_periph_t *self)
{
    xassert_not_null(self);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_TIM);

    if (XTIM_CAST(self)->data.config.mode != XTIM_MODE_ENCODER)
        return XHAL_ERR_INVALID;

    xhal_err_t ret  = XHAL_OK;
    xhal_tim_t *tim = XTIM_CAST(self);

    xperiph_lock(self);
    int16_t delta = 0;
    ret           = tim->ops->encoder_get_delta(tim, &delta);
    xperiph_unlock(self);

    return ret == XHAL_OK ? delta : 0;
}

xhal_err_t xtim_encoder_clear_position(xhal_periph_t *self)
{
    xassert_not_null(self);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_TIM);

    if (XTIM_CAST(self)->data.config.mode != XTIM_MODE_ENCODER)
        return XHAL_ERR_INVALID;

    xhal_tim_t *tim = XTIM_CAST(self);
    xperiph_lock(self);
    xhal_err_t ret = tim->ops->encoder_reset_count(tim);
    xperiph_unlock(self);

    return ret;
}
