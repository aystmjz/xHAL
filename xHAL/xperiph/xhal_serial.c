#include "xhal_serial.h"
#include "../xcore/xhal_assert.h"
#include "../xcore/xhal_log.h"
#include "../xcore/xhal_malloc.h"
#include "../xcore/xhal_time.h"
#include <stdarg.h>
#include <stdio.h>

#ifdef XHAL_OS_SUPPORTING
static const osMutexAttr_t xserial_mutex_attr = {
    .name      = "xserial_mutex",
    .attr_bits = osMutexPrioInherit | osMutexRecursive,
    .cb_mem    = NULL,
    .cb_size   = 0,
};
static const osEventFlagsAttr_t xserial_event_flag_attr = {
    .name      = "xserial_event_flag",
    .attr_bits = 0,
    .cb_mem    = NULL,
    .cb_size   = 0,
};
#endif

XLOG_TAG("xSerial");

xhal_err_t xserial_inst(xhal_serial_t *self, const char *name,
                        const xhal_serial_ops_t *ops, const char *serial_name,
                        const xhal_serial_config_t *config, void *tx_buff,
                        void *rx_buff, uint32_t tx_bufsz, uint32_t rx_bufsz)
{
    xassert_not_null(self);
    xassert_not_null(name);
    xassert_not_null(serial_name);
    xassert_not_null(config);
    xassert_ptr_struct_not_null(ops, name);

    xhal_serial_t *serial            = self;
    xhal_periph_attr_t periph_config = {
        .name = name,
        .type = XHAL_PERIPH_UART,
    };
    xperiph_register(&serial->peri, &periph_config);

    serial->ops         = ops;
    serial->data.config = *config;
    serial->data.name   = serial_name;

    xrbuf_init(&serial->data.tx_rbuf, tx_buff, tx_bufsz);
    xrbuf_init(&serial->data.rx_rbuf, rx_buff, rx_bufsz);

#ifdef XHAL_OS_SUPPORTING
    serial->data.rx_expect = 1;
    serial->data.tx_mutex  = osMutexNew(&xserial_mutex_attr);
    xassert_not_null(serial->data.tx_mutex);
    serial->data.rx_mutex = osMutexNew(&xserial_mutex_attr);
    xassert_not_null(serial->data.rx_mutex);
    serial->data.event_flag = osEventFlagsNew(&xserial_event_flag_attr);
    xassert_not_null(serial->data.event_flag);
#endif
    xhal_err_t ret = serial->ops->init(serial);
    if (ret != XHAL_OK)
    {
        xperiph_unregister(&serial->peri);

#ifdef XHAL_OS_SUPPORTING
        osEventFlagsDelete(serial->data.event_flag);
        osMutexDelete(serial->data.tx_mutex);
        osMutexDelete(serial->data.rx_mutex);
#endif
        return ret;
    }

    serial->peri.is_inited = XPERIPH_INITED;

    return XHAL_OK;
}

uint32_t xserial_write_timeout(xhal_periph_t *self, const void *data,
                               uint32_t size, uint32_t timeout_ms)
{
    xassert_not_null(self);
    xassert_not_null(data);
    XPERIPH_CHECK_INIT(self, 0);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_UART);

    if (size == 0)
        return 0;

    xhal_serial_t *serial        = XHAL_SERIAL_CAST(self);
    xhal_tick_ms_t start_tick_ms = xtime_get_tick_ms();
    uint32_t written             = 0;

    xperiph_lock(self);
    while (1)
    {
        uint32_t w = serial->ops->transmit(
            serial, (const uint8_t *)data + written, size - written);
        written += w;
        if (written >= size)
            break;

        uint32_t elapsed_ms = xtime_get_tick_ms() - start_tick_ms;

        if (elapsed_ms >= timeout_ms)
            break;

#ifdef XHAL_OS_SUPPORTING
        uint32_t wait_ms = timeout_ms - elapsed_ms;
        osEventFlagsWait(serial->data.event_flag, XSERIAL_EVENT_CAN_WRITE,
                         osFlagsWaitAny, XOS_MS_TO_TICKS(wait_ms));
#endif
    }

