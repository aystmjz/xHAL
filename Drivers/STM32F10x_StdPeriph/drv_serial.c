#include "xhal_assert.h"
#include "xhal_log.h"
#include "xhal_os.h"
#include "xhal_ringbuf.h"
#include "xhal_serial.h"
#include "xhal_time.h"
#include <ctype.h>
#include <string.h>

#include XHAL_CMSIS_DEVICE_HEADER

XLOG_TAG("xDriverSerial");

static volatile uint16_t uart_tx_dma_len[3] = {0};
static volatile uint16_t uart_rx_dma_len[3] = {0};
static xhal_serial_t *usart_p[3]            = {NULL};

static xhal_err_t _init(xhal_serial_t *self);
static xhal_err_t _set_attr(xhal_serial_t *self,
                            const xhal_serial_attr_t *attr);
static uint32_t _transmit(xhal_serial_t *self, const void *buff, uint32_t size);

const xhal_serial_ops_t serial_ops_driver = {
    .init     = _init,
    .set_attr = _set_attr,
    .transmit = _transmit,
};

typedef struct usart_hw_info usart_hw_info_t;

static bool _strcasecmp_upper(const char *s1, const char *s2);
static bool _check_usart_name_valid(const char *name);
static const usart_hw_info_t *_find_usart_info(const char *name);

static void _usart_gpio_msp_init(xhal_serial_t *self);
static void _usart_dma_irq_msp_init(xhal_serial_t *self);

static void _dma_config_transfer(DMA_Channel_TypeDef *DMA_CHx, u32 periph_addr,
                                 u32 mem_addr, u16 data_len);

typedef struct usart_hw_info
{
    uint8_t id;           /* 外设编号 */
    USART_TypeDef *usart; /* 外设基址 */

    DMA_Channel_TypeDef *dma_tx; /* DMA 发送通道 */
    DMA_Channel_TypeDef *dma_rx; /* DMA 接收通道 */

    IRQn_Type irq_usart;     /* USART 中断号 */
    uint8_t irq_usart_prio;  /* USART 中断优先级 */
    IRQn_Type irq_dma_tx;    /* DMA TX 中断号 */
    uint8_t irq_dma_tx_prio; /* DMA TX 中断优先级 */
    IRQn_Type irq_dma_rx;    /* DMA RX 中断号 */
    uint8_t irq_dma_rx_prio; /* DMA RX 中断优先级 */

    GPIO_TypeDef *tx_port; /* TX 引脚端口 */
    uint16_t tx_pin;       /* TX 引脚编号 */
    GPIO_TypeDef *rx_port; /* RX 引脚端口 */
    uint16_t rx_pin;       /* RX 引脚编号 */

    uint32_t tx_port_clk; /* TX 端口时钟 */
    uint32_t rx_port_clk; /* RX 端口时钟 */

    uint32_t usart_clk; /* USART 外设时钟 */
} usart_hw_info_t;

