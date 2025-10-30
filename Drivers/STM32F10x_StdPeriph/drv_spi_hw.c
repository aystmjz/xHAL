#include "UART.h"
#include "xhal_assert.h"
#include "xhal_log.h"
#include "xhal_spi.h"
#include "xhal_time.h"
#include <ctype.h>
#include <string.h>
#include XHAL_CMSIS_DEVICE_HEADER

XLOG_TAG("xDriverSPIhw");

#define XSPI_DUMMY_BYTE (0xFFFFU)

typedef struct
{
    const void *tx_buf;
    void *rx_buf;
    uint32_t len;
    uint32_t rx_index;
    uint32_t tx_index;
} spi_xfer_ctx_t;

static volatile spi_xfer_ctx_t spi_ctx[2] = {0};
static xhal_spi_t *spi_p[2]               = {NULL};

static xhal_err_t _init(xhal_spi_t *self);
static xhal_err_t _config(xhal_spi_t *self, const xhal_spi_config_t *config);
static xhal_err_t _transfer(xhal_spi_t *self, xhal_spi_msg_t *msg);

const xhal_spi_ops_t spi_hw_ops_driver = {
    .init     = _init,
    .config   = _config,
    .transfer = _transfer,
};

typedef struct spi_hw_info spi_hw_info_t;

static void _spi_gpio_msp_init(xhal_spi_t *self);
static void _spi_irq_msp_init(xhal_spi_t *self);
static bool _check_spi_name_valid(const char *name);
static const spi_hw_info_t *_find_spi_info(const char *name);

typedef struct spi_hw_info
{
    uint8_t id;       /* 外设编号 */
    SPI_TypeDef *spi; /* SPI 外设基地址 */

    GPIO_TypeDef *sck_port; /* SCK 引脚端口 */
    uint16_t sck_pin;       /* SCK 引脚编号 */

    GPIO_TypeDef *miso_port; /* MISO 引脚端口 */
    uint16_t miso_pin;       /* MISO 引脚编号 */

    GPIO_TypeDef *mosi_port; /* MOSI 引脚端口 */
    uint16_t mosi_pin;       /* MOSI 引脚编号 */

    uint32_t sck_clk;  /* SCK 端口时钟 */
    uint32_t miso_clk; /* MISO 端口时钟 */
    uint32_t mosi_clk; /* MOSI 端口时钟 */

    uint32_t spi_clk; /* SPI 外设时钟 */

    IRQn_Type irq_spi;    /* USART 中断号 */
    uint8_t irq_spi_prio; /* USART 中断优先级 */
} spi_hw_info_t;

static const spi_hw_info_t spi_table[] = {
    {
        .id           = 0,
        .spi          = SPI1,
        .sck_port     = GPIOA,
        .sck_pin      = GPIO_Pin_5,
        .miso_port    = GPIOA,
        .miso_pin     = GPIO_Pin_6,
        .mosi_port    = GPIOA,
        .mosi_pin     = GPIO_Pin_7,
        .sck_clk      = RCC_APB2Periph_GPIOA,
        .miso_clk     = RCC_APB2Periph_GPIOA,
        .mosi_clk     = RCC_APB2Periph_GPIOA,
        .spi_clk      = RCC_APB2Periph_SPI1,
        .irq_spi      = SPI1_IRQn,
        .irq_spi_prio = 6,
    },
    {
        .id           = 1,
        .spi          = SPI2,
        .sck_port     = GPIOB,
        .sck_pin      = GPIO_Pin_13,
        .miso_port    = GPIOB,
        .miso_pin     = GPIO_Pin_14,
        .mosi_port    = GPIOB,
        .mosi_pin     = GPIO_Pin_15,
        .sck_clk      = RCC_APB2Periph_GPIOB,
        .miso_clk     = RCC_APB2Periph_GPIOB,
        .mosi_clk     = RCC_APB2Periph_GPIOB,
        .spi_clk      = RCC_APB1Periph_SPI2,
        .irq_spi      = SPI2_IRQn,
        .irq_spi_prio = 6,
    },
};

static xhal_err_t _init(xhal_spi_t *self)
{
    xassert_name(_check_spi_name_valid(self->data.spi_name),
                 self->data.spi_name);

    xhal_err_t ret = XHAL_OK;

    const spi_hw_info_t *info = _find_spi_info(self->data.spi_name);
    spi_p[info->id]           = self;

    if (info->spi_clk == RCC_APB2Periph_SPI1)
    {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
    }
    else
    {
        RCC_APB1PeriphClockCmd(info->spi_clk, ENABLE);
    }

    _spi_gpio_msp_init(self);
    ret = _config(self, &self->data.config);
    _spi_irq_msp_init(self);

    return ret;
}

