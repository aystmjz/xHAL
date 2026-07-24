#ifndef __XHAL_CRC_H
#define __XHAL_CRC_H

#include "../xcore/xhal_std.h"

#define XCRC32_INIT   (0U)
#define XCRC32_XOROUT (0xFFFFFFFFU)

#define XCRC8_INIT    (0xFFU)
#define XCRC8_POLY    (0x31U)
#define XCRC8_XOROUT  (0x00U)

uint32_t xcrc32(uint32_t crc, const void *data, uint32_t size);
uint8_t xcrc8(uint8_t crc, const void *data, uint32_t size);

#endif /* __XHAL_CRC_H */