static const usart_hw_info_t usart_table[] = {
    {
        .id              = 0,
        .usart           = USART1,
        .dma_tx          = DMA1_Channel4,
        .dma_rx          = DMA1_Channel5,
        .irq_usart       = USART1_IRQn,
        .irq_usart_prio  = 7,
        .irq_dma_tx      = DMA1_Channel4_IRQn,
        .irq_dma_tx_prio = 6,
        .irq_dma_rx      = DMA1_Channel5_IRQn,
        .irq_dma_rx_prio = 5,
        .tx_port         = GPIOA,
        .tx_pin          = GPIO_Pin_9,
        .rx_port         = GPIOA,
        .rx_pin          = GPIO_Pin_10,
        .tx_port_clk     = RCC_APB2Periph_GPIOA,
        .rx_port_clk     = RCC_APB2Periph_GPIOA,
        .usart_clk       = RCC_APB2Periph_USART1,
    },
    {
        .id              = 1,
        .usart           = USART2,
        .dma_tx          = DMA1_Channel7,
        .dma_rx          = DMA1_Channel6,
        .irq_usart       = USART2_IRQn,
        .irq_usart_prio  = 7,
        .irq_dma_tx      = DMA1_Channel7_IRQn,
        .irq_dma_tx_prio = 6,
        .irq_dma_rx      = DMA1_Channel6_IRQn,
        .irq_dma_rx_prio = 5,
        .tx_port         = GPIOA,
        .tx_pin          = GPIO_Pin_2,
        .rx_port         = GPIOA,
        .rx_pin          = GPIO_Pin_3,
        .tx_port_clk     = RCC_APB2Periph_GPIOA,
        .rx_port_clk     = RCC_APB2Periph_GPIOA,
        .usart_clk       = RCC_APB1Periph_USART2,
    },
    {
        .id              = 2,
        .usart           = USART3,
        .dma_tx          = DMA1_Channel2,
        .dma_rx          = DMA1_Channel3,
        .irq_usart       = USART3_IRQn,
        .irq_usart_prio  = 7,
        .irq_dma_tx      = DMA1_Channel2_IRQn,
        .irq_dma_tx_prio = 6,
        .irq_dma_rx      = DMA1_Channel3_IRQn,
        .irq_dma_rx_prio = 5,
        .tx_port         = GPIOB,
        .tx_pin          = GPIO_Pin_10,
        .rx_port         = GPIOB,
        .rx_pin          = GPIO_Pin_11,
        .tx_port_clk     = RCC_APB2Periph_GPIOB,
        .rx_port_clk     = RCC_APB2Periph_GPIOB,
        .usart_clk       = RCC_APB1Periph_USART3,
    },
};

static xhal_err_t _init(xhal_serial_t *self)
{
    xassert_name(_check_usart_name_valid(self->data.name), self->data.name);

    const usart_hw_info_t *info = _find_usart_info(self->data.name);
    xhal_err_t ret              = XHAL_OK;

    usart_p[info->id]         = self;
    uart_rx_dma_len[info->id] = self->data.rx_rbuf.size;

    _usart_gpio_msp_init(self);
    ret = _set_attr(self, &self->data.attr);
    _usart_dma_irq_msp_init(self);

    _dma_config_transfer(info->dma_rx, (u32)&info->usart->DR,
                         (u32)self->data.rx_rbuf.buff, self->data.rx_rbuf.size);

    return ret;
}
static xhal_err_t _set_attr(xhal_serial_t *self, const xhal_serial_attr_t *attr)
{
    const usart_hw_info_t *info = _find_usart_info(self->data.name);

    if (info->usart_clk == RCC_APB2Periph_USART1)
    {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    }
    else
    {
        RCC_APB1PeriphClockCmd(info->usart_clk, ENABLE);
    }

    USART_Cmd(info->usart, DISABLE);

    USART_InitTypeDef USART_InitStruct;
    USART_InitStruct.USART_BaudRate   = attr->baud_rate;
    USART_InitStruct.USART_WordLength = (attr->data_bits == XSERIAL_DATA_BITS_8)
                                            ? USART_WordLength_8b
                                            : USART_WordLength_9b;
    USART_InitStruct.USART_StopBits   = (attr->stop_bits == XSERIAL_STOP_BITS_1)
                                            ? USART_StopBits_1
                                            : USART_StopBits_2;
    switch (attr->parity)
    {
    case XSERIAL_PARITY_ODD:
        USART_InitStruct.USART_Parity = USART_Parity_Odd;
        break;
    case XSERIAL_PARITY_EVEN:
        USART_InitStruct.USART_Parity = USART_Parity_Even;
        break;
    case XSERIAL_PARITY_NONE:
        USART_InitStruct.USART_Parity = USART_Parity_No;
        break;
    default:
        break;
    }
    USART_InitStruct.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(info->usart, &USART_InitStruct);

    USART_Cmd(info->usart, ENABLE);

    return XHAL_OK;
}

static uint32_t _transmit(xhal_serial_t *self, const void *buff, uint32_t size)
{
    uint32_t written =
        xrbuf_write(&self->data.tx_rbuf, (const uint8_t *)buff, size);

    const usart_hw_info_t *info = _find_usart_info(self->data.name);

    if (DMA_GetCurrDataCounter(info->dma_tx) == 0)
    {
        uint16_t len = xrbuf_get_linear_block_read_length(&self->data.tx_rbuf);
        if (len == 0)
        {
            return written;
        }

        uart_tx_dma_len[info->id] = len;

        void *addr = xrbuf_get_linear_block_read_address(&self->data.tx_rbuf);
        _dma_config_transfer(info->dma_tx, (u32)&info->usart->DR, (u32)addr,
                             len);
    }

    return written;
}

