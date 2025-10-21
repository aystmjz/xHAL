#ifndef __XHAL_COMMON_H
#define __XHAL_COMMON_H

#include "xhal_config.h"
#include "xhal_def.h"

#define XHAL_VALID_RAM_START   (0x20000000U)
#define XHAL_VALID_RAM_END     (0x20004FFFU)
#define XHAL_VALID_FLASH_START (0x08000000U)
#define XHAL_VALID_FLASH_END   (0x08020000U)

#define XHAL_IS_VALID_RAM_ADDRESS(addr)                  \
    (((xhal_pointer_t)(addr) >= XHAL_VALID_RAM_START) && \
     ((xhal_pointer_t)(addr) <= XHAL_VALID_RAM_END))

#define XHAL_IS_VALID_FLASH_ADDRESS(addr)                  \
    (((xhal_pointer_t)(addr) >= XHAL_VALID_FLASH_START) && \
     ((xhal_pointer_t)(addr) <= XHAL_VALID_FLASH_END))

#define XHAL_VERSION_MAJOR 1
#define XHAL_VERSION_MINOR 2
#define XHAL_VERSION_PATCH 0
#define XHAL_VERSION_HEX                                      \
    ((XHAL_VERSION_MAJOR << 16) | (XHAL_VERSION_MINOR << 8) | \
     (XHAL_VERSION_PATCH))

#define XHAL_VERSION_STR "1.3.2"

extern const char xhal_logo[];

uint32_t xhal_version(void);
const char *xhal_version_str(void);

const char *xhal_err_to_str(xhal_err_t err);

#endif /* __XHAL_COMMON_H */
