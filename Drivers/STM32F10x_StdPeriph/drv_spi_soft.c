#include "drv_util.h"
#include "xhal_spi.h"
#include <ctype.h>
#include <string.h>

XLOG_TAG("xDriverSPIsoft");

#define XSPI_DUMMY_BYTE      (0xFFFFU)

#define SCK_H(port, pin)     GPIO_SetBits(port, pin)
#define SCK_L(port, pin)     GPIO_ResetBits(port, pin)
#define MOSI_H(port, pin)    GPIO_SetBits(port, pin)
#define MOSI_L(port, pin)    GPIO_ResetBits(port, pin)
#define MISO_READ(port, pin) GPIO_ReadInputDataBit(port, pin)

#define SPI_DELAY()                            \
    for (volatile uint16_t d = 0; d < 1; d++) \
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

static xhal_err_t _init(xhal_spi_t *self)
{
    xassert_not_null(self->data.sck_name);
    xassert_not_null(self->data.mosi_name);

    xassert_name(_check_pin_name_valid(self->data.sck_name),
                 self->data.sck_name);
    xassert_name(_check_pin_name_valid(self->data.mosi_name),
                 self->data.mosi_name);

    _gpio_clock_enable(self->data.sck_name);
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
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin   = pin;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(port, &GPIO_InitStructure);

    switch (self->data.config.direction)
    {
    case XSPI_DIR_2LINE_FULL_DUPLEX:
    case XSPI_DIR_2LINE_RX_ONLY:
        xassert_not_null(self->data.miso_name);
        xassert_name(_check_pin_name_valid(self->data.miso_name),
                     self->data.miso_name);
        _gpio_clock_enable(self->data.miso_name);
        port = _get_port_from_name(self->data.miso_name);
        pin  = _get_pin_from_name(self->data.miso_name);
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
        GPIO_InitStructure.GPIO_Pin  = pin;
        GPIO_Init(port, &GPIO_InitStructure);
    case XSPI_DIR_1LINE_TX:
        port = _get_port_from_name(self->data.mosi_name);
        pin  = _get_pin_from_name(self->data.mosi_name);
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_InitStructure.GPIO_Pin  = pin;
        GPIO_Init(port, &GPIO_InitStructure);
        GPIO_WriteBit(port, pin, Bit_RESET);
        break;
    case XSPI_DIR_1LINE_RX:
        port = _get_port_from_name(self->data.mosi_name);
        pin  = _get_pin_from_name(self->data.mosi_name);
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
        GPIO_InitStructure.GPIO_Pin  = pin;
        GPIO_Init(port, &GPIO_InitStructure);
        break;
    default:
        break;
    }

    return XHAL_OK;
}

static xhal_err_t _config(xhal_spi_t *self, const xhal_spi_config_t *config)
{
    GPIO_TypeDef *port = _get_port_from_name(self->data.mosi_name);
    uint16_t pin       = _get_pin_from_name(self->data.mosi_name);

    GPIO_InitTypeDef GPIO_InitStructure;
    switch (self->data.config.direction)
    {
    case XSPI_DIR_1LINE_TX:
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_InitStructure.GPIO_Pin  = pin;
        GPIO_Init(port, &GPIO_InitStructure);
        GPIO_WriteBit(port, pin, Bit_RESET);
        break;
    case XSPI_DIR_1LINE_RX:
        port = _get_port_from_name(self->data.mosi_name);
        pin  = _get_pin_from_name(self->data.mosi_name);
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
        GPIO_InitStructure.GPIO_Pin  = pin;
        GPIO_Init(port, &GPIO_InitStructure);
        break;
    default:
        break;
    }

    return XHAL_OK;
}

