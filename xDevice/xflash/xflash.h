#ifndef __XFLASH_H
#define __XFLASH_H

#include "xhal_coro.h"
#include "xhal_def.h"
#include "xhal_os.h"
#include "xhal_ringbuf.h"

#define XFLASH_EVENT_QUEUE_SIZE (sizeof(xflash_event_t) * 3 + 1)

typedef enum
{
    XFLASH_ERASE_SECTOR = 0,
    XFLASH_ERASE_BLOCK,
    XFLASH_ERASE_CHIP,
} xflash_erase_type_t;

typedef struct xflash xflash_t;
typedef struct xflash_event xflash_event_t;

typedef void (*xflash_cb_t)(xflash_event_t *event, xhal_err_t err);

typedef struct xflash_event
{
    uint32_t type;
    uint32_t address;
    uint32_t timeout_ms;
    xflash_cb_t cb;
} xflash_event_t;

typedef struct xflash_ops
{
    xhal_err_t (*init)(void *inst);
    xhal_err_t (*deinit)(void *inst);

    xhal_err_t (*read_id)(void *inst, uint8_t *manufacturer_id,
                          uint16_t *device_id, uint32_t timeout_ms);
    xhal_err_t (*read)(void *inst, uint32_t address, uint8_t *buffer,
                       uint32_t size, uint32_t timeout_ms);
    xhal_err_t (*write)(void *inst, uint32_t address, const uint8_t *data,
                        uint32_t size, uint32_t timeout_ms);

    void (*erase)(xcoro_handle_t *handle, void *inst, xflash_event_t *event);
} xflash_ops_t;

typedef struct xflash
{
    xrbuf_t evt_rb;
    uint8_t evt_buff[XFLASH_EVENT_QUEUE_SIZE];
    xcoro_event_t event;
    void *inst;
    const xflash_ops_t *ops;

#ifdef XHAL_OS_SUPPORTING
    osMutexId_t mutex;
#endif
} xflash_t;

xhal_err_t xflash_init(xflash_t *flash, const xflash_ops_t *ops, void *inst);
xhal_err_t xflash_deinit(xflash_t *flash);

xhal_err_t xflash_read_id(xflash_t *flash, uint8_t *manufacturer_id,
                          uint16_t *device_id, uint32_t timeout_ms);
xhal_err_t xflash_read(xflash_t *flash, uint32_t address, uint8_t *buffer,
                       uint32_t size, uint32_t timeout_ms);
xhal_err_t xflash_write(xflash_t *flash, uint32_t address, const uint8_t *data,
                        uint32_t size, uint32_t timeout_ms);

xhal_err_t xflash_erase(xflash_t *flash, xflash_erase_type_t type,
                        uint32_t address, xflash_cb_t cb, uint32_t timeout_ms);

void xflash_handler_thread(xcoro_handle_t *handle, xflash_t *flash);

#endif /* __XFLASH_H */
