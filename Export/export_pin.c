#include "xhal_export.h"
#include "xhal_pin.h"

extern const xhal_pin_ops_t pin_ops_driver;

static xhal_pin_t pin_led;
static xhal_pin_t pin_w25q128;
static void driver_pin(void)
{
    xpin_inst(&pin_led, "pin_led", &pin_ops_driver, "PA1", XPIN_MODE_OUTPUT_PP,
              XPIN_LOW);

    xpin_inst(&pin_w25q128, "w25q128_cs", &pin_ops_driver, "PB12",
              XPIN_MODE_OUTPUT_PP, XPIN_HIGH);
}
INIT_EXPORT(driver_pin, EXPORT_LEVEL_PERIPH);
