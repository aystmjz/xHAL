#include "xhal_adc.h"
#include "xhal_assert.h"
#include "xhal_log.h"
#include "xhal_time.h"
#include <ctype.h>
#include <string.h>
#include XHAL_CMSIS_DEVICE_HEADER

XLOG_TAG("xDriverADC");

/* ADC_BUFFER_BLOCKS 须大于等于3 */
#define ADC_BUFFER_BLOCKS 4

typedef struct
{
    uint32_t target_samples;
    uint8_t first_sample;
} adc_ctx_t;

static volatile adc_ctx_t adc_ctx[1] = {0};
static xhal_adc_t *adc_p[1]          = {NULL};

static xhal_err_t _init(xhal_adc_t *self);
static xhal_err_t _trigger_single(xhal_adc_t *self, uint32_t samples);
static uint32_t _read_sample(xhal_adc_t *self, uint32_t samples,
                             uint16_t channel_mask, uint16_t **buffers);
static xhal_err_t _set_config(xhal_adc_t *self,
                              const xhal_adc_config_t *config);
static xhal_err_t _start_continuous(xhal_adc_t *self);
static xhal_err_t _stop_continuous(xhal_adc_t *self);
static xhal_err_t _calibrate(xhal_adc_t *self);

const xhal_adc_ops_t adc_ops_driver = {
    .init             = _init,
    .trigger_single   = _trigger_single,
    .read_sample      = _read_sample,
    .set_config       = _set_config,
    .start_continuous = _start_continuous,
    .stop_continuous  = _stop_continuous,
    .calibrate        = _calibrate,
};

typedef struct adc_hw_info adc_hw_info_t;

static xhal_err_t _adjust_buffer_size(xhal_adc_t *self);
static const adc_hw_info_t *_find_adc_info(const char *name);
static bool _check_adc_name_valid(const char *name);
static void _adc_gpio_msp_init(uint16_t channel_mask);
static void _adc_dma_irq_msp_init(xhal_adc_t *self);
static void inline _dma_config_transfer(DMA_Channel_TypeDef *DMA_CHx,
                                        u32 periph_addr, u32 mem_addr,
                                        u16 data_len);

typedef struct adc_hw_info
{
    uint8_t id;
    ADC_TypeDef *adc;

    DMA_Channel_TypeDef *dma;

    IRQn_Type irq_dma;
    uint8_t irq_dma_prio;

    uint32_t adc_clk;
    uint32_t dma_clk;
} adc_hw_info_t;

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    uint8_t channel;
    uint32_t port_clk;
} adc_channel_map_t;

static const adc_hw_info_t adc_table[] = {
    /* ----------------------------ADC1---------------------------- */
    {
        .id           = 0,
        .adc          = ADC1,
        .dma          = DMA1_Channel1,
        .irq_dma      = DMA1_Channel1_IRQn,
        .irq_dma_prio = 5,
        .adc_clk      = RCC_APB2Periph_ADC1,
        .dma_clk      = RCC_AHBPeriph_DMA1,
    }};

static const adc_channel_map_t channel_map[] = {
    {GPIOA, GPIO_Pin_0, ADC_Channel_0, RCC_APB2Periph_GPIOA},
    {GPIOA, GPIO_Pin_1, ADC_Channel_1, RCC_APB2Periph_GPIOA},
    {GPIOA, GPIO_Pin_2, ADC_Channel_2, RCC_APB2Periph_GPIOA},
    {GPIOA, GPIO_Pin_3, ADC_Channel_3, RCC_APB2Periph_GPIOA},
    {GPIOA, GPIO_Pin_4, ADC_Channel_4, RCC_APB2Periph_GPIOA},
    {GPIOA, GPIO_Pin_5, ADC_Channel_5, RCC_APB2Periph_GPIOA},
    {GPIOA, GPIO_Pin_6, ADC_Channel_6, RCC_APB2Periph_GPIOA},
    {GPIOA, GPIO_Pin_7, ADC_Channel_7, RCC_APB2Periph_GPIOA},
    {GPIOB, GPIO_Pin_0, ADC_Channel_8, RCC_APB2Periph_GPIOB},
    {GPIOB, GPIO_Pin_1, ADC_Channel_9, RCC_APB2Periph_GPIOB},
    {GPIOC, GPIO_Pin_0, ADC_Channel_10, RCC_APB2Periph_GPIOC},
    {GPIOC, GPIO_Pin_1, ADC_Channel_11, RCC_APB2Periph_GPIOC},
    {GPIOC, GPIO_Pin_2, ADC_Channel_12, RCC_APB2Periph_GPIOC},
    {GPIOC, GPIO_Pin_3, ADC_Channel_13, RCC_APB2Periph_GPIOC},
    {GPIOC, GPIO_Pin_4, ADC_Channel_14, RCC_APB2Periph_GPIOC},
    {GPIOC, GPIO_Pin_5, ADC_Channel_15, RCC_APB2Periph_GPIOC},
};

