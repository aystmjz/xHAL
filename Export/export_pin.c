#include "xhal_export.h"
#include "xhal_pin.h"

extern const xhal_pin_ops_t pin_ops_driver;

static xhal_pin_t pin_led;

static void driver_pin(void)
{
    xpin_inst(&pin_led, "pin_led", &pin_ops_driver, "PA1", XPIN_MODE_OUTPUT_PP,
              XPIN_LOW);
}
INIT_EXPORT(driver_pin, EXPORT_LEVEL_PERIPH);
