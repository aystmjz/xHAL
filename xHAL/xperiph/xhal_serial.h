#ifndef __XHAL_SERIAL_H
#define __XHAL_SERIAL_H

#include "../xlib/xhal_ringbuf.h"
#include "xhal_periph.h"

typedef struct xhal_serial xhal_serial_t;

#define XSERIAL_EVENT_CAN_READ      (1U << 0) /* 有数据可读 */
#define XSERIAL_EVENT_CAN_WRITE     (1U << 1) /* 数据发送完成，可继续写入 */

#define XSERIAL_TERM_SCANF_BUF_SIZE 128
#define XSERIAL_PRINTF_BUF_SIZE     128

#define XSERIAL_ATTR_DEFAULT                     \
    {                                            \
        115200,              /* 115200 bits/s */ \
        XSERIAL_DATA_BITS_8, /* 8 databits */    \
        XSERIAL_STOP_BITS_1, /* 1 stopbit */     \
        XSERIAL_PARITY_NONE, /* No parity  */    \
        0,                                       \
    }

enum xhal_serial_data_bits
{
    XSERIAL_DATA_BITS_8 = 8,
    XSERIAL_DATA_BITS_9,
};

enum xhal_serail_stop_bits
{
    XSERIAL_STOP_BITS_1 = 0,
    XSERIAL_STOP_BITS_2,
};

enum xhal_serial_parity
{
    XSERIAL_PARITY_NONE = 0,
    XSERIAL_PARITY_ODD,
    XSERIAL_PARITY_EVEN,
};

typedef struct xhal_serial_attr
{
    uint32_t baud_rate;
    uint16_t data_bits : 4;
    uint16_t stop_bits : 2;
    uint16_t parity : 2;
    uint16_t reserved : 8;
} xhal_serial_attr_t;

typedef struct
{
    uint32_t rx_full; /* 已接收但未读取的字节数 */
    uint32_t rx_free; /* 接收缓冲剩余空间 */
    uint32_t tx_full; /* 发送缓冲中未发送的字节数 */
    uint32_t tx_free; /* 发送缓冲剩余空间 */
} xserial_status_t;

typedef struct xhal_serial_ops
{
    xhal_err_t (*init)(xhal_serial_t *self);
    xhal_err_t (*set_attr)(xhal_serial_t *self, const xhal_serial_attr_t *attr);
    uint32_t (*transmit)(xhal_serial_t *self, const void *buff, uint32_t size);
} xhal_serial_ops_t;

typedef struct xhal_serial_data
{
    xhal_serial_attr_t attr;

    xrbuf_t tx_rbuf;
    xrbuf_t rx_rbuf;

#ifdef XHAL_OS_SUPPORTING
    uint32_t rx_expect;
    osMutexId_t tx_mutex;
    osMutexId_t rx_mutex;
    osEventFlagsId_t event_flag;
#endif
    const char *name;
} xhal_serial_data_t;

typedef struct xhal_serial
{
    xhal_periph_t peri;

    const xhal_serial_ops_t *ops;
    xhal_serial_data_t data;
} xhal_serial_t;

#define XHAL_SERIAL_CAST(_dev) ((xhal_serial_t *)_dev)

xhal_err_t xserial_inst(xhal_serial_t *self, const char *name,
                        const xhal_serial_ops_t *ops, const char *serial_name,
                        const xhal_serial_attr_t *attr, void *tx_buff,
                        void *rx_buff, uint32_t tx_bufsz, uint32_t rx_bufsz);

uint32_t xserial_printf(xhal_periph_t *self, const char *fmt, ...);
uint32_t xserial_write(xhal_periph_t *self, const void *data, uint32_t size);
uint32_t xserial_write_timeout(xhal_periph_t *self, const void *data,
                               uint32_t size, uint32_t timeout_ms);

uint32_t xserial_scanf(xhal_periph_t *self, const char *fmt, ...);
uint32_t xserial_read(xhal_periph_t *self, void *buff, uint32_t size);
uint32_t xserial_read_timeout(xhal_periph_t *self, void *buf, uint32_t size,
                              uint32_t timeout_ms);

uint32_t xserial_peek(xhal_periph_t *self, void *buff, uint32_t offset,
                      uint32_t size);
uint32_t xserial_discard(xhal_periph_t *self, uint32_t size);
uint8_t xserial_find(xhal_periph_t *self, const void *data, uint32_t size,
                     uint32_t offset, uint32_t *index);
xhal_err_t xserial_clear(xhal_periph_t *self);

uint32_t xserial_term_scanf(xhal_periph_t *self, const char *fmt, ...);

xhal_err_t xserial_get_status(xhal_periph_t *self, xserial_status_t *status);
xhal_err_t xserial_get_attr(xhal_periph_t *self, xhal_serial_attr_t *attr);
xhal_err_t xserial_set_attr(xhal_periph_t *self, xhal_serial_attr_t *attr);
xhal_err_t xserial_set_baudrate(xhal_periph_t *self, uint32_t baudrate);

#endif /* __XHAL_SERIAL_H */
