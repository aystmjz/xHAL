#include "xhal_config.h"
#include "xhal_export.h"
#include "xhal_trace.h"

static void backtrace_init(void)
{
    cm_backtrace_init(XTRACE_FIRMWARE_NAME, XTRACE_HARDWARE_VERSION,
                      XTRACE_SOFTWARE_VERSION);
}
INIT_EXPORT(backtrace_init, EXPORT_LEVEL_CORE);
