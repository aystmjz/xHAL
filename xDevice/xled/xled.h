#ifndef __XLED_H
#define __XLED_H

#include "xhal_def.h"
#include "xhal_os.h"

typedef enum
{
    XLED_STATE_OFF = 0, /* LED关闭 */
    XLED_STATE_ON       /* LED打开 */
} xled_state_t;

typedef struct xled xled_t;

typedef struct xled_ops
{
    xhal_err_t (*set_state)(xled_t *self, xled_state_t state);
    xhal_err_t (*get_state)(xled_t *self, xled_state_t *state);
    xhal_err_t (*toggle)(xled_t *self);
} xled_ops_t;

typedef struct xled
{
    xled_state_t state;
    const xled_ops_t *ops;

#ifdef XHAL_OS_SUPPORTING
    osMutexId_t mutex;
#endif
} xled_t;

xhal_err_t xled_init(xled_t *led, const xled_ops_t *ops, xled_state_t state);
xhal_err_t xled_deinit(xled_t *led);

xhal_err_t xled_on(xled_t *led);
xhal_err_t xled_off(xled_t *led);
xhal_err_t xled_toggle(xled_t *led);
xhal_err_t xled_set_state(xled_t *led, xled_state_t state);
xhal_err_t xled_get_state(xled_t *led, xled_state_t *state);

#endif /* __XLED_H */
