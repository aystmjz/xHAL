#include "xhal_adc.h"
#include "xhal_export.h"

#define ADC_BUFFER_BLOCKS 4
#define ADC_BUFFER_TOTAL_SIZE(ch_count, block_size, block_num) \
    (uint32_t)((ch_count) * (block_size) * (block_num) * sizeof(uint16_t))

static xhal_adc_t adc;
static const xhal_adc_config_t adc_config = XADC_CONFIG_DEFAULT;

static char adc_buf[ADC_BUFFER_TOTAL_SIZE(1, 8, ADC_BUFFER_BLOCKS)];

extern const xhal_adc_ops_t adc_ops_driver;

static void adc_driver(void)
{
    xadc_inst(&adc, "adc", &adc_ops_driver, "ADC1", &adc_config, XADC_CHANNEL_8,
              adc_buf, sizeof(adc_buf));
}
INIT_EXPORT(adc_driver, EXPORT_LEVEL_PERIPH);
