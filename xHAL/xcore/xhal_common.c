#include "xhal_common.h"

const char xhal_logo[] = "   _  __ __  _____    __ \r\n"
                         "  | |/ // / / /   |  / / \r\n"
                         "  |   // /_/ / /| | / /  \r\n"
                         " /   |/ __  / ___ |/ /___\r\n"
                         "/_/|_/_/ /_/_/  |_/_____/\r\n";

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