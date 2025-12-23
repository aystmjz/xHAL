#ifndef __XBATTERY_H
#define __XBATTERY_H

#include "xhal_def.h"
#include "xhal_os.h"

#define XBATTERY_VOLTAGE_BUFFER_SIZE 5

typedef enum
{
    XBATTERY_STATE_NORMAL   = 0, /* 正常状态 */
    XBATTERY_STATE_CHARGING = 1, /* 充电中 */
    XBATTERY_STATE_EMPTY    = 2, /* 空电状态 */
    XBATTERY_STATE_POWER    = 3  /* 外部电源供电 */
} xbattery_state_t;

typedef struct xbattery_config
{
    uint16_t full_voltage_mv;  /* 满电电压(mV) */
    uint16_t empty_voltage_mv; /* 空电电压(mV) */
    uint16_t charge_threshold; /* 充电检测阈值 */
    uint16_t power_threshold;  /* 外部电源供电检测阈值 */
} xbattery_config_t;

typedef struct xbattery
{
    xbattery_state_t state;
    xbattery_config_t config;
    uint16_t voltage_mv[XBATTERY_VOLTAGE_BUFFER_SIZE];
    uint8_t percentage;
    uint8_t is_init;

#ifdef XHAL_OS_SUPPORTING
    osMutexId_t mutex;
#endif
} xbattery_t;

xhal_err_t xbattery_init(xbattery_t *battery, xbattery_config_t *config);
xhal_err_t xbattery_deinit(xbattery_t *battery);

xhal_err_t xbattery_update(xbattery_t *battery, uint16_t voltage_mv);

xhal_err_t xbattery_get_state(xbattery_t *battery, xbattery_state_t *state);
xhal_err_t xbattery_get_percentage(xbattery_t *battery, uint8_t *percentage);
xhal_err_t xbattery_get_voltage(xbattery_t *battery, uint16_t *voltage_mv);

xhal_err_t xbattery_set_config(xbattery_t *battery,
                               const xbattery_config_t *config);
xhal_err_t xbattery_get_config(xbattery_t *battery, xbattery_config_t *config);

#endif /* __XBATTERY_H */