static xhal_err_t _transfer(xhal_spi_t *self, xhal_spi_msg_t *msg)
{

    GPIO_TypeDef *sck_port  = _get_port_from_name(self->data.sck_name);
    uint16_t sck_pin        = _get_pin_from_name(self->data.sck_name);
    GPIO_TypeDef *mosi_port = _get_port_from_name(self->data.mosi_name);
    uint16_t mosi_pin       = _get_pin_from_name(self->data.mosi_name);

    const uint8_t mode = self->data.config.mode;
    const uint8_t bits = self->data.config.data_bits;
    const uint8_t dir  = self->data.config.direction;

    const uint8_t cpol = (mode >> 1) & 0x01;
    const uint8_t cpha = mode & 0x01;

    GPIO_TypeDef *miso_port = NULL;
    uint16_t miso_pin       = 0;
    if (dir != XSPI_DIR_1LINE_RX && dir != XSPI_DIR_1LINE_TX)
    {
        miso_port = _get_port_from_name(self->data.miso_name);
        miso_pin  = _get_pin_from_name(self->data.miso_name);
    }

    /* 设置 SCK 空闲电平 */
    if (cpol)
        SCK_H(sck_port, sck_pin);
    else
        SCK_L(sck_port, sck_pin);

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
                if (dir != XSPI_DIR_1LINE_RX && dir != XSPI_DIR_2LINE_RX_ONLY)
                {
                    if (bit)
                        MOSI_H(mosi_port, mosi_pin);
                    else
                        MOSI_L(mosi_port, mosi_pin);
                }

                SPI_DELAY();

                /* 产生时钟沿（上升或下降取决于 CPOL） */
                if (cpol == 0)
                    SCK_H(sck_port, sck_pin);
                else
                    SCK_L(sck_port, sck_pin);

                SPI_DELAY();

                if (dir != XSPI_DIR_1LINE_TX)
                {
                    if (dir == XSPI_DIR_1LINE_RX)
                        recv_data =
                            (recv_data << 1) | MISO_READ(mosi_port, mosi_pin);
                    else
                        recv_data =
                            (recv_data << 1) | MISO_READ(miso_port, miso_pin);
                }

                /* 恢复空闲电平 */
                if (cpol == 0)
                    SCK_L(sck_port, sck_pin);
                else
                    SCK_H(sck_port, sck_pin);
            }
            else /* 模式1、3：先产生时钟沿，再输出数据 */
            {
                if (cpol == 0)
                    SCK_H(sck_port, sck_pin);
                else
                    SCK_L(sck_port, sck_pin);

                SPI_DELAY();

                if (dir != XSPI_DIR_1LINE_RX && dir != XSPI_DIR_2LINE_RX_ONLY)
                {
                    if (bit)
                        MOSI_H(mosi_port, mosi_pin);
                    else
                        MOSI_L(mosi_port, mosi_pin);
                }

                SPI_DELAY();

                if (cpol == 0)
                    SCK_L(sck_port, sck_pin);
                else
                    SCK_H(sck_port, sck_pin);

                if (dir != XSPI_DIR_1LINE_TX)
                {
                    if (dir == XSPI_DIR_1LINE_RX)
                        recv_data =
                            (recv_data << 1) | MISO_READ(mosi_port, mosi_pin);
                    else
                        recv_data =
                            (recv_data << 1) | MISO_READ(miso_port, miso_pin);
                }
            } /* end of if (!cpha) */
        } /* end of for (int8_t b=(bits==XSPI_DATA_BITS_8?7:15); b>=0;b--) */

        /* 保存接收数据 */
        if (dir != XSPI_DIR_1LINE_TX && msg->rx_buf != NULL)
        {
            if (bits == XSPI_DATA_BITS_8)
                ((uint8_t *)msg->rx_buf)[i] = (uint8_t)recv_data;
            else
                ((uint16_t *)msg->rx_buf)[i] = (uint16_t)recv_data;
        }
    } /* end of for (uint32_t i = 0; i < msg->len; i++) */

#ifdef XHAL_OS_SUPPORTING
    osEventFlagsSet(self->data.event_flag, XSPI_EVENT_DONE);
#endif

    return XHAL_OK;
}
