#include "xhal_export.h"
#include "xhal_spi.h"

extern const xhal_spi_ops_t spi_hw_ops_driver;

static xhal_spi_t spi_w25q128;
static const xhal_spi_config_t spi_config = XSPI_CONFIG_DEFAULT;

static void driver_spi(void)
{
    xspi_inst(&spi_w25q128, "spi_w25q128", &spi_hw_ops_driver, "SPI2", NULL,
              NULL, NULL, &spi_config);
}
INIT_EXPORT(driver_spi, EXPORT_LEVEL_PERIPH);