#ifdef XHAL_OS_SUPPORTING
    ret_os = osMutexRelease(serial->data.tx_mutex);
    xassert(ret_os == osOK);
#endif
    return written;
}

uint32_t xserial_write(xhal_periph_t *self, const void *data, uint32_t size)
{
    return xserial_write_timeout(self, data, size, XHAL_WAIT_FOREVER);
}

uint32_t xserial_read_timeout(xhal_periph_t *self, void *buf, uint32_t size,
                              uint32_t timeout_ms)
{
    xassert_not_null(self);
    xassert_not_null(buf);
    XPERIPH_CHECK_INIT(self, 0);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_UART);
    if (size == 0)
        return 0;

    xhal_serial_t *serial        = XHAL_SERIAL_CAST(self);
    xhal_tick_ms_t start_tick_ms = xtime_get_tick_ms();
    uint32_t read                = 0;

#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret_os = osOK;
    ret_os            = osMutexAcquire(serial->data.rx_mutex, osWaitForever);
    xassert(ret_os == osOK);
    serial->data.rx_expect = size;
#endif
    while (1)
    {
        uint32_t r = xrbuf_read(&serial->data.rx_rbuf, (uint8_t *)buf + read,
                                size - read);
        read += r;
        if (read >= size)
            break;

        uint32_t elapsed_ms = xtime_get_tick_ms() - start_tick_ms;
        if (elapsed_ms >= timeout_ms)
            break;

#ifdef XHAL_OS_SUPPORTING
        uint32_t wait_ms = timeout_ms - elapsed_ms;
        osEventFlagsWait(serial->data.event_flag, XSERIAL_EVENT_CAN_READ,
                         osFlagsWaitAny, XOS_MS_TO_TICKS(wait_ms));
#endif
    }

#ifdef XHAL_OS_SUPPORTING
    ret_os = osMutexRelease(serial->data.rx_mutex);
    xassert(ret_os == osOK);
#endif

    return read;
}

uint32_t xserial_read(xhal_periph_t *self, void *buff, uint32_t size)
{
    return xserial_read_timeout(self, buff, size, XHAL_WAIT_FOREVER);
}

uint32_t xserial_peek(xhal_periph_t *self, void *buff, uint32_t offset,
                      uint32_t size)
{
    xassert_not_null(self);
    xassert_not_null(buff);
    XPERIPH_CHECK_INIT(self, 0);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_UART);
    if (size == 0)
        return 0;

    xhal_serial_t *serial = XHAL_SERIAL_CAST(self);
    uint32_t peeked       = 0;

#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret_os = osOK;
    ret_os            = osMutexAcquire(serial->data.rx_mutex, osWaitForever);
    xassert(ret_os == osOK);
#endif
    peeked = xrbuf_peek(&serial->data.rx_rbuf, offset, buff, size);

#ifdef XHAL_OS_SUPPORTING
    ret_os = osMutexRelease(serial->data.rx_mutex);
    xassert(ret_os == osOK);
#endif
    return peeked;
}

uint32_t xserial_discard(xhal_periph_t *self, uint32_t size)
{
    xassert_not_null(self);
    XPERIPH_CHECK_INIT(self, 0);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_UART);
    if (size == 0)
        return 0;

    xhal_serial_t *serial = XHAL_SERIAL_CAST(self);
    uint32_t skipped      = 0;

#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret_os = osOK;
    ret_os            = osMutexAcquire(serial->data.rx_mutex, osWaitForever);
    xassert(ret_os == osOK);
#endif
    skipped = xrbuf_skip(&serial->data.rx_rbuf, size);

#ifdef XHAL_OS_SUPPORTING
    ret_os = osMutexRelease(serial->data.rx_mutex);
    xassert(ret_os == osOK);
#endif
    return skipped;
}

