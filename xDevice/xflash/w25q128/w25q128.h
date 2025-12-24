#ifndef __W25Q128_H
#define __W25Q128_H

#include "../xflash.h"
#include "xhal_def.h"

typedef struct w25q128_bus_ops
{
    xhal_err_t (*transfer)(const uint8_t *tx, uint8_t *rx, uint32_t len,
                           uint32_t timeout_ms);

    xhal_err_t (*cs_select)(void);
    xhal_err_t (*cs_deselect)(void);
} w25q128_bus_ops_t;

typedef struct w25q128_dev
{
    const w25q128_bus_ops_t *bus;
} w25q128_dev_t;

extern const xflash_ops_t w25q128_ops;

#endif /* __W25Q128_H */
