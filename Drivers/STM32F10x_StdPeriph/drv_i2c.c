#include "xhal_assert.h"
#include "xhal_i2c.h"
#include "xhal_log.h"
#include "xhal_time.h"
#include <ctype.h>
#include <string.h>
#include XHAL_CMSIS_DEVICE_HEADER

// #define I2C_DEBUG

XLOG_TAG("xDriverI2C");

typedef struct i2c_hw
{
    GPIO_TypeDef *sda_port;
    uint16_t sda_pin;
    GPIO_TypeDef *scl_port;
    uint16_t scl_pin;
    uint32_t clock;
} i2c_hw_t;

static xhal_err_t _init(xhal_i2c_t *self);
static xhal_err_t _config(xhal_i2c_t *self, const xhal_i2c_config_t *config);
static xhal_err_t _transfer(xhal_i2c_t *self, xhal_i2c_msg_t *msg);

static void _i2c_delay(i2c_hw_t *hw);
static inline void _sda_set(i2c_hw_t *hw, uint8_t val);
static inline void _scl_set(i2c_hw_t *hw, uint8_t val);
static inline uint8_t _sda_read(i2c_hw_t *hw);
static void _i2c_start(i2c_hw_t *hw);
static void _i2c_stop(i2c_hw_t *hw);
static uint8_t _i2c_write_byte(i2c_hw_t *hw, uint8_t data);
static uint8_t _i2c_read_byte(i2c_hw_t *hw, uint8_t ack);

static void _gpio_clock_enable(const char *name);
static bool _check_pin_name_valid(const char *name);
static GPIO_TypeDef *_get_port_from_name(const char *name);
static uint16_t _get_pin_from_name(const char *name);

const xhal_i2c_ops_t i2c_ops_driver = {
    .init     = _init,
    .config   = _config,
    .transfer = _transfer,
};

static xhal_err_t _init(xhal_i2c_t *self)
{
    xassert_not_null(self->data.sda_name);
    xassert_not_null(self->data.scl_name);

    xassert_name(_check_pin_name_valid(self->data.sda_name),
                 self->data.sda_name);
    xassert_name(_check_pin_name_valid(self->data.scl_name),
                 self->data.scl_name);

    _gpio_clock_enable(self->data.sda_name);
    _gpio_clock_enable(self->data.scl_name);

    GPIO_TypeDef *port = _get_port_from_name(self->data.sda_name);
    uint16_t pin       = _get_pin_from_name(self->data.sda_name);

    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_Out_OD;
    GPIO_InitStruct.GPIO_Pin   = pin;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(port, &GPIO_InitStruct);
    GPIO_WriteBit(port, pin, Bit_SET);

    port                     = _get_port_from_name(self->data.scl_name);
    pin                      = _get_pin_from_name(self->data.scl_name);
    GPIO_InitStruct.GPIO_Pin = pin;
    GPIO_Init(port, &GPIO_InitStruct);
    GPIO_WriteBit(port, pin, Bit_SET);

    return XHAL_OK;
}

static xhal_err_t _config(xhal_i2c_t *self, const xhal_i2c_config_t *config)
{
    return XHAL_OK;
}

static xhal_err_t _transfer(xhal_i2c_t *self, xhal_i2c_msg_t *msg)
{
    xhal_err_t ret = XHAL_OK;
    i2c_hw_t hw    = {
           .sda_port = _get_port_from_name(self->data.sda_name),
           .sda_pin  = _get_pin_from_name(self->data.sda_name),
           .scl_port = _get_port_from_name(self->data.scl_name),
           .scl_pin  = _get_pin_from_name(self->data.scl_name),
           .clock    = self->data.config.clock,
    };

    if (msg->flags & XI2C_NOSTART)
    {
        goto data_phase;
    }

    _i2c_start(&hw);

#ifdef I2C_DEBUG
    xlog_printf("%s", (msg->flags & XI2C_RD) ? "(Rd) " : "(Wr) ");
#endif
    if (msg->flags & XI2C_TEN)
    {
        /* 10 位地址 */
        uint8_t addr_high = 0xF0 | (uint8_t)(((msg->addr >> 8) & 0x03) << 1) |
                            ((msg->flags & XI2C_RD) ? 1 : 0);
        uint8_t addr_low = (uint8_t)(msg->addr & 0xFF);
        if (!_i2c_write_byte(&hw, addr_high))
        {
            if (!(msg->flags & XI2C_IGNORE_NAK))
                ret = XHAL_ERR_IO;
            goto out_stop;
        }
        if (!_i2c_write_byte(&hw, addr_low))
        {
            if (!(msg->flags & XI2C_IGNORE_NAK))
                ret = XHAL_ERR_IO;
            goto out_stop;
        }
    }
    else
    {
        uint8_t addr_byte =
            ((msg->addr & 0x7F) << 1) | ((msg->flags & XI2C_RD) ? 1 : 0);
        if (!_i2c_write_byte(&hw, addr_byte))
        {
            if (!(msg->flags & XI2C_IGNORE_NAK))
                ret = XHAL_ERR_IO;
            goto out_stop;
        }
    }

data_phase:
    if (msg->flags & XI2C_RD)
    {
        uint16_t read_len = msg->len;
        uint8_t *buf      = msg->buf;
        uint16_t offset   = 0;

        if (msg->flags & XI2C_RECV_LEN)
        {
            uint8_t len_byte = _i2c_read_byte(&hw, 1);
            buf[0]           = len_byte;
            offset           = 1;
            read_len         = (len_byte < (read_len - offset)) ? len_byte
                                                                : (read_len - offset);
        }

        for (uint16_t i = 0; i < read_len; i++)
        {
            uint8_t ack     = (i == (read_len - 1)) ? 0 : 1;
            buf[i + offset] = _i2c_read_byte(&hw, ack);
        }
    }
    else
    {
        for (uint16_t i = 0; i < msg->len; i++)
        {
            if (!_i2c_write_byte(&hw, msg->buf[i]))
            {
                if (!(msg->flags & XI2C_IGNORE_NAK))
                {
                    ret = XHAL_ERR_IO;
                    goto out_stop;
                }
            }
        }
    }

#ifdef XHAL_OS_SUPPORTING
    osEventFlagsSet(self->data.event_flag, XI2C_EVENT_DONE);
#endif
    if (msg->flags & XI2C_STOP)
        _i2c_stop(&hw);
    return ret;

out_stop:
    _i2c_stop(&hw);
    return ret;
}