static xhal_err_t _init(xhal_adc_t *self)
{
    xassert_name(_check_adc_name_valid(self->data.name), self->data.name);
    xassert_name(_adjust_buffer_size(self) == XHAL_OK,
                 "ADC buffer size too small");

    xhal_err_t ret            = XHAL_OK;
    const adc_hw_info_t *info = _find_adc_info(self->data.name);
    adc_p[info->id]           = self;

    RCC_APB2PeriphClockCmd(info->adc_clk, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div8);

    _adc_gpio_msp_init(self->data.channel_mask);
    ret = _set_config(self, &self->data.config);
    _adc_dma_irq_msp_init(self);

    _calibrate(self);

    return ret;
}

static xhal_err_t _trigger_single(xhal_adc_t *self, uint32_t samples)
{
    const adc_hw_info_t *info = _find_adc_info(self->data.name);

    adc_ctx[info->id].target_samples = samples;
    adc_ctx[info->id].first_sample   = 1;

    _start_continuous(self);

    return XHAL_OK;
}

static uint32_t _read_sample(xhal_adc_t *self, uint32_t samples,
                             uint16_t channel_mask, uint16_t **buffers)
{
    xhal_adc_t *adc       = XADC_CAST(self);
    uint8_t map[16]       = {0};
    uint8_t channel_count = 0;
    uint8_t write_count   = 0;

    for (uint8_t i = 0; i < 16; i++)
    {
        if (self->data.channel_mask & (1U << i))
        {
            if (channel_mask & (1U << i))
            {
                map[write_count++] = channel_count;
            }
            channel_count++;
        }
    }

    uint16_t sample[16];
    for (uint32_t num = 0; num < samples; num++)
    {
        uint32_t r = xrbuf_read(&adc->data.data_rbuf, sample,
                                sizeof(uint16_t) * channel_count);
        if (r != (sizeof(uint16_t) * channel_count))
            return num;

        for (uint8_t i = 0; i < write_count; i++)
        {
            buffers[i][num] = sample[map[i]];
        }
    }

    return samples;
}

static xhal_err_t _set_config(xhal_adc_t *self, const xhal_adc_config_t *config)
{
    const adc_hw_info_t *info = _find_adc_info(self->data.name);

    ADC_Cmd(info->adc, DISABLE);

    uint8_t channel_count = 0;
    for (uint8_t i = 0; i < 16; i++)
    {
        if (self->data.channel_mask & (1U << i))
        {
            channel_count++;
        }
    }

    ADC_InitTypeDef ADC_InitStructure;
    ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode       = ENABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel       = channel_count;
    ADC_Init(info->adc, &ADC_InitStructure);

    uint8_t rank = 0;
    for (uint8_t i = 0; i < 16; i++)
    {
        if (self->data.channel_mask & (1U << i))
        {
            ADC_RegularChannelConfig(info->adc, channel_map[i].channel, ++rank,
                                     ADC_SampleTime_239Cycles5);
        }
    }

    ADC_Cmd(info->adc, ENABLE);

    return XHAL_OK;
}

static xhal_err_t _start_continuous(xhal_adc_t *self)
{
    const adc_hw_info_t *info = _find_adc_info(self->data.name);

    self->data.sample_count   = 0;
    self->data.overflow_count = 0;

    xrbuf_reset(&self->data.data_rbuf);

    void *addr = xrbuf_get_linear_block_write_address(&self->data.data_rbuf);
    uint32_t advance =
        self->data.data_rbuf.size / sizeof(uint16_t) / ADC_BUFFER_BLOCKS;
    _dma_config_transfer(info->dma, (u32)&info->adc->DR, (u32)addr, advance);

    ADC_SoftwareStartConvCmd(info->adc, ENABLE);

    return XHAL_OK;
}

static xhal_err_t _stop_continuous(xhal_adc_t *self)
{
    const adc_hw_info_t *info = _find_adc_info(self->data.name);

    ADC_SoftwareStartConvCmd(info->adc, DISABLE);

    return XHAL_OK;
}

static xhal_err_t _calibrate(xhal_adc_t *self)
{
    const adc_hw_info_t *info = _find_adc_info(self->data.name);

    ADC_ResetCalibration(info->adc);
    while (ADC_GetResetCalibrationStatus(info->adc))
    {
    };

    ADC_StartCalibration(info->adc);
    while (ADC_GetCalibrationStatus(info->adc))
    {
    };

    return XHAL_OK;
}

static xhal_err_t _adjust_buffer_size(xhal_adc_t *self)
{
    uint8_t channel_count = 0;
    for (uint8_t i = 0; i < 16; i++)
    {
        if (self->data.channel_mask & (1U << i))
        {
            channel_count++;
        }
    }

    uint32_t base = channel_count * sizeof(uint16_t) * ADC_BUFFER_BLOCKS;
    uint32_t current_size = self->data.data_rbuf.size;

    /* 如果当前大小小于 base，则无法减小到有效的非零倍数 */
    if (current_size < base)
    {
        return XHAL_ERR_INVALID;
    }

    /* 计算最大的 base 的倍数（不超过当前大小） */
    self->data.data_rbuf.size = (current_size / base) * base;

    return XHAL_OK;
}

