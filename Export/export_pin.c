#include "xhal_export.h"
#include "xhal_pin.h"

extern const xhal_pin_ops_t pin_ops_driver;

static xhal_pin_t pin_led;
static xhal_pin_t pin_key;

static void driver_pin(void) {
  xpin_inst(&pin_led, "pin_led", &pin_ops_driver, "PA1", XPIN_MODE_OUTPUT_PP);
  xpin_inst(&pin_key, "pin_key", &pin_ops_driver, "PB1", XPIN_MODE_INPUT);
}
INIT_EXPORT(driver_pin, EXPORT_LEVEL_PERIPH);
