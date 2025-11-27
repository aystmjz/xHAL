#ifndef __XHAL_EXIT_H
#define __XHAL_EXIT_H

#include "xhal_periph.h"

typedef enum xhal_exit_mode
{
    XEXIT_MODE_INTERRUPT = 0, /* 中断模式 */
    XEXIT_MODE_EVENT,         /* 事件模式 */
} xhal_exit_mode_t;

typedef enum xhal_exit_trigger
{
    XEXIT_TRIGGER_RISING = 0, /* 上升沿唤醒 */
    XEXIT_TRIGGER_FALLING,    /* 下降沿唤醒 */
    XEXIT_TRIGGER_BOTH,       /* 上升沿和下降沿唤醒 */
} xhal_exit_trigger_t;

typedef void (*xhal_exit_cb_t)(void);

typedef struct xhal_exit xhal_exit_t;

typedef struct xhal_exit_config
{
    uint8_t trigger;
    uint8_t mode;
} xhal_exit_config_t;

typedef struct xhal_exit_ops
{
    xhal_err_t (*init)(xhal_exit_t *self);
    xhal_err_t (*config)(xhal_exit_t *self, const xhal_exit_config_t *config);
    xhal_err_t (*enable_irq)(xhal_exit_t *self);
    xhal_err_t (*disable_irq)(xhal_exit_t *self);
    xhal_err_t (*set_irq_callback)(xhal_exit_t *self, xhal_exit_cb_t cb);
} xhal_exit_ops_t;

typedef struct xhal_exit_data
{
    xhal_exit_config_t config;
    xhal_exit_cb_t irq_callback;
    const char *name;
} xhal_exit_data_t;

typedef struct xhal_exit
{
    xhal_periph_t peri;
    const xhal_exit_ops_t *ops;
    xhal_exit_data_t data;
} xhal_exit_t;

#define XEXIT_CAST(_dev) ((xhal_exit_t *)_dev)

xhal_err_t xexit_inst(xhal_exit_t *self, const char *name,
                      const xhal_exit_ops_t *ops, const char *exit_name,
                      const xhal_exit_config_t *config);

xhal_err_t xexit_set_irq_callback(xhal_periph_t *self, xhal_exit_cb_t cb);
xhal_err_t xexit_enable_irq(xhal_periph_t *self);
xhal_err_t xexit_disable_irq(xhal_periph_t *self);

xhal_err_t xexit_set_config(xhal_periph_t *self, xhal_exit_config_t *config);
xhal_err_t xexit_get_config(xhal_periph_t *self, xhal_exit_config_t *config);

#endif /* __XHAL_EXIT_H */
