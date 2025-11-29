#include "drv_util.h"
#include "xhal_exit.h"
#include <ctype.h>
#include <string.h>

XLOG_TAG("xDriverEXIT");

static xhal_exit_t *exit_p[16] = {NULL};

static xhal_err_t _init(xhal_exit_t *self);
static xhal_err_t _config(xhal_exit_t *self, const xhal_exit_config_t *config);
static xhal_err_t _enable_irq(xhal_exit_t *self);
static xhal_err_t _disable_irq(xhal_exit_t *self);
static xhal_err_t _set_irq_callback(xhal_exit_t *self, xhal_exit_cb_t cb);

const xhal_exit_ops_t exit_ops_driver = {
    .init             = _init,
    .config           = _config,
    .enable_irq       = _enable_irq,
    .disable_irq      = _disable_irq,
    .set_irq_callback = _set_irq_callback,
};

typedef struct exit_hw_info exit_hw_info_t;

static void _exit_irq_msp_init(xhal_exit_t *self);
static const exit_hw_info_t *_find_exit_info(const char *name);
static uint8_t _get_gpio_port_source(const char *name);
static uint8_t _get_gpio_pin_source(const char *name);

typedef struct exit_hw_info
{
    uint8_t id;         /* EXTI编号 */
    uint32_t exti_line; /* EXTI线 */
    IRQn_Type irq;      /* 中断号 */
    uint8_t irq_prio;   /* 中断优先级 */
} exit_hw_info_t;

static const exit_hw_info_t exit_table[] = {

    {.id        = 0,
     .exti_line = EXTI_Line0,
     .irq       = EXTI0_IRQn,
     .irq_prio  = 6}, /* EXTI0 */

    {.id        = 1,
     .exti_line = EXTI_Line1,
     .irq       = EXTI1_IRQn,
     .irq_prio  = 6}, /* EXTI1 */

    {.id        = 2,
     .exti_line = EXTI_Line2,
     .irq       = EXTI2_IRQn,
     .irq_prio  = 6}, /* EXTI2 */

    {.id        = 3,
     .exti_line = EXTI_Line3,
     .irq       = EXTI3_IRQn,
     .irq_prio  = 6}, /* EXTI3 */

    {.id        = 4,
     .exti_line = EXTI_Line4,
     .irq       = EXTI4_IRQn,
     .irq_prio  = 6}, /* EXTI4 */

    {.id        = 5,
     .exti_line = EXTI_Line5,
     .irq       = EXTI9_5_IRQn,
     .irq_prio  = 6}, /* EXTI5 */

    {.id        = 6,
     .exti_line = EXTI_Line6,
     .irq       = EXTI9_5_IRQn,
     .irq_prio  = 6}, /* EXTI6 */

    {.id        = 7,
     .exti_line = EXTI_Line7,
     .irq       = EXTI9_5_IRQn,
     .irq_prio  = 6}, /* EXTI7 */

    {.id        = 8,
     .exti_line = EXTI_Line8,
     .irq       = EXTI9_5_IRQn,
     .irq_prio  = 6}, /* EXTI8 */

    {.id        = 9,
     .exti_line = EXTI_Line9,
     .irq       = EXTI9_5_IRQn,
     .irq_prio  = 6}, /* EXTI9 */

    {.id        = 10,
     .exti_line = EXTI_Line10,
     .irq       = EXTI15_10_IRQn,
     .irq_prio  = 6}, /* EXTI10 */

    {.id        = 11,
     .exti_line = EXTI_Line11,
     .irq       = EXTI15_10_IRQn,
     .irq_prio  = 6}, /* EXTI11 */

    {.id        = 12,
     .exti_line = EXTI_Line12,
     .irq       = EXTI15_10_IRQn,
     .irq_prio  = 6}, /* EXTI12 */

    {.id        = 13,
     .exti_line = EXTI_Line13,
     .irq       = EXTI15_10_IRQn,
     .irq_prio  = 6}, /* EXTI13 */

    {.id        = 14,
     .exti_line = EXTI_Line14,
     .irq       = EXTI15_10_IRQn,
     .irq_prio  = 6}, /* EXTI14 */

    {.id        = 15,
     .exti_line = EXTI_Line15,
     .irq       = EXTI15_10_IRQn,
     .irq_prio  = 6} /* EXTI15 */};

