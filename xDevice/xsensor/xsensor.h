#ifndef __XSENSOR_H
#define __XSENSOR_H

#include "xhal_coro.h"
#include "xhal_def.h"
#include "xhal_os.h"
#include "xhal_ringbuf.h"

#define XSENSOR_EVENT_QUEUE_SIZE (sizeof(xsensor_event_t) * 3 + 1)

typedef enum
{
    XSENSOR_RESET,
    XSENSOR_READ,
} xsensor_event_type_t;

typedef struct xsensor xsensor_t;
typedef struct xsensor_event xsensor_event_t;

typedef void (*xsensor_cb_t)(xsensor_event_t *event, void *data,
                             xhal_err_t err);

typedef struct xsensor_event
{
    xsensor_event_type_t type;
    uint32_t timeout_ms;
    xsensor_cb_t cb;
} xsensor_event_t;

typedef struct xsensor_ops
{
    xhal_err_t (*init)(void *inst);
    xhal_err_t (*deinit)(void *inst);

    void (*reset)(xcoro_handle_t *handle, void *inst, xsensor_event_t *event);
    void (*read)(xcoro_handle_t *handle, void *inst, xsensor_event_t *event);
} xsensor_ops_t;

typedef struct xsensor
{
    xrbuf_t evt_rb;
    uint8_t evt_buff[XSENSOR_EVENT_QUEUE_SIZE];
    xcoro_event_t event;
    void *inst;
    const xsensor_ops_t *ops;

#ifdef XHAL_OS_SUPPORTING
    osMutexId_t mutex;
#endif
} xsensor_t;

xhal_err_t xsensor_init(xsensor_t *sensor, const xsensor_ops_t *ops,
                        void *inst);
xhal_err_t xsensor_deinit(xsensor_t *sensor);

xhal_err_t xsensor_reset(xsensor_t *sensor, xsensor_cb_t cb,
                         uint32_t timeout_ms);
xhal_err_t xsensor_read(xsensor_t *sensor, xsensor_cb_t cb,
                        uint32_t timeout_ms);

void xsensor_handler_thread(xcoro_handle_t *handle, xsensor_t *sensor);

#endif /* __XSENSOR_H */
