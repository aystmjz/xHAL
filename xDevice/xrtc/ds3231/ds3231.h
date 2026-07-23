#ifndef __DS3231_H
#define __DS3231_H

#include "../xrtc.h"
#include "xhal_def.h"
#include "xhal_i2c.h"

#define DS3231_I2C_ADDR 0x68

#define ALARM1          0
#define ALARM2          1

typedef struct ds3231_bus_ops
{
    xhal_err_t (*read)(uint16_t addr, uint8_t *buff, uint16_t size,
                       uint32_t timeout_ms);
    xhal_err_t (*write)(uint16_t addr, uint8_t *buff, uint16_t len,
                        uint32_t timeout_ms);
} ds3231_bus_ops_t;

typedef struct ds3231_dev
{
    const ds3231_bus_ops_t *bus;
} ds3231_dev_t;

extern const xrtc_ops_t ds3231_ops;

#endif /* __DS3231_H */