static xhal_err_t _config(xhal_spi_t *self, const xhal_spi_config_t *config)
{

    const spi_hw_info_t *info = _find_spi_info(self->data.spi_name);

    SPI_Cmd(info->spi, DISABLE);

    SPI_InitTypeDef SPI_InitStructure;
    SPI_InitStructure.SPI_Mode     = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = (config->data_bits == XSPI_DATA_BITS_8)
                                         ? SPI_DataSize_8b
                                         : SPI_DataSize_16b;
    switch (config->direction)
    {
    case XSPI_DIR_2LINE_FULL_DUPLEX:
        SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
        break;
    case XSPI_DIR_2LINE_RX_ONLY:
        SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_RxOnly;
        break;
    case XSPI_DIR_1LINE_RX:
        SPI_InitStructure.SPI_Direction = SPI_Direction_1Line_Rx;
        break;
    case XSPI_DIR_1LINE_TX:
        SPI_InitStructure.SPI_Direction = SPI_Direction_1Line_Tx;
        break;
    default:
        break;
    }
    switch (config->mode)
    {
    case XSPI_MODE_0: /* CPOL = 0, CPHA = 0 */
        SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
        SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
        break;
    case XSPI_MODE_1: /* CPOL = 0, CPHA = 1 */
        SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
        SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;
        break;
    case XSPI_MODE_2: /* CPOL = 1, CPHA = 0 */
        SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;
        SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
        break;
    case XSPI_MODE_3: /* CPOL = 1, CPHA = 1 */
        SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;
        SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;
        break;
    default:
        break;
    }
    SPI_InitStructure.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_64;
    SPI_InitStructure.SPI_NSS               = SPI_NSS_Soft;
    SPI_Init(info->spi, &SPI_InitStructure);

    if (config->direction != XSPI_DIR_2LINE_RX_ONLY &&
        config->direction != XSPI_DIR_1LINE_RX)
    {
        SPI_Cmd(info->spi, ENABLE);
    }

    return XHAL_OK;
}

static xhal_err_t _transfer(xhal_spi_t *self, xhal_spi_msg_t *msg)
{
    const spi_hw_info_t *info = _find_spi_info(self->data.spi_name);

    uint8_t id           = info->id;
    spi_ctx[id].tx_buf   = msg->tx_buf;
    spi_ctx[id].rx_buf   = msg->rx_buf;
    spi_ctx[id].len      = msg->len;
    spi_ctx[id].rx_index = 0;
    spi_ctx[id].tx_index = 0;

    SPI_I2S_ITConfig(info->spi, SPI_I2S_IT_TXE, ENABLE);
    SPI_Cmd(info->spi, ENABLE);

    return XHAL_OK;
}

