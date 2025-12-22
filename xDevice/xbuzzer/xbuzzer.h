#ifndef __XBUZZER_H
#define __XBUZZER_H

#include "xhal_def.h"
#include "xhal_os.h"

typedef enum
{
    XBUZZER_STATE_OFF = 0, /* 蜂鸣器关闭 */
    XBUZZER_STATE_ON       /* 蜂鸣器打开 */
} xbuzzer_state_t;

typedef struct xbuzzer xbuzzer_t;

typedef struct xbuzzer_ops
{
    xhal_err_t (*set_state)(xbuzzer_t *self, xbuzzer_state_t state);
    xhal_err_t (*set_frequency)(xbuzzer_t *self, uint16_t frequency_hz);
    xhal_err_t (*set_duty_cycle)(xbuzzer_t *self, uint16_t duty_percent);
} xbuzzer_ops_t;

typedef struct xbuzzer
{
    xbuzzer_state_t state;
    uint16_t frequency_hz;
    uint16_t duty_cycle_percent;

    const xbuzzer_ops_t *ops;

#ifdef XHAL_OS_SUPPORTING
    osMutexId_t mutex;
#endif
} xbuzzer_t;

xhal_err_t xbuzzer_init(xbuzzer_t *buzzer, const xbuzzer_ops_t *ops,
                        xbuzzer_state_t state, uint16_t frequency_hz,
                        uint16_t duty_percent);
xhal_err_t xbuzzer_deinit(xbuzzer_t *buzzer);

xhal_err_t xbuzzer_on(xbuzzer_t *buzzer);
xhal_err_t xbuzzer_off(xbuzzer_t *buzzer);
xhal_err_t xbuzzer_set_state(xbuzzer_t *buzzer, xbuzzer_state_t state);
xhal_err_t xbuzzer_get_state(xbuzzer_t *buzzer, xbuzzer_state_t *state);

xhal_err_t xbuzzer_set_frequency(xbuzzer_t *buzzer, uint16_t frequency_hz);
xhal_err_t xbuzzer_get_frequency(xbuzzer_t *buzzer, uint16_t *frequency_hz);
xhal_err_t xbuzzer_set_duty_cycle(xbuzzer_t *buzzer, uint16_t duty_percent);
xhal_err_t xbuzzer_get_duty_cycle(xbuzzer_t *buzzer, uint16_t *duty_percent);

#endif /* __XBUZZER_H */