uint8_t xserial_find(xhal_periph_t *self, const void *data, uint32_t size,
                     uint32_t offset, uint32_t *index)
{
    xassert_not_null(self);
    xassert_not_null(data);
    xassert_not_null(index);
    XPERIPH_CHECK_INIT(self, 0);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_UART);
    if (size == 0)
        return 0;

    xhal_serial_t *serial = XHAL_SERIAL_CAST(self);
    uint8_t found         = 0;

#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret_os = osOK;
    ret_os            = osMutexAcquire(serial->data.rx_mutex, osWaitForever);
    xassert(ret_os == osOK);
#endif
    found = xrbuf_find(&serial->data.rx_rbuf, data, size, offset, index);

#ifdef XHAL_OS_SUPPORTING
    ret_os = osMutexRelease(serial->data.rx_mutex);
    xassert(ret_os == osOK);
#endif
    return found;
}

xhal_err_t xserial_clear(xhal_periph_t *self)
{
    xassert_not_null(self);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_SYSTEM);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_UART);

    xhal_serial_t *serial = XHAL_SERIAL_CAST(self);

#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret_os = osOK;
    ret_os            = osMutexAcquire(serial->data.rx_mutex, osWaitForever);
    xassert(ret_os == osOK);
#endif
    uint32_t full = xrbuf_get_full(&serial->data.rx_rbuf);
    uint32_t len  = xrbuf_skip(&serial->data.rx_rbuf, full);
    xassert_name(len == full, self->attr.name);

#ifdef XHAL_OS_SUPPORTING
    ret_os = osMutexRelease(serial->data.rx_mutex);
    xassert(ret_os == osOK);
#endif
    return XHAL_OK;
}

uint32_t xserial_printf(xhal_periph_t *self, const char *fmt, ...)
{
    xassert_not_null(fmt);

    char stack_buf[XSERIAL_PRINTF_BUF_SIZE];

    va_list args;
    va_start(args, fmt);
    int32_t len = vsnprintf(stack_buf, sizeof(stack_buf), fmt, args);
    va_end(args);
    if (len < 0)
        return 0;

    if (len < sizeof(stack_buf))
    {
        return xserial_write_timeout(self, stack_buf, len, XHAL_WAIT_FOREVER);
    }

    char *heap_buf = (char *)xmalloc(len + 1);
    if (heap_buf == NULL)
        return 0;

    va_start(args, fmt);
    vsnprintf(heap_buf, len + 1, fmt, args);
    va_end(args);

    uint32_t written =
        xserial_write_timeout(self, heap_buf, len, XHAL_WAIT_FOREVER);

    xfree(heap_buf);

    return written;
}

uint32_t xserial_scanf(xhal_periph_t *self, const char *fmt, ...)
{
    xassert_not_null(self);
    xassert_not_null(fmt);
    XPERIPH_CHECK_INIT(self, 0);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_UART);

    xhal_serial_t *serial = XHAL_SERIAL_CAST(self);
    int32_t ret           = 0;

#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret_os = osOK;
    ret_os            = osMutexAcquire(serial->data.rx_mutex, osWaitForever);
    xassert(ret_os == osOK);
#endif
    char *buf     = NULL;
    uint32_t read = 0;
    uint32_t len  = xrbuf_get_full(&serial->data.rx_rbuf);
    if (len == 0)
        goto exit;

    buf = (char *)xmalloc(len + 1);
    if (buf == NULL)
        goto exit;

    read = xrbuf_read(&serial->data.rx_rbuf, buf, len);
    xassert_name(read == len, self->attr.name);

    buf[len] = '\0';

    va_list args;
    va_start(args, fmt);
    ret = vsscanf(buf, fmt, args);
    va_end(args);

    xfree(buf);

exit:

#ifdef XHAL_OS_SUPPORTING
    ret_os = osMutexRelease(serial->data.rx_mutex);
    xassert(ret_os == osOK);
#endif
    return (ret >= 0) ? (uint32_t)ret : 0;
}