static void _i2c_delay(i2c_hw_t *hw)
{
    uint32_t half_us = 500000 / hw->clock;
    xtime_delay_us(half_us);
}

static inline void _sda_set(i2c_hw_t *hw, uint8_t val)
{
    if (val)
        GPIO_WriteBit(hw->sda_port, hw->sda_pin, Bit_SET);
    else
        GPIO_WriteBit(hw->sda_port, hw->sda_pin, Bit_RESET);
}

static inline void _scl_set(i2c_hw_t *hw, uint8_t val)
{
    if (val)
        GPIO_WriteBit(hw->scl_port, hw->scl_pin, Bit_SET);
    else
        GPIO_WriteBit(hw->scl_port, hw->scl_pin, Bit_RESET);
}

static inline uint8_t _sda_read(i2c_hw_t *hw)
{
    return GPIO_ReadInputDataBit(hw->sda_port, hw->sda_pin) ? 1 : 0;
}

static void _i2c_start(i2c_hw_t *hw)
{
#ifdef I2C_DEBUG
    xlog_printf("S "); // Start condition
#endif
    _sda_set(hw, 1);
    _scl_set(hw, 1);
    _i2c_delay(hw);
    _sda_set(hw, 0);
    _i2c_delay(hw);
    _scl_set(hw, 0);
}

static void _i2c_stop(i2c_hw_t *hw)
{
#ifdef I2C_DEBUG
    xlog_printf("P\r\n");
#endif
    _sda_set(hw, 0);
    _scl_set(hw, 1);
    _i2c_delay(hw);
    _sda_set(hw, 1);
    _i2c_delay(hw);
}

static uint8_t _i2c_write_byte(i2c_hw_t *hw, uint8_t data)
{
#ifdef I2C_DEBUG
    xlog_printf("%02X ", data);
#endif

    for (int i = 0; i < 8; i++)
    {
        _sda_set(hw, (data & 0x80) != 0);
        _i2c_delay(hw);
        _scl_set(hw, 1);
        _i2c_delay(hw);
        _scl_set(hw, 0);
        data <<= 1;
    }

    _sda_set(hw, 1);
    _i2c_delay(hw);
    _scl_set(hw, 1);
    _i2c_delay(hw);
    uint8_t ack = !_sda_read(hw);
    _scl_set(hw, 0);

#ifdef I2C_DEBUG
    xlog_printf("[%c] ", ack ? 'A' : 'N');
#endif

    return ack;
}

static uint8_t _i2c_read_byte(i2c_hw_t *hw, uint8_t ack)
{
    uint8_t data = 0;
    _sda_set(hw, 1);
    for (int i = 0; i < 8; i++)
    {
        data <<= 1;
        _scl_set(hw, 1);
        _i2c_delay(hw);
        if (_sda_read(hw))
            data |= 1;
        _scl_set(hw, 0);
        _i2c_delay(hw);
    }

    _sda_set(hw, !ack);
    _i2c_delay(hw);
    _scl_set(hw, 1);
    _i2c_delay(hw);
    _scl_set(hw, 0);
    _sda_set(hw, 1);

#ifdef I2C_DEBUG
    xlog_printf("[%02X] %c ", data, ack ? 'A' : 'N');
#endif

    return data;
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