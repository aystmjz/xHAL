#include "UART.h"
#include "xhal_assert.h"
#include "xhal_log.h"
#include "xhal_spi.h"
#include "xhal_time.h"
#include <ctype.h>
#include <string.h>
#include XHAL_CMSIS_DEVICE_HEADER

XLOG_TAG("xDriverSPIsoft");

#define XSPI_DUMMY_BYTE (0xFFFFU)

#define SCK_H()         GPIO_SetBits(sck_port, sck_pin)
#define SCK_L()         GPIO_ResetBits(sck_port, sck_pin)
#define MOSI_H()        GPIO_SetBits(mosi_port, mosi_pin)
#define MOSI_L()        GPIO_ResetBits(mosi_port, mosi_pin)
#define MISO_READ()     GPIO_ReadInputDataBit(miso_port, miso_pin)

#define SPI_DELAY()                            \
    for (volatile uint16_t d = 0; d < 30; d++) \
    __NOP()

static xhal_err_t _init(xhal_spi_t *self);
static xhal_err_t _config(xhal_spi_t *self, const xhal_spi_config_t *config);
static xhal_err_t _transfer(xhal_spi_t *self, xhal_spi_msg_t *msg);

const xhal_spi_ops_t spi_soft_ops_driver = {
    .init     = _init,
    .config   = _config,
    .transfer = _transfer,
};

typedef struct spi_hw_info spi_hw_info_t;

static void _gpio_clock_enable(const char *name);
static bool _check_pin_name_valid(const char *name);
static GPIO_TypeDef *_get_port_from_name(const char *name);
static uint16_t _get_pin_from_name(const char *name);

static xhal_err_t _init(xhal_spi_t *self)
{
    xassert_not_null(self->data.sck_name);
    xassert_not_null(self->data.miso_name);
    xassert_not_null(self->data.mosi_name);

    xassert_name(_check_pin_name_valid(self->data.sck_name),
                 self->data.sck_name);
    xassert_name(_check_pin_name_valid(self->data.miso_name),
                 self->data.miso_name);
    xassert_name(_check_pin_name_valid(self->data.mosi_name),
                 self->data.mosi_name);

    _gpio_clock_enable(self->data.sck_name);
    _gpio_clock_enable(self->data.miso_name);
    _gpio_clock_enable(self->data.mosi_name);

    GPIO_TypeDef *port = _get_port_from_name(self->data.sck_name);
    uint16_t pin       = _get_pin_from_name(self->data.sck_name);

    switch (self->data.config.mode)
    {
    case XSPI_MODE_0:
    case XSPI_MODE_1:
        GPIO_WriteBit(port, pin, Bit_RESET);
        break;
    case XSPI_MODE_2:
    case XSPI_MODE_3:
        GPIO_WriteBit(port, pin, Bit_SET);
        break;
    default:
        break;
    }
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin   = pin;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(port, &GPIO_InitStructure);

    port                        = _get_port_from_name(self->data.mosi_name);
    pin                         = _get_pin_from_name(self->data.mosi_name);
    GPIO_InitStructure.GPIO_Pin = pin;
    GPIO_Init(port, &GPIO_InitStructure);
    GPIO_WriteBit(port, pin, Bit_RESET);

    port                         = _get_port_from_name(self->data.miso_name);
    pin                          = _get_pin_from_name(self->data.miso_name);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Pin  = pin;
    GPIO_Init(port, &GPIO_InitStructure);

    return XHAL_OK;
}

static xhal_err_t _config(xhal_spi_t *self, const xhal_spi_config_t *config)
{
    return XHAL_OK;
}

static xhal_err_t _transfer(xhal_spi_t *self, xhal_spi_msg_t *msg)
{
    GPIO_TypeDef *sck_port  = _get_port_from_name(self->data.sck_name);
    uint16_t sck_pin        = _get_pin_from_name(self->data.sck_name);
    GPIO_TypeDef *mosi_port = _get_port_from_name(self->data.mosi_name);
    uint16_t mosi_pin       = _get_pin_from_name(self->data.mosi_name);
    GPIO_TypeDef *miso_port = _get_port_from_name(self->data.miso_name);
    uint16_t miso_pin       = _get_pin_from_name(self->data.miso_name);

    const uint8_t mode = self->data.config.mode;
    const uint8_t bits = self->data.config.data_bits;
    const uint8_t dir  = self->data.config.direction;

    const uint8_t cpol = (mode >> 1) & 0x01;
    const uint8_t cpha = mode & 0x01;

    /* 设置 SCK 空闲电平 */
    if (cpol)
        SCK_H();
    else
        SCK_L();

    for (uint32_t i = 0; i < msg->len; i++)
    {
        uint16_t send_data = XSPI_DUMMY_BYTE;
        uint16_t recv_data = 0;

        if (msg->tx_buf != NULL)
        {
            send_data = (bits == XSPI_DATA_BITS_8)
                            ? ((const uint8_t *)msg->tx_buf)[i]
                            : ((const uint16_t *)msg->tx_buf)[i];
        }

        for (int8_t b = (bits == XSPI_DATA_BITS_8 ? 7 : 15); b >= 0; b--)
        {
            uint8_t bit = (send_data >> b) & 0x01;

            if (!cpha) /* 模式0、2：先输出数据，再采样 */
            {
                if (dir != XSPI_DIR_1LINE_RX)
                {
                    if (bit)
                        MOSI_H();
                    else
                        MOSI_L();
                }

                SPI_DELAY();

                /* 产生时钟沿（上升或下降取决于 CPOL） */
                if (cpol == 0)
                    SCK_H();
                else
                    SCK_L();

                SPI_DELAY();

                if (dir != XSPI_DIR_1LINE_TX)
                    recv_data = (recv_data << 1) | MISO_READ();

                /* 恢复空闲电平 */
                if (cpol == 0)
                    SCK_L();
                else
                    SCK_H();
            }
            else /* 模式1、3：先产生时钟沿，再输出数据 */
            {
                if (cpol == 0)
                    SCK_H();
                else
                    SCK_L();

                SPI_DELAY();

                if (dir != XSPI_DIR_1LINE_RX)
                {
                    if (bit)
                        MOSI_H();
                    else
                        MOSI_L();
                }

                SPI_DELAY();

                if (cpol == 0)
                    SCK_L();
                else
                    SCK_H();

                if (dir != XSPI_DIR_1LINE_TX)
                    recv_data = (recv_data << 1) | MISO_READ();
            }
        }

        /* 保存接收数据 */
        if ((dir != XSPI_DIR_1LINE_TX) && msg->rx_buf != NULL)
        {
            if (bits == XSPI_DATA_BITS_8)
                ((uint8_t *)msg->rx_buf)[i] = (uint8_t)recv_data;
            else
                ((uint16_t *)msg->rx_buf)[i] = (uint16_t)recv_data;
        }
    }

#ifdef XHAL_OS_SUPPORTING
    osEventFlagsSet(self->data.event_flag, XSPI_EVENT_DONE);
#endif

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