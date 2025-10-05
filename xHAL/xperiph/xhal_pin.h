#ifndef __XHAL_PIN_H
#define __XHAL_PIN_H

#include "xhal_periph.h"

typedef struct xhal_pin xhal_pin_t;

typedef enum pin_mode
{
    XPIN_MODE_INPUT = 0,
    XPIN_MODE_INPUT_PULLUP,
    XPIN_MODE_INPUT_PULLDOWN,
    XPIN_MODE_OUTPUT_PP,
    XPIN_MODE_OUTPUT_OD,

    XPIN_MODE_MAX
} pin_mode_t;

typedef enum pin_state
{
    XPIN_RESET = 0,
    XPIN_SET
} pin_state_t;

typedef struct xhal_pin_ops
{
    xhal_err_t (*init)(xhal_pin_t *const self);
    xhal_err_t (*set_mode)(xhal_pin_t *const self, pin_mode_t mode);
    xhal_err_t (*get_status)(xhal_pin_t *const self, pin_state_t *status);
    xhal_err_t (*set_status)(xhal_pin_t *const self, pin_state_t status);
} xhal_pin_ops_t;

typedef struct xhal_pin_data
{
    pin_mode_t mode;
    pin_state_t status;
    const char *name;
} xhal_pin_data_t;

typedef struct xhal_pin
{
    xhal_periph_t peri;

    const xhal_pin_ops_t *ops;
    xhal_pin_data_t data;
} xhal_pin_t;

#define XPIN_CAST(_peri) ((xhal_pin_t *)_peri)

xhal_err_t xpin_inst(xhal_pin_t *const self, const char *name,
                     const xhal_pin_ops_t *ops, const char *pin_name,
                     pin_mode_t mode);

xhal_err_t xpin_set_mode(xhal_periph_t *const self, pin_mode_t mode);
pin_state_t xpin_get_status(xhal_periph_t *const self);
xhal_err_t xpin_set_status(xhal_periph_t *const self, pin_state_t status);

#endif /* __XHAL_PIN_H */
