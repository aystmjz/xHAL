#ifndef __XHAL_COMMON_H
#define __XHAL_COMMON_H

#include "xhal_config.h"
#include "xhal_def.h"

#define XHAL_VERSION_MAJOR 1
#define XHAL_VERSION_MINOR 2
#define XHAL_VERSION_PATCH 0
#define XHAL_VERSION_HEX                                      \
    ((XHAL_VERSION_MAJOR << 16) | (XHAL_VERSION_MINOR << 8) | \
     (XHAL_VERSION_PATCH))

#define XHAL_VERSION_STR "1.2.0"

uint32_t xhal_version(void);
const char *xhal_version_str(void);

const char *xhal_err_to_str(xhal_err_t err);

#endif /* __XHAL_COMMON_H */
