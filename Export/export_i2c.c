#include "xhal_export.h"
#include "xhal_i2c.h"

extern const xhal_i2c_ops_t i2c_ops_driver;

static xhal_i2c_t i2c_ds3231;
static xhal_i2c_t i2c_sht30;
static xhal_i2c_config_t i2c_config = XI2C_CONFIG_DEFAULT;

static void driver_i2c(void)
{
    xi2c_inst(&i2c_ds3231, "i2c_ds3231", &i2c_ops_driver, "i2c_1", "PA11",
              "PA12", &i2c_config);
    xi2c_inst(&i2c_sht30, "i2c_sht30", &i2c_ops_driver, "i2c_2", "PC15",
              "PC14",&i2c_config);
}
INIT_EXPORT(driver_i2c, EXPORT_LEVEL_PERIPH);
