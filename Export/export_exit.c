#include "xhal_exit.h"
#include "xhal_export.h"

extern const xhal_exit_ops_t exit_ops_driver;

static xhal_exit_t exit_key;
static const xhal_exit_config_t exit_key_config = {
    .mode    = XEXIT_MODE_EVENT,
    .trigger = XEXIT_TRIGGER_FALLING,
};

static void driver_exit(void)
{
    xexit_inst(&exit_key, "exit_key", &exit_ops_driver, "PB5",
               &exit_key_config);
}
INIT_EXPORT(driver_exit, EXPORT_LEVEL_PERIPH);