static bool _strcasecmp_upper(const char *s1, const char *s2)
{
    while (*s1 && *s2)
    {
        if (toupper((unsigned char)*s1) != toupper((unsigned char)*s2))
            return false;
        s1++;
        s2++;
    }
    return (*s1 == '\0' && *s2 == '\0');
}

static bool _check_usart_name_valid(const char *name)
{
    if (!name)
        return false;

    uint8_t len = strlen(name);
    if (len < 5 || len > 6)
        return false;

    /* 支持的串口名字 */
    const char *valid_names[] = {"UART1",  "UART2",  "UART3",
                                 "USART1", "USART2", "USART3"};

    for (size_t i = 0; i < sizeof(valid_names) / sizeof(valid_names[0]); i++)
    {
        if (_strcasecmp_upper(name, valid_names[i]))
        {
            return true;
        }
    }

    return false;
}

static const usart_hw_info_t *_find_usart_info(const char *name)
{
    uint8_t len = strlen(name);

    return &usart_table[name[len - 1] - '1'];
}

static void _usart_gpio_msp_init(xhal_serial_t *self)
{
    const usart_hw_info_t *info = _find_usart_info(self->data.name);

    RCC_APB2PeriphClockCmd(info->tx_port_clk, ENABLE);
    RCC_APB2PeriphClockCmd(info->rx_port_clk, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin   = info->tx_pin;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(info->tx_port, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin  = info->rx_pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(info->rx_port, &GPIO_InitStructure);
}

static void _dma_config_transfer(DMA_Channel_TypeDef *DMA_CHx, u32 periph_addr,
                                 u32 mem_addr, u16 data_len)
{
    DMA_CHx->CCR &= ~DMA_CCR1_EN; /* 禁用 DMA 通道 */
    DMA_CHx->CPAR  = periph_addr; /* 配置外设地址 */
    DMA_CHx->CMAR  = mem_addr;    /* 配置内存地址 */
    DMA_CHx->CNDTR = data_len;    /* 配置传输数据数量 */
    DMA_CHx->CCR |= DMA_CCR1_EN;  /* 使能 DMA 通道 */
}

static void _usart_dma_irq_msp_init(xhal_serial_t *self)
{
    const usart_hw_info_t *info = _find_usart_info(self->data.name);

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    /* DMA (TX) */
    DMA_InitTypeDef DMA_InitStructure;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)&info->usart->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr     = 0;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize         = 0;
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_Medium;
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(info->dma_tx, &DMA_InitStructure);
    DMA_Cmd(info->dma_tx, DISABLE);

    DMA_ITConfig(info->dma_tx, DMA_IT_TC, ENABLE);
    DMA_ITConfig(info->dma_tx, DMA_IT_HT, ENABLE);

    /* NVIC for DMA (TX) */
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = info->irq_dma_tx;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority =
        info->irq_dma_tx_prio;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd         = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&info->usart->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr     = 0;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize         = 0;
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(info->dma_rx, &DMA_InitStructure);
    DMA_Cmd(info->dma_rx, DISABLE);

    DMA_ITConfig(info->dma_rx, DMA_IT_TC, ENABLE);
    DMA_ITConfig(info->dma_rx, DMA_IT_HT, ENABLE);

    /* NVIC for DMA (RX) */
    NVIC_InitStructure.NVIC_IRQChannel = info->irq_dma_rx;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority =
        info->irq_dma_rx_prio;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd         = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_DMACmd(info->usart, USART_DMAReq_Tx, ENABLE);
    USART_DMACmd(info->usart, USART_DMAReq_Rx, ENABLE);
    USART_ITConfig(info->usart, USART_IT_IDLE, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel                   = info->irq_usart;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = info->irq_usart_prio;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/* ==== USART1 IRQ handlers ==== */
void USART1_IRQHandler(void)
{
    const usart_hw_info_t *info = &usart_table[0];
    const uint8_t id            = info->id;

    if (USART_GetITStatus(info->usart, USART_IT_IDLE) != RESET)
    {
        /* 立即清除 IDLE 中断标志（SR + DR） */
        XHAL_UNUSED(info->usart->SR);
        XHAL_UNUSED(info->usart->DR);

        uint16_t remaining = DMA_GetCurrDataCounter(info->dma_rx);
        uint16_t received  = uart_rx_dma_len[id] - remaining;

        xrbuf_advance(&usart_p[id]->data.rx_rbuf, received);
        uart_rx_dma_len[id] = 0;

#ifdef XHAL_OS_SUPPORTING
        if (xrbuf_get_full(&usart_p[id]->data.rx_rbuf) >=
            usart_p[id]->data.rx_expect)
        {
            osEventFlagsSet(usart_p[id]->data.event_flag,
                            XSERIAL_EVENT_CAN_READ);
        }
#endif

        uint16_t len =
            xrbuf_get_linear_block_write_length(&usart_p[id]->data.rx_rbuf);
        if (len == 0)
        {
            return;
        }
        uart_rx_dma_len[id] = len;

        void *addr =
            xrbuf_get_linear_block_write_address(&usart_p[id]->data.rx_rbuf);
        _dma_config_transfer(info->dma_rx, (u32)&info->usart->DR, (u32)addr,
                             len);
    }
}

void DMA1_Channel4_IRQHandler(void)
{
    const uint32_t DMAy_IT_HTx = DMA1_IT_HT4;
    const uint32_t DMAy_IT_TCx = DMA1_IT_TC4;

    const usart_hw_info_t *info = &usart_table[0];
    const uint8_t id            = info->id;

    if (DMA_GetITStatus(DMAy_IT_HTx) != RESET)
    {
        DMA_ClearITPendingBit(DMAy_IT_HTx);
        uint16_t remaining = DMA_GetCurrDataCounter(info->dma_tx);
        uint16_t sent      = uart_tx_dma_len[id] - remaining;

        uart_tx_dma_len[id] = remaining;

        xrbuf_skip(&usart_p[id]->data.tx_rbuf, sent);

#ifdef XHAL_OS_SUPPORTING
        osEventFlagsSet(usart_p[id]->data.event_flag, XSERIAL_EVENT_CAN_WRITE);
#endif
    }
    else if (DMA_GetITStatus(DMAy_IT_TCx) != RESET)
    {
        DMA_ClearITPendingBit(DMAy_IT_TCx);

        xrbuf_skip(&usart_p[id]->data.tx_rbuf, uart_tx_dma_len[id]);
        uart_tx_dma_len[id] = 0;

#ifdef XHAL_OS_SUPPORTING
        osEventFlagsSet(usart_p[id]->data.event_flag, XSERIAL_EVENT_CAN_WRITE);
#endif
        uint16_t len =
            xrbuf_get_linear_block_read_length(&usart_p[id]->data.tx_rbuf);
        if (len == 0)
        {
            return;
        }

        uart_tx_dma_len[0] = len;

        void *addr =
            xrbuf_get_linear_block_read_address(&usart_p[id]->data.tx_rbuf);
        _dma_config_transfer(info->dma_tx, (u32)&info->usart->DR, (u32)addr,
                             len);
    }
}

void DMA1_Channel5_IRQHandler(void)
{
    const uint32_t DMAy_IT_HTx = DMA1_IT_HT5;
    const uint32_t DMAy_IT_TCx = DMA1_IT_TC5;

    const usart_hw_info_t *info = &usart_table[0];
    const uint8_t id            = info->id;

    if (DMA_GetITStatus(DMAy_IT_HTx) != RESET)
    {
        DMA_ClearITPendingBit(DMAy_IT_HTx);

        uint16_t remaining = DMA_GetCurrDataCounter(info->dma_rx);
        uint16_t received  = uart_rx_dma_len[id] - remaining;

        uart_rx_dma_len[id] = remaining;

        xrbuf_advance(&usart_p[id]->data.rx_rbuf, received);

#ifdef XHAL_OS_SUPPORTING
        if (xrbuf_get_full(&usart_p[id]->data.rx_rbuf) >=
            usart_p[id]->data.rx_expect)
        {
            osEventFlagsSet(usart_p[id]->data.event_flag,
                            XSERIAL_EVENT_CAN_READ);
        }
#endif
    }
    else if (DMA_GetITStatus(DMAy_IT_TCx) != RESET)
    {
        DMA_ClearITPendingBit(DMAy_IT_TCx);

        xrbuf_advance(&usart_p[id]->data.rx_rbuf, uart_rx_dma_len[id]);
        uart_rx_dma_len[id] = 0;

#ifdef XHAL_OS_SUPPORTING
        if (xrbuf_get_full(&usart_p[id]->data.rx_rbuf) >
            usart_p[id]->data.rx_expect)
        {
            osEventFlagsSet(usart_p[id]->data.event_flag,
                            XSERIAL_EVENT_CAN_READ);
        }
#endif
        uint16_t len =
            xrbuf_get_linear_block_write_length(&usart_p[id]->data.rx_rbuf);
        if (len == 0)
        {
            return;
        }

        uart_rx_dma_len[id] = len;

        void *addr =
            xrbuf_get_linear_block_write_address(&usart_p[id]->data.rx_rbuf);
        _dma_config_transfer(info->dma_rx, (u32)&info->usart->DR, (u32)addr,
                             len);
    }
}

/* ==== USART2 IRQ handlers ==== */
void USART2_IRQHandler(void)
{
    const usart_hw_info_t *info = &usart_table[1];
    const uint8_t id            = info->id;

    if (USART_GetITStatus(info->usart, USART_IT_IDLE) != RESET)
    {
        /* 立即清除 IDLE 中断标志（SR + DR） */
        XHAL_UNUSED(info->usart->SR);
        XHAL_UNUSED(info->usart->DR);

        uint16_t remaining = DMA_GetCurrDataCounter(info->dma_rx);
        uint16_t received  = uart_rx_dma_len[id] - remaining;

        xrbuf_advance(&usart_p[id]->data.rx_rbuf, received);
        uart_rx_dma_len[id] = 0;

#ifdef XHAL_OS_SUPPORTING
        if (xrbuf_get_full(&usart_p[id]->data.rx_rbuf) >=
            usart_p[id]->data.rx_expect)
        {
            osEventFlagsSet(usart_p[id]->data.event_flag,
                            XSERIAL_EVENT_CAN_READ);
        }
#endif

        uint16_t len =
            xrbuf_get_linear_block_write_length(&usart_p[id]->data.rx_rbuf);
        if (len == 0)
        {
            return;
        }
        uart_rx_dma_len[id] = len;

        void *addr =
            xrbuf_get_linear_block_write_address(&usart_p[id]->data.rx_rbuf);
        _dma_config_transfer(info->dma_rx, (u32)&info->usart->DR, (u32)addr,
                             len);
    }
}

void DMA1_Channel7_IRQHandler(void)
{
    const uint32_t DMAy_IT_HTx = DMA1_IT_HT7;
    const uint32_t DMAy_IT_TCx = DMA1_IT_TC7;

    const usart_hw_info_t *info = &usart_table[1];
    const uint8_t id            = info->id;

    if (DMA_GetITStatus(DMAy_IT_HTx) != RESET)
    {
        DMA_ClearITPendingBit(DMAy_IT_HTx);
        uint16_t remaining = DMA_GetCurrDataCounter(info->dma_tx);
        uint16_t sent      = uart_tx_dma_len[id] - remaining;

        uart_tx_dma_len[id] = remaining;

        xrbuf_skip(&usart_p[id]->data.tx_rbuf, sent);

#ifdef XHAL_OS_SUPPORTING
        osEventFlagsSet(usart_p[id]->data.event_flag, XSERIAL_EVENT_CAN_WRITE);
#endif
    }
    else if (DMA_GetITStatus(DMAy_IT_TCx) != RESET)
    {
        DMA_ClearITPendingBit(DMAy_IT_TCx);

        xrbuf_skip(&usart_p[id]->data.tx_rbuf, uart_tx_dma_len[id]);
        uart_tx_dma_len[id] = 0;

#ifdef XHAL_OS_SUPPORTING
        osEventFlagsSet(usart_p[id]->data.event_flag, XSERIAL_EVENT_CAN_WRITE);
#endif
        uint16_t len =
            xrbuf_get_linear_block_read_length(&usart_p[id]->data.tx_rbuf);
        if (len == 0)
        {
            return;
        }

        uart_tx_dma_len[0] = len;

        void *addr =
            xrbuf_get_linear_block_read_address(&usart_p[id]->data.tx_rbuf);
        _dma_config_transfer(info->dma_tx, (u32)&info->usart->DR, (u32)addr,
                             len);
    }
}

void DMA1_Channel6_IRQHandler(void)
{
    const uint32_t DMAy_IT_HTx = DMA1_IT_HT6;
    const uint32_t DMAy_IT_TCx = DMA1_IT_TC6;

    const usart_hw_info_t *info = &usart_table[1];
    const uint8_t id            = info->id;

    if (DMA_GetITStatus(DMAy_IT_HTx) != RESET)
    {
        DMA_ClearITPendingBit(DMAy_IT_HTx);

        uint16_t remaining = DMA_GetCurrDataCounter(info->dma_rx);
        uint16_t received  = uart_rx_dma_len[id] - remaining;

        uart_rx_dma_len[id] = remaining;

        xrbuf_advance(&usart_p[id]->data.rx_rbuf, received);

#ifdef XHAL_OS_SUPPORTING
        if (xrbuf_get_full(&usart_p[id]->data.rx_rbuf) >=
            usart_p[id]->data.rx_expect)
        {
            osEventFlagsSet(usart_p[id]->data.event_flag,
                            XSERIAL_EVENT_CAN_READ);
        }
#endif
    }
    else if (DMA_GetITStatus(DMAy_IT_TCx) != RESET)
    {
        DMA_ClearITPendingBit(DMAy_IT_TCx);

        xrbuf_advance(&usart_p[id]->data.rx_rbuf, uart_rx_dma_len[id]);
        uart_rx_dma_len[id] = 0;

#ifdef XHAL_OS_SUPPORTING
        if (xrbuf_get_full(&usart_p[id]->data.rx_rbuf) >
            usart_p[id]->data.rx_expect)
        {
            osEventFlagsSet(usart_p[id]->data.event_flag,
                            XSERIAL_EVENT_CAN_READ);
        }
#endif
        uint16_t len =
            xrbuf_get_linear_block_write_length(&usart_p[id]->data.rx_rbuf);
        if (len == 0)
        {
            return;
        }

        uart_rx_dma_len[id] = len;

        void *addr =
            xrbuf_get_linear_block_write_address(&usart_p[id]->data.rx_rbuf);
        _dma_config_transfer(info->dma_rx, (u32)&info->usart->DR, (u32)addr,
                             len);
    }
}

/* ==== USART3 IRQ handlers ==== */
void USART3_IRQHandler(void)
{
    const usart_hw_info_t *info = &usart_table[2];
    const uint8_t id            = info->id;

    if (USART_GetITStatus(info->usart, USART_IT_IDLE) != RESET)
    {
        /* 立即清除 IDLE 中断标志（SR + DR） */
        XHAL_UNUSED(info->usart->SR);
        XHAL_UNUSED(info->usart->DR);

        uint16_t remaining = DMA_GetCurrDataCounter(info->dma_rx);
        uint16_t received  = uart_rx_dma_len[id] - remaining;

        xrbuf_advance(&usart_p[id]->data.rx_rbuf, received);
        uart_rx_dma_len[id] = 0;

#ifdef XHAL_OS_SUPPORTING
        if (xrbuf_get_full(&usart_p[id]->data.rx_rbuf) >=
            usart_p[id]->data.rx_expect)
        {
            osEventFlagsSet(usart_p[id]->data.event_flag,
                            XSERIAL_EVENT_CAN_READ);
        }
#endif

        uint16_t len =
            xrbuf_get_linear_block_write_length(&usart_p[id]->data.rx_rbuf);
        if (len == 0)
        {
            return;
        }
        uart_rx_dma_len[id] = len;

        void *addr =
            xrbuf_get_linear_block_write_address(&usart_p[id]->data.rx_rbuf);
        _dma_config_transfer(info->dma_rx, (u32)&info->usart->DR, (u32)addr,
                             len);
    }
}

void DMA1_Channel2_IRQHandler(void)
{
    const uint32_t DMAy_IT_HTx = DMA1_IT_HT2;
    const uint32_t DMAy_IT_TCx = DMA1_IT_TC2;

    const usart_hw_info_t *info = &usart_table[2];
    const uint8_t id            = info->id;

    if (DMA_GetITStatus(DMAy_IT_HTx) != RESET)
    {
        DMA_ClearITPendingBit(DMAy_IT_HTx);
        uint16_t remaining = DMA_GetCurrDataCounter(info->dma_tx);
        uint16_t sent      = uart_tx_dma_len[id] - remaining;

        uart_tx_dma_len[id] = remaining;

        xrbuf_skip(&usart_p[id]->data.tx_rbuf, sent);

#ifdef XHAL_OS_SUPPORTING
        osEventFlagsSet(usart_p[id]->data.event_flag, XSERIAL_EVENT_CAN_WRITE);
#endif
    }
    else if (DMA_GetITStatus(DMAy_IT_TCx) != RESET)
    {
        DMA_ClearITPendingBit(DMAy_IT_TCx);

        xrbuf_skip(&usart_p[id]->data.tx_rbuf, uart_tx_dma_len[id]);
        uart_tx_dma_len[id] = 0;

#ifdef XHAL_OS_SUPPORTING
        osEventFlagsSet(usart_p[id]->data.event_flag, XSERIAL_EVENT_CAN_WRITE);
#endif
        uint16_t len =
            xrbuf_get_linear_block_read_length(&usart_p[id]->data.tx_rbuf);
        if (len == 0)
        {
            return;
        }

        uart_tx_dma_len[0] = len;

        void *addr =
            xrbuf_get_linear_block_read_address(&usart_p[id]->data.tx_rbuf);
        _dma_config_transfer(info->dma_tx, (u32)&info->usart->DR, (u32)addr,
                             len);
    }
}

void DMA1_Channel3_IRQHandler(void)
{
    const uint32_t DMAy_IT_HTx = DMA1_IT_HT3;
    const uint32_t DMAy_IT_TCx = DMA1_IT_TC3;

    const usart_hw_info_t *info = &usart_table[2];
    const uint8_t id            = info->id;

    if (DMA_GetITStatus(DMAy_IT_HTx) != RESET)
    {
        DMA_ClearITPendingBit(DMAy_IT_HTx);

        uint16_t remaining = DMA_GetCurrDataCounter(info->dma_rx);
        uint16_t received  = uart_rx_dma_len[id] - remaining;

        uart_rx_dma_len[id] = remaining;

        xrbuf_advance(&usart_p[id]->data.rx_rbuf, received);

#ifdef XHAL_OS_SUPPORTING
        if (xrbuf_get_full(&usart_p[id]->data.rx_rbuf) >=
            usart_p[id]->data.rx_expect)
        {
            osEventFlagsSet(usart_p[id]->data.event_flag,
                            XSERIAL_EVENT_CAN_READ);
        }
#endif
    }
    else if (DMA_GetITStatus(DMAy_IT_TCx) != RESET)
    {
        DMA_ClearITPendingBit(DMAy_IT_TCx);

        xrbuf_advance(&usart_p[id]->data.rx_rbuf, uart_rx_dma_len[id]);
        uart_rx_dma_len[id] = 0;

#ifdef XHAL_OS_SUPPORTING
        if (xrbuf_get_full(&usart_p[id]->data.rx_rbuf) >
            usart_p[id]->data.rx_expect)
        {
            osEventFlagsSet(usart_p[id]->data.event_flag,
                            XSERIAL_EVENT_CAN_READ);
        }
#endif
        uint16_t len =
            xrbuf_get_linear_block_write_length(&usart_p[id]->data.rx_rbuf);
        if (len == 0)
        {
            return;
        }

        uart_rx_dma_len[id] = len;

        void *addr =
            xrbuf_get_linear_block_write_address(&usart_p[id]->data.rx_rbuf);
        _dma_config_transfer(info->dma_rx, (u32)&info->usart->DR, (u32)addr,
                             len);
    }
}