uint32_t xserial_term_scanf(xhal_periph_t *self, const char *fmt, ...)
{
    char buf[XSERIAL_TERM_SCANF_BUF_SIZE];
    char ch;
    uint32_t len = 0;

    while (1)
    {
        /* 等待一个字节输入 */
        if (xserial_read(self, &ch, 1) == 1)
        {
            if (ch == '\r' || ch == '\n')
            {
                /* 回车：输入结束 */
                xserial_write(self, "\r\n", 2); /* 回显换行 */
                break;
            }
            else if ((ch == '\b' || ch == 127) && len > 0)
            {
                /* 退格键（8或127），删除一个字符 */
                len--;
                xserial_write(self, "\b \b", 3); /* 回显退格 */
            }
            else if (len < sizeof(buf) - 1)
            {
                /* 普通字符，存入缓冲区并回显 */
                buf[len++] = ch;
                xserial_write(self, &ch, 1);
            }
        }
    }
    buf[len] = '\0';

    va_list args;
    va_start(args, fmt);
    int32_t ret = vsscanf(buf, fmt, args);
    va_end(args);

    return (ret >= 0) ? (uint32_t)ret : 0;
}

xhal_err_t xserial_get_status(xhal_periph_t *self, xserial_status_t *status)
{
    xassert_not_null(self);
    xassert_not_null(status);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_SYSTEM);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_UART);

    xhal_serial_t *serial = XHAL_SERIAL_CAST(self);

#ifdef XHAL_OS_SUPPORTING
    osStatus_t ret_os = osOK;
    ret_os            = osMutexAcquire(serial->data.rx_mutex, osWaitForever);
    xassert(ret_os == osOK);
#endif
    status->rx_full = xrbuf_get_full(&serial->data.rx_rbuf);
    status->rx_free = xrbuf_get_free(&serial->data.rx_rbuf);

#ifdef XHAL_OS_SUPPORTING
    ret_os = osMutexRelease(serial->data.rx_mutex);
    xassert(ret_os == osOK);
    ret_os = osMutexAcquire(serial->data.tx_mutex, osWaitForever);
    xassert(ret_os == osOK);
#endif
    status->tx_full = xrbuf_get_full(&serial->data.tx_rbuf);
    status->tx_free = xrbuf_get_free(&serial->data.tx_rbuf);

#ifdef XHAL_OS_SUPPORTING
    ret_os = osMutexRelease(serial->data.tx_mutex);
    xassert(ret_os == osOK);
#endif
    return XHAL_OK;
}

xhal_err_t xserial_get_config(xhal_periph_t *self, xhal_serial_config_t *config)
{
    xassert_not_null(self);
    xassert_not_null(config);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_SYSTEM);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_UART);

    xhal_serial_t *serial = XHAL_SERIAL_CAST(self);

    xperiph_lock(self);
    *config = serial->data.config;
    xperiph_unlock(self);

    return XHAL_OK;
}

xhal_err_t xserial_set_config(xhal_periph_t *self, xhal_serial_config_t *config)
{
    xassert_not_null(self);
    xassert_not_null(config);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_SYSTEM);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_UART);

    xhal_serial_t *serial = XHAL_SERIAL_CAST(self);
    xhal_err_t ret        = XHAL_OK;

    xperiph_lock(self);
    ret = serial->ops->set_config(serial, config);
    if (ret == XHAL_OK)
    {
        serial->data.config = *config;
    }
    xperiph_unlock(self);

    return ret;
}

xhal_err_t xserial_set_baudrate(xhal_periph_t *self, uint32_t baudrate)
{
    xassert_not_null(self);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_SYSTEM);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_UART);

    xhal_serial_t *serial = XHAL_SERIAL_CAST(self);

    xperiph_lock(self);
    xhal_serial_config_t config = serial->data.config;
    xperiph_unlock(self);
    config.baud_rate = baudrate;

    return xserial_set_config(self, &config);
}
