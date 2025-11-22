#ifndef __DRV_UTIL_H
#define __DRV_UTIL_H

#include "xhal_assert.h"
#include "xhal_log.h"
#include "xhal_time.h"
#include XHAL_CMSIS_DEVICE_HEADER

void _gpio_clock_enable(const char *name);
bool _check_pin_name_valid(const char *name);
GPIO_TypeDef *_get_port_from_name(const char *name);
uint16_t _get_pin_from_name(const char *name);

#endif /* __DRV_UTIL_H */
