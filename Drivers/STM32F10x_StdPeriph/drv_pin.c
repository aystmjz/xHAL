#include "drv_util.h"
#include "xhal_pin.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

XLOG_TAG("xDriverPin");

static xhal_err_t _init(xhal_pin_t *self, xhal_pin_state_t status);
static xhal_err_t _set_mode(xhal_pin_t *self, xhal_pin_mode_t mode);
static xhal_err_t _read(xhal_pin_t *self, xhal_pin_state_t *status);
static xhal_err_t _write(xhal_pin_t *self, xhal_pin_state_t status);

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
