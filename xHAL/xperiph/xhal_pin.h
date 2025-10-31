#ifndef __XHAL_PIN_H
#define __XHAL_PIN_H

#include "xhal_periph.h"

typedef struct xhal_pin xhal_pin_t;

typedef enum xhal_pin_mode
{
    XPIN_MODE_INPUT = 0,
    XPIN_MODE_INPUT_PULLUP,
    XPIN_MODE_INPUT_PULLDOWN,
    XPIN_MODE_OUTPUT_PP,
    XPIN_MODE_OUTPUT_OD
} xhal_pin_mode_t;

typedef enum xhal_pin_state
{
    XPIN_LOW = 0,
    XPIN_HIGH
} xhal_pin_state_t;

typedef struct xhal_pin_ops
{
    xhal_err_t (*init)(xhal_pin_t *self, xhal_pin_state_t status);
    xhal_err_t (*set_mode)(xhal_pin_t *self, xhal_pin_mode_t mode);
    xhal_err_t (*read)(xhal_pin_t *self, xhal_pin_state_t *status);
    xhal_err_t (*write)(xhal_pin_t *self, xhal_pin_state_t status);
} xhal_pin_ops_t;

typedef struct xhal_pin_data
{
    xhal_pin_mode_t mode;
    xhal_pin_state_t status;
    const char *name;
} xhal_pin_data_t;

typedef struct xhal_pin
{
    xhal_periph_t peri;

    const xhal_pin_ops_t *ops;
    xhal_pin_data_t data;
} xhal_pin_t;

#define XPIN_CAST(_peri) ((xhal_pin_t *)_peri)

xhal_err_t xpin_inst(xhal_pin_t *self, const char *name,
                     const xhal_pin_ops_t *ops, const char *pin_name,
                     xhal_pin_mode_t mode, xhal_pin_state_t status);

xhal_err_t xpin_set_mode(xhal_periph_t *self, xhal_pin_mode_t mode);
xhal_pin_state_t xpin_read(xhal_periph_t *self);
xhal_err_t xpin_write(xhal_periph_t *self, xhal_pin_state_t status);
xhal_err_t xpin_toggle(xhal_periph_t *self);

#endif /* __XHAL_PIN_H */
