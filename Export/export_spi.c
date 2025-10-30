#include "xhal_export.h"
#include "xhal_spi.h"

extern const xhal_spi_ops_t spi_hw_ops_driver;
extern const xhal_spi_ops_t spi_soft_ops_driver;

static xhal_spi_t spi_w25q128;
static xhal_spi_t spi_oled;
static const xhal_spi_config_t spi_default_config = XSPI_CONFIG_DEFAULT;
static const xhal_spi_config_t spi_oled_config    = {
    XSPI_MODE_0,
    XSPI_DIR_1LINE_TX,
    XSPI_DATA_BITS_8,
};

static void driver_spi(void)
{
    xspi_inst(&spi_w25q128, "spi_w25q128", &spi_hw_ops_driver, "SPI2", "PB13",
              "PB15", "PB14", &spi_default_config);
    xspi_inst(&spi_oled, "spi_oled", &spi_soft_ops_driver, "SPI_SOFT", "PA5",
              "PA7", NULL, &spi_oled_config);
}
INIT_EXPORT(driver_spi, EXPORT_LEVEL_PERIPH);
