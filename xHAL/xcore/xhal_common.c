#include "xhal_common.h"

uint32_t xhal_version(void)
{
    return XHAL_VERSION_HEX;
}
const char *xhal_version_str(void)
{
    return XHAL_VERSION_STR;
}

const char *xhal_err_to_str(xhal_err_t err)
{
    switch (err)
    {
#define ERR(code, value, str) \
    case code:                \
        return str " (" #value ")";
        XHAL_ERR_LIST
#undef X
    default:
        return "Unknown error";
    }
}