static xhal_err_t _init(xhal_exit_t *self)
{
    xassert_name(_check_pin_name_valid(self->data.name), self->data.name);

    xhal_err_t ret             = XHAL_OK;
    const exit_hw_info_t *info = _find_exit_info(self->data.name);
    exit_p[info->id]           = self;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    uint8_t port_source = _get_gpio_port_source(self->data.name);
    uint8_t pin_source  = _get_gpio_pin_source(self->data.name);
    GPIO_EXTILineConfig(port_source, pin_source);

    ret = _config(self, &self->data.config);
    if (self->data.config.mode == XEXIT_MODE_INTERRUPT)
    {
        _exit_irq_msp_init(self);
    }

    return ret;
}

static xhal_err_t _config(xhal_exit_t *self, const xhal_exit_config_t *config)
{
    const exit_hw_info_t *info = _find_exit_info(self->data.name);

    EXTI_InitTypeDef EXTI_InitStructure;
    EXTI_InitStructure.EXTI_Line = info->exti_line;
    EXTI_InitStructure.EXTI_Mode = (config->mode == XEXIT_MODE_INTERRUPT)
                                       ? EXTI_Mode_Interrupt
                                       : EXTI_Mode_Event;
    switch (config->trigger)
    {
    case XEXIT_TRIGGER_RISING:
        EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
        break;
    case XEXIT_TRIGGER_FALLING:
        EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
        break;
    case XEXIT_TRIGGER_BOTH:
        EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
        break;
    default:
        break;
    }
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    return XHAL_OK;
}

static xhal_err_t _enable_irq(xhal_exit_t *self)
{
    const exit_hw_info_t *info = _find_exit_info(self->data.name);

    EXTI_ClearITPendingBit(info->exti_line);

    _config(self, &self->data.config);

    return XHAL_OK;
}

static xhal_err_t _disable_irq(xhal_exit_t *self)
{
    const exit_hw_info_t *info = _find_exit_info(self->data.name);

    EXTI_InitTypeDef EXTI_InitStructure;
    EXTI_InitStructure.EXTI_Line    = info->exti_line;
    EXTI_InitStructure.EXTI_LineCmd = DISABLE;
    EXTI_Init(&EXTI_InitStructure);

    EXTI_ClearITPendingBit(info->exti_line);

    return XHAL_OK;
}

static xhal_err_t _set_irq_callback(xhal_exit_t *self, xhal_exit_cb_t cb)
{
    self->data.irq_callback = cb;
    return XHAL_OK;
}

