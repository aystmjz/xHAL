#include "xhal_common.h"

uint32_t xhal_version(void)
{
    return XHAL_VERSION_HEX;
}
const char *xhal_version_str(void)
{
    return XHAL_VERSION_STR;
}