static const adc_hw_info_t *_find_adc_info(const char *name)
{
    uint8_t len = strlen(name);

    return &adc_table[name[len - 1] - '1'];
}

static bool _check_adc_name_valid(const char *name)
{
    uint8_t len = strlen(name);
    if (len != 4)
    {
        return false;
    }

    if (toupper(name[0]) != 'A' || toupper(name[1]) != 'D' ||
        toupper(name[2]) != 'C')
    {
        return false;
    }

    if (name[3] != '1')
    {
        return false;
    }

    return true;
}

static void _adc_gpio_msp_init(uint16_t channel_mask)
{
    for (int i = 0; i < 16; i++)
    {
        if (channel_mask & (1 << i))
        {
            const adc_channel_map_t *ch_info = &channel_map[i];

            RCC_APB2PeriphClockCmd(ch_info->port_clk, ENABLE);

            GPIO_InitTypeDef GPIO_InitStructure;
            GPIO_InitStructure.GPIO_Pin   = ch_info->pin;
            GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AIN;
            GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
            GPIO_Init(ch_info->port, &GPIO_InitStructure);
        }
    }
}

static void _adc_dma_irq_msp_init(xhal_adc_t *self)
{
    const adc_hw_info_t *info = _find_adc_info(self->data.name);

    RCC_AHBPeriphClockCmd(info->dma_clk, ENABLE);

    DMA_InitTypeDef DMA_InitStructure;

    DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)&info->adc->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr     = 0;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize         = 0;
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(info->dma, &DMA_InitStructure);
    DMA_Cmd(info->dma, DISABLE);

    DMA_ITConfig(info->dma, DMA_IT_TC, ENABLE);
    DMA_ITConfig(info->dma, DMA_IT_HT, ENABLE);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel                   = info->irq_dma;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = info->irq_dma_prio;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    ADC_DMACmd(info->adc, ENABLE);
}

static void inline _dma_config_transfer(DMA_Channel_TypeDef *DMA_CHx,
                                        u32 periph_addr, u32 mem_addr,
                                        u16 data_len)
{
    DMA_CHx->CCR &= ~DMA_CCR1_EN; /* 禁用 DMA 通道 */
    DMA_CHx->CPAR  = periph_addr; /* 配置外设地址 */
    DMA_CHx->CMAR  = mem_addr;    /* 配置内存地址 */
    DMA_CHx->CNDTR = data_len;    /* 配置传输数据数量 */
    DMA_CHx->CCR |= DMA_CCR1_EN;  /* 使能 DMA 通道 */
}

void DMA1_Channel1_IRQHandler(void)
{
    const uint32_t DMAy_IT_HTx = DMA1_IT_HT1;
    const uint32_t DMAy_IT_TCx = DMA1_IT_TC1;

    const adc_hw_info_t *info = &adc_table[0];
    xhal_adc_t *adc           = adc_p[info->id];

    if (adc == NULL)
        return;

    uint32_t advance =
        adc->data.data_rbuf.size / sizeof(uint16_t) / ADC_BUFFER_BLOCKS;
    if (DMA_GetITStatus(DMAy_IT_HTx) != RESET)
    {
        DMA_ClearITPendingBit(DMAy_IT_HTx);

        if (adc->data.config.mode == XADC_MODE_REALTIME &&
            adc->data.sample_count >= adc_ctx[info->id].target_samples)
        {

            DMA_Cmd(info->dma, DISABLE);
            _stop_continuous(adc);
            return;
        }

        uint8_t channel_count = __builtin_popcount(adc->data.channel_mask);
        if (!adc_ctx[info->id].first_sample)
        {
            xrbuf_advance(&adc->data.data_rbuf, advance * sizeof(uint16_t));
            adc->data.sample_count += advance / channel_count;
        }
        else
        {
            adc_ctx[info->id].first_sample = 0;
        }

        uint32_t free =
            xrbuf_get_linear_block_write_length(&adc->data.data_rbuf) /
            sizeof(uint16_t);
        if (free < advance)
        {
            adc->data.overflow_count += (advance - free) / channel_count;
            xrbuf_skip(&adc->data.data_rbuf, advance * sizeof(uint16_t));
        }

#ifdef XHAL_OS_SUPPORTING
        osEventFlagsSet(adc->data.event_flag, XADC_EVENT_DATA_READY);
#endif
    }
    else if (DMA_GetITStatus(DMAy_IT_TCx) != RESET)
    {
        DMA_ClearITPendingBit(DMAy_IT_TCx);

        void *addr = xrbuf_get_linear_block_write_address(&adc->data.data_rbuf);
        _dma_config_transfer(info->dma, (u32)&info->adc->DR, (u32)addr,
                             advance);
    }
}