static void _exit_irq_msp_init(xhal_exit_t *self)
{
    const exit_hw_info_t *info = _find_exit_info(self->data.name);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel                   = info->irq;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = info->irq_prio;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

static const exit_hw_info_t *_find_exit_info(const char *name)
{
    char *str_num   = (char *)&name[2];
    uint8_t exit_num = atoi(str_num);

    return &exit_table[exit_num];
}

static uint8_t _get_gpio_port_source(const char *name)
{
    char port_char = name[1];

    switch (port_char)
    {
    case 'A':
        return GPIO_PortSourceGPIOA;
    case 'B':
        return GPIO_PortSourceGPIOB;
    case 'C':
        return GPIO_PortSourceGPIOC;
    case 'D':
        return GPIO_PortSourceGPIOD;
    case 'E':
        return GPIO_PortSourceGPIOE;
    case 'F':
        return GPIO_PortSourceGPIOF;
    case 'G':
        return GPIO_PortSourceGPIOG;
    default:
        return GPIO_PortSourceGPIOA;
    }
}

static uint8_t _get_gpio_pin_source(const char *name)
{
    char *str_num   = (char *)&name[2];
    uint8_t pin_num = atoi(str_num);

    switch (pin_num)
    {
    case 0:
        return GPIO_PinSource0;
    case 1:
        return GPIO_PinSource1;
    case 2:
        return GPIO_PinSource2;
    case 3:
        return GPIO_PinSource3;
    case 4:
        return GPIO_PinSource4;
    case 5:
        return GPIO_PinSource5;
    case 6:
        return GPIO_PinSource6;
    case 7:
        return GPIO_PinSource7;
    case 8:
        return GPIO_PinSource8;
    case 9:
        return GPIO_PinSource9;
    case 10:
        return GPIO_PinSource10;
    case 11:
        return GPIO_PinSource11;
    case 12:
        return GPIO_PinSource12;
    case 13:
        return GPIO_PinSource13;
    case 14:
        return GPIO_PinSource14;
    case 15:
        return GPIO_PinSource15;
    default:
        return GPIO_PinSource0;
    }
}

void EXTI0_IRQHandler(void)
{
    const exit_hw_info_t *info = &exit_table[0];
    xhal_exit_t *exit          = exit_p[info->id];

    if (exit == NULL)
        return;

    if (EXTI_GetITStatus(info->exti_line) != RESET)
    {
        EXTI_ClearITPendingBit(info->exti_line);
        if (exit->data.irq_callback)
        {
            exit->data.irq_callback();
        }
    }
}

void EXTI1_IRQHandler(void)
{
    const exit_hw_info_t *info = &exit_table[1];
    xhal_exit_t *exit          = exit_p[info->id];

    if (exit == NULL)
        return;

    if (EXTI_GetITStatus(info->exti_line) != RESET)
    {
        EXTI_ClearITPendingBit(info->exti_line);
        if (exit->data.irq_callback)
        {
            exit->data.irq_callback();
        }
    }
}

void EXTI2_IRQHandler(void)
{
    const exit_hw_info_t *info = &exit_table[2];
    xhal_exit_t *exit          = exit_p[info->id];

    if (exit == NULL)
        return;

    if (EXTI_GetITStatus(info->exti_line) != RESET)
    {
        EXTI_ClearITPendingBit(info->exti_line);
        if (exit->data.irq_callback)
        {
            exit->data.irq_callback();
        }
    }
}

void EXTI3_IRQHandler(void)
{
    const exit_hw_info_t *info = &exit_table[3];
    xhal_exit_t *exit          = exit_p[info->id];

    if (exit == NULL)
        return;

    if (EXTI_GetITStatus(info->exti_line) != RESET)
    {
        EXTI_ClearITPendingBit(info->exti_line);
        if (exit->data.irq_callback)
        {
            exit->data.irq_callback();
        }
    }
}

void EXTI4_IRQHandler(void)
{
    const exit_hw_info_t *info = &exit_table[4];
    xhal_exit_t *exit          = exit_p[info->id];

    if (exit == NULL)
        return;

    if (EXTI_GetITStatus(info->exti_line) != RESET)
    {
        EXTI_ClearITPendingBit(info->exti_line);
        if (exit->data.irq_callback)
        {
            exit->data.irq_callback();
        }
    }
}

void EXTI9_5_IRQHandler(void)
{
    for (uint8_t i = 5; i <= 9; i++)
    {
        const exit_hw_info_t *info = &exit_table[i];
        xhal_exit_t *exit          = exit_p[info->id];

        if (exit == NULL)
            continue;

        if (EXTI_GetITStatus(info->exti_line) != RESET)
        {
            EXTI_ClearITPendingBit(info->exti_line);
            if (exit->data.irq_callback)
            {
                exit->data.irq_callback();
            }
        }
    }
}

void EXTI15_10_IRQHandler(void)
{
    for (uint8_t i = 10; i <= 15; i++)
    {
        const exit_hw_info_t *info = &exit_table[i];
        xhal_exit_t *exit          = exit_p[info->id];

        if (exit == NULL)
            continue;

        if (EXTI_GetITStatus(info->exti_line) != RESET)
        {
            EXTI_ClearITPendingBit(info->exti_line);
            if (exit->data.irq_callback)
            {
                exit->data.irq_callback();
            }
        }
    }
}
