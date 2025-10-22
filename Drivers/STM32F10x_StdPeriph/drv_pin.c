#include "xhal_assert.h"
#include "xhal_config.h"
#include "xhal_def.h"
#include "xhal_export.h"
#include "xhal_log.h"
#include "xhal_pin.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include XHAL_CMSIS_DEVICE_HEADER

XLOG_TAG("xDriverPin");

static xhal_err_t _init(xhal_pin_t *self, xhal_pin_state_t status);
static xhal_err_t _set_mode(xhal_pin_t *self, xhal_pin_mode_t mode);
static xhal_err_t _read(xhal_pin_t *self, xhal_pin_state_t *status);
static xhal_err_t _write(xhal_pin_t *self, xhal_pin_state_t status);

static void _gpio_clock_enable(const char *name);
static bool _check_pin_name_valid(const char *name);
static GPIO_TypeDef *_get_port_from_name(const char *name);
static uint16_t _get_pin_from_name(const char *name);

const xhal_pin_ops_t pin_ops_driver = {
    .init     = _init,
    .set_mode = _set_mode,
    .read     = _read,
    .write    = _write,
};

static xhal_err_t _init(xhal_pin_t *self, xhal_pin_state_t status)
{
    xassert_name(_check_pin_name_valid(self->data.name), self->data.name);

    xhal_err_t ret = XHAL_OK;

    _gpio_clock_enable(self->data.name);

    ret = _set_mode(self, self->data.mode);

    if (self->data.mode == XPIN_MODE_OUTPUT_PP ||
        self->data.mode == XPIN_MODE_OUTPUT_OD)
    {
        ret = _write(self, status);
    }

    return ret;
}

static xhal_err_t _set_mode(xhal_pin_t *self, xhal_pin_mode_t mode)
{
    GPIO_TypeDef *port = _get_port_from_name(self->data.name);
    uint16_t pin       = _get_pin_from_name(self->data.name);
    GPIO_InitTypeDef GPIO_InitStruct;

    GPIO_StructInit(&GPIO_InitStruct);
    switch (mode)
    {
    case XPIN_MODE_INPUT:
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        break;
    case XPIN_MODE_INPUT_PULLUP:
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
        break;
    case XPIN_MODE_INPUT_PULLDOWN:
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPD;
        break;
    case XPIN_MODE_OUTPUT_PP:
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
        break;
    case XPIN_MODE_OUTPUT_OD:
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_OD;
        break;
    default:
        break;
    }
    GPIO_InitStruct.GPIO_Pin   = pin;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(port, &GPIO_InitStruct);

    return XHAL_OK;
}

static xhal_err_t _read(xhal_pin_t *self, xhal_pin_state_t *status)
{
    GPIO_TypeDef *port = _get_port_from_name(self->data.name);
    uint16_t pin       = _get_pin_from_name(self->data.name);

    uint8_t gpio_status = GPIO_ReadInputDataBit(port, pin);

    *status = (gpio_status == Bit_SET) ? XPIN_HIGH : XPIN_LOW;

    return XHAL_OK;
}

static xhal_err_t _write(xhal_pin_t *self, xhal_pin_state_t status)
{
    GPIO_TypeDef *port = _get_port_from_name(self->data.name);
    uint16_t pin       = _get_pin_from_name(self->data.name);

    GPIO_WriteBit(port, pin, (status == XPIN_HIGH) ? Bit_SET : Bit_RESET);

    return XHAL_OK;
}

static void _gpio_clock_enable(const char *name)
{
    GPIO_TypeDef *port = _get_port_from_name(name);
    switch ((uintptr_t)port)
    {
    case (uintptr_t)GPIOA:
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
        break;
    case (uintptr_t)GPIOB:
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
        break;
    case (uintptr_t)GPIOC:
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
        break;
    case (uintptr_t)GPIOD:
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
        break;
    case (uintptr_t)GPIOE:
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);
        break;
    case (uintptr_t)GPIOF:
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOF, ENABLE);
        break;
    case (uintptr_t)GPIOG:
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOG, ENABLE);
        break;
    default:
        break;
    }
}

static bool _check_pin_name_valid(const char *name)
{
    uint8_t len = strlen(name);
    if (len < 3 || len > 4)
    {
        return false;
    }

    if (toupper(name[0]) != 'P')
    {
        return false;
    }
    if (toupper(name[1]) < 'A' || toupper(name[1]) > 'G')
    {
        return false;
    }

    for (uint8_t i = 2; i < len; i++)
    {
        if (!(name[i] >= '0' && name[i] <= '9'))
        {
            return false;
        }
    }

    int8_t pin_num = atoi(&name[2]);
    if (pin_num < 0 || pin_num >= 16)
    {
        return false;
    }

    return true;
}

static GPIO_TypeDef *_get_port_from_name(const char *name)
{
    static const GPIO_TypeDef *port_table[] = {
        GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG,
    };

    return (GPIO_TypeDef *)port_table[name[1] - 'A'];
}

static uint16_t _get_pin_from_name(const char *name)
{
    char *str_num   = (char *)&name[2];
    uint8_t pin_num = atoi(str_num);

    return (uint16_t)(1 << pin_num);
}