static void _spi_gpio_msp_init(xhal_spi_t *self)
{
    const spi_hw_info_t *info = _find_spi_info(self->data.spi_name);

    RCC_APB2PeriphClockCmd(info->sck_clk, ENABLE);
    RCC_APB2PeriphClockCmd(info->mosi_clk, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin   = info->sck_pin;
    GPIO_Init(info->sck_port, &GPIO_InitStructure);

    switch (self->data.config.direction)
    {
    case XSPI_DIR_2LINE_FULL_DUPLEX:
    case XSPI_DIR_2LINE_RX_ONLY:
        RCC_APB2PeriphClockCmd(info->miso_clk, ENABLE);
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
        GPIO_InitStructure.GPIO_Pin  = info->miso_pin;
        GPIO_Init(info->miso_port, &GPIO_InitStructure);
    case XSPI_DIR_1LINE_RX:
    case XSPI_DIR_1LINE_TX:
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
        GPIO_InitStructure.GPIO_Pin  = info->mosi_pin;
        GPIO_Init(info->mosi_port, &GPIO_InitStructure);
        break;
    default:
        break;
    }
}

static void _spi_irq_msp_init(xhal_spi_t *self)
{
    const spi_hw_info_t *info = _find_spi_info(self->data.spi_name);

    SPI_I2S_ITConfig(info->spi, SPI_I2S_IT_RXNE, ENABLE);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel                   = info->irq_spi;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = info->irq_spi_prio;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

static bool _check_spi_name_valid(const char *name)
{
    uint8_t len = strlen(name);
    if (len != 4)
    {
        return false;
    }

    if (toupper(name[0]) != 'S' || toupper(name[1]) != 'P' ||
        toupper(name[2]) != 'I')
    {
        return false;
    }

    if (!(name[3] >= '0' && name[3] <= '9'))
    {
        return false;
    }

    return true;
}

static const spi_hw_info_t *_find_spi_info(const char *name)
{
    uint8_t len = strlen(name);

    return &spi_table[name[len - 1] - '1'];
}

void SPI1_IRQHandler(void)
{
    const spi_hw_info_t *info = &spi_table[0];
    const uint8_t id          = info->id;

    if (SPI_I2S_GetITStatus(info->spi, SPI_I2S_IT_RXNE) == SET)
    {
        uint16_t data_read = SPI_I2S_ReceiveData(info->spi);
        if (spi_ctx[id].rx_buf != NULL)
        {
            if (spi_p[id]->data.config.data_bits == XSPI_DATA_BITS_8)
                ((uint8_t *)spi_ctx[id].rx_buf)[spi_ctx[id].rx_index] =
                    (uint8_t)data_read;
            else
                ((uint16_t *)spi_ctx[id].rx_buf)[spi_ctx[id].rx_index] =
                    (uint16_t)data_read;
        }

        spi_ctx[id].rx_index++;
        if (spi_ctx[id].rx_index >= spi_ctx[id].len)
        {
            SPI_Cmd(info->spi, DISABLE);
#ifdef XHAL_OS_SUPPORTING
            osEventFlagsSet(spi_p[id]->data.event_flag, XSPI_EVENT_RX_DONE);
#endif
        }
    }

    if (SPI_I2S_GetITStatus(info->spi, SPI_I2S_IT_TXE) == SET)
    {
        uint16_t data_send = XSPI_DUMMY_BYTE;
        if (spi_ctx[id].tx_buf != NULL)
        {
            if (spi_p[id]->data.config.data_bits == XSPI_DATA_BITS_8)
                data_send =
                    ((uint8_t *)spi_ctx[id].tx_buf)[spi_ctx[id].tx_index];
            else
                data_send =
                    ((uint16_t *)spi_ctx[id].tx_buf)[spi_ctx[id].tx_index];
        }
        SPI_I2S_SendData(info->spi, data_send);

        spi_ctx[id].tx_index++;
        if (spi_ctx[id].tx_index >= spi_ctx[id].len)
        {
            SPI_I2S_ITConfig(info->spi, SPI_I2S_IT_TXE, DISABLE);
#ifdef XHAL_OS_SUPPORTING
            osEventFlagsSet(spi_p[id]->data.event_flag, XSPI_EVENT_TX_DONE);
#endif
        }
    }
}

void SPI2_IRQHandler(void)
{
    const spi_hw_info_t *info = &spi_table[1];
    const uint8_t id          = info->id;

    if (SPI_I2S_GetITStatus(info->spi, SPI_I2S_IT_RXNE) == SET)
    {
        uint16_t data_read = SPI_I2S_ReceiveData(info->spi);
        if (spi_ctx[id].rx_buf != NULL)
        {
            if (spi_p[id]->data.config.data_bits == XSPI_DATA_BITS_8)
                ((uint8_t *)spi_ctx[id].rx_buf)[spi_ctx[id].rx_index] =
                    (uint8_t)data_read;
            else
                ((uint16_t *)spi_ctx[id].rx_buf)[spi_ctx[id].rx_index] =
                    (uint16_t)data_read;
        }

        spi_ctx[id].rx_index++;
        if (spi_ctx[id].rx_index >= spi_ctx[id].len)
        {
            SPI_Cmd(info->spi, DISABLE);
#ifdef XHAL_OS_SUPPORTING
            osEventFlagsSet(spi_p[id]->data.event_flag, XSPI_EVENT_RX_DONE);
#endif
        }
    }

    if (SPI_I2S_GetITStatus(info->spi, SPI_I2S_IT_TXE) == SET)
    {
        uint16_t data_send = XSPI_DUMMY_BYTE;
        if (spi_ctx[id].tx_buf != NULL)
        {
            if (spi_p[id]->data.config.data_bits == XSPI_DATA_BITS_8)
                data_send =
                    ((uint8_t *)spi_ctx[id].tx_buf)[spi_ctx[id].tx_index];
            else
                data_send =
                    ((uint16_t *)spi_ctx[id].tx_buf)[spi_ctx[id].tx_index];
        }
        SPI_I2S_SendData(info->spi, data_send);

        spi_ctx[id].tx_index++;
        if (spi_ctx[id].tx_index >= spi_ctx[id].len)
        {
            SPI_I2S_ITConfig(info->spi, SPI_I2S_IT_TXE, DISABLE);
#ifdef XHAL_OS_SUPPORTING
            osEventFlagsSet(spi_p[id]->data.event_flag, XSPI_EVENT_TX_DONE);
#endif
        }
    }
}
