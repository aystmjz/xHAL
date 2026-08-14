#include "xhal_serial.h"
#include "../xcore/xhal_assert.h"
#include "../xcore/xhal_log.h"
#include "../xcore/xhal_malloc.h"
#include "../xcore/xhal_time.h"
#include <stdarg.h>
#include <stdio.h>

XHAL_TAG(xSerial);

#define IS_XSERIAL_DATA_BITS(BITS) \
    (((BITS) == XSERIAL_DATA_BITS_8) || ((BITS) == XSERIAL_DATA_BITS_9))

#define IS_XSERIAL_STOP_BITS(BITS) \
    (((BITS) == XSERIAL_STOP_BITS_1) || ((BITS) == XSERIAL_STOP_BITS_2))

#define IS_XSERIAL_PARITY(PARITY)                                             \
    (((PARITY) == XSERIAL_PARITY_NONE) || ((PARITY) == XSERIAL_PARITY_ODD) || \
     ((PARITY) == XSERIAL_PARITY_EVEN))

#if (XHAL_OS_SUPPORTING == 1)
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
    xassert_info(IS_XSERIAL_DATA_BITS(config->data_bits), name);
    xassert_info(IS_XSERIAL_STOP_BITS(config->stop_bits), name);
    xassert_info(IS_XSERIAL_PARITY(config->parity), name);

    xhal_err_t ret                   = XHAL_OK;
    xhal_serial_t *serial            = self;
    xhal_periph_attr_t periph_config = {
        .name = name,
        .type = XHAL_PERIPH_UART,
    };

    ret = xperiph_register(&serial->peri, &periph_config);
    if (ret != XHAL_OK)
    {
        return ret;
    }

    serial->ops         = ops;
    serial->data.config = *config;
    serial->data.name   = serial_name;

    xrbuf_init(&serial->data.tx_rbuf, tx_buff, tx_bufsz);
    xrbuf_init(&serial->data.rx_rbuf, rx_buff, rx_bufsz);

#if (XHAL_OS_SUPPORTING == 1)
    serial->data.rx_expect  = 1;
    serial->data.tx_mutex   = osMutexNew(&xserial_mutex_attr);
    serial->data.rx_mutex   = osMutexNew(&xserial_mutex_attr);
    serial->data.event_flag = osEventFlagsNew(&xserial_event_flag_attr);

    if ((serial->data.tx_mutex == NULL) || (serial->data.rx_mutex == NULL) ||
        (serial->data.event_flag == NULL))
    {
        ret = XHAL_ERR_NO_MEMORY;
        goto fail;
    }
#endif
    ret = serial->ops->init(serial);
    if (ret != XHAL_OK)
    {
        goto fail;
    }

    serial->peri.is_inited = XPERIPH_INITED;

    return XHAL_OK;

fail:
    xperiph_unregister(&serial->peri);

#if (XHAL_OS_SUPPORTING == 1)
    if (serial->data.event_flag != NULL)
    {
        osEventFlagsDelete(serial->data.event_flag);
    }
    if (serial->data.tx_mutex != NULL)
    {
        osMutexDelete(serial->data.tx_mutex);
    }
    if (serial->data.rx_mutex != NULL)
    {
        osMutexDelete(serial->data.rx_mutex);
    }
#endif

    return ret;
}

uint32_t xserial_write(xhal_periph_t *self, const void *data, uint32_t size,
                       uint32_t timeout_ms)
{
    xassert_not_null(self);
    xassert_not_null(data);
    XPERIPH_CHECK_INIT(self, 0);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_UART);

    if (size == 0)
        return 0;

    xhal_serial_t *serial     = XSERIAL_CAST(self);
    xhal_tick_t start_tick_ms = xtime_get_tick_ms();
    uint32_t written          = 0;

#if (XHAL_OS_SUPPORTING == 1)
    osStatus_t ret_os = osOK;
    ret_os            = osMutexAcquire(serial->data.tx_mutex, osWaitForever);
    xassert(ret_os == osOK);
#endif
    while (1)
    {
        uint32_t w = serial->ops->transmit(
            serial, (const uint8_t *)data + written, size - written);
        written += w;
        if (written >= size)
            break;

        uint32_t elapsed_ms = TIME_DIFF(xtime_get_tick_ms(), start_tick_ms);

        if (elapsed_ms >= timeout_ms)
            break;

#if (XHAL_OS_SUPPORTING == 1)
        uint32_t wait_ms = timeout_ms - elapsed_ms;
        osEventFlagsWait(serial->data.event_flag, XSERIAL_EVENT_CAN_WRITE,
                         osFlagsWaitAll, XOS_MS_TO_TICKS(wait_ms));
#endif
    }

#if (XHAL_OS_SUPPORTING == 1)
    ret_os = osMutexRelease(serial->data.tx_mutex);
    xassert(ret_os == osOK);
#endif
    return written;
}

uint32_t xserial_read(xhal_periph_t *self, void *buf, uint32_t size,
                      uint32_t timeout_ms)
{
    xassert_not_null(self);
    xassert_not_null(buf);
    XPERIPH_CHECK_INIT(self, 0);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_UART);
    if (size == 0)
        return 0;

    xhal_serial_t *serial     = XSERIAL_CAST(self);
    xhal_tick_t start_tick_ms = xtime_get_tick_ms();
    uint32_t read             = 0;

#if (XHAL_OS_SUPPORTING == 1)
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

        uint32_t elapsed_ms = TIME_DIFF(xtime_get_tick_ms(), start_tick_ms);
        if (elapsed_ms >= timeout_ms)
            break;

#if (XHAL_OS_SUPPORTING == 1)
        uint32_t wait_ms = timeout_ms - elapsed_ms;
        osEventFlagsWait(serial->data.event_flag, XSERIAL_EVENT_CAN_READ,
                         osFlagsWaitAll, XOS_MS_TO_TICKS(wait_ms));
#endif
    }

#if (XHAL_OS_SUPPORTING == 1)
    ret_os = osMutexRelease(serial->data.rx_mutex);
    xassert(ret_os == osOK);
#endif

    return read;
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

    xhal_serial_t *serial = XSERIAL_CAST(self);
    uint32_t peeked       = 0;

#if (XHAL_OS_SUPPORTING == 1)
    osStatus_t ret_os = osOK;
    ret_os            = osMutexAcquire(serial->data.rx_mutex, osWaitForever);
    xassert(ret_os == osOK);
#endif
    peeked = xrbuf_peek(&serial->data.rx_rbuf, offset, buff, size);

#if (XHAL_OS_SUPPORTING == 1)
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

    xhal_serial_t *serial = XSERIAL_CAST(self);
    uint32_t skipped      = 0;

#if (XHAL_OS_SUPPORTING == 1)
    osStatus_t ret_os = osOK;
    ret_os            = osMutexAcquire(serial->data.rx_mutex, osWaitForever);
    xassert(ret_os == osOK);
#endif
    skipped = xrbuf_skip(&serial->data.rx_rbuf, size);

#if (XHAL_OS_SUPPORTING == 1)
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

    xhal_serial_t *serial = XSERIAL_CAST(self);
    uint8_t found         = 0;

#if (XHAL_OS_SUPPORTING == 1)
    osStatus_t ret_os = osOK;
    ret_os            = osMutexAcquire(serial->data.rx_mutex, osWaitForever);
    xassert(ret_os == osOK);
#endif
    found = xrbuf_find(&serial->data.rx_rbuf, data, size, offset, index);

#if (XHAL_OS_SUPPORTING == 1)
    ret_os = osMutexRelease(serial->data.rx_mutex);
    xassert(ret_os == osOK);
#endif
    return found;
}

xhal_err_t xserial_clear(xhal_periph_t *self)
{
    xassert_not_null(self);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_UART);

    xhal_err_t ret        = XHAL_OK;
    xhal_serial_t *serial = XSERIAL_CAST(self);

#if (XHAL_OS_SUPPORTING == 1)
    osStatus_t ret_os = osOK;
    ret_os            = osMutexAcquire(serial->data.rx_mutex, osWaitForever);
    xassert(ret_os == osOK);
#endif
    uint32_t full = xrbuf_get_full(&serial->data.rx_rbuf);
    uint32_t len  = xrbuf_skip(&serial->data.rx_rbuf, full);
    if (len != full)
    {
        ret = XHAL_ERROR;
    }
#if (XHAL_OS_SUPPORTING == 1)
    ret_os = osMutexRelease(serial->data.rx_mutex);
    xassert(ret_os == osOK);
#endif
    return ret;
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
        return xserial_write(self, stack_buf, len, XHAL_WAIT_FOREVER);
    }

    char *heap_buf = (char *)xmalloc(len + 1);
    if (heap_buf == NULL)
        return 0;

    va_start(args, fmt);
    vsnprintf(heap_buf, len + 1, fmt, args);
    va_end(args);

    uint32_t written = xserial_write(self, heap_buf, len, XHAL_WAIT_FOREVER);

    xfree(heap_buf);

    return written;
}

uint32_t xserial_scanf(xhal_periph_t *self, const char *fmt, ...)
{
    xassert_not_null(self);
    xassert_not_null(fmt);
    XPERIPH_CHECK_INIT(self, 0);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_UART);

    xhal_serial_t *serial = XSERIAL_CAST(self);
    int32_t ret           = 0;

#if (XHAL_OS_SUPPORTING == 1)
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
    xassert_info(read == len, self->attr.name);

    buf[len] = '\0';

    va_list args;
    va_start(args, fmt);
    ret = vsscanf(buf, fmt, args);
    va_end(args);

    xfree(buf);

exit:

#if (XHAL_OS_SUPPORTING == 1)
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
        if (xserial_read(self, &ch, 1, XHAL_WAIT_FOREVER) == 1)
        {
            if (ch == '\r' || ch == '\n')
            {
                /* 回车：输入结束 */
                xserial_write(self, "\r\n", 2,
                              XHAL_WAIT_FOREVER); /* 回显换行 */
                break;
            }
            else if ((ch == '\b' || ch == 127) && len > 0)
            {
                /* 退格键（8或127），删除一个字符 */
                len--;
                xserial_write(self, "\b \b", 3,
                              XHAL_WAIT_FOREVER); /* 回显退格 */
            }
            else if (len < sizeof(buf) - 1)
            {
                /* 普通字符，存入缓冲区并回显 */
                buf[len++] = ch;
                xserial_write(self, &ch, 1, XHAL_WAIT_FOREVER);
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
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_UART);

    xhal_serial_t *serial = XSERIAL_CAST(self);

#if (XHAL_OS_SUPPORTING == 1)
    osStatus_t ret_os = osOK;
    ret_os            = osMutexAcquire(serial->data.rx_mutex, osWaitForever);
    xassert(ret_os == osOK);
#endif
    status->rx_used = xrbuf_get_full(&serial->data.rx_rbuf);
    status->rx_free = xrbuf_get_free(&serial->data.rx_rbuf);

#if (XHAL_OS_SUPPORTING == 1)
    ret_os = osMutexRelease(serial->data.rx_mutex);
    xassert(ret_os == osOK);
    ret_os = osMutexAcquire(serial->data.tx_mutex, osWaitForever);
    xassert(ret_os == osOK);
#endif
    status->tx_used = xrbuf_get_full(&serial->data.tx_rbuf);
    status->tx_free = xrbuf_get_free(&serial->data.tx_rbuf);

#if (XHAL_OS_SUPPORTING == 1)
    ret_os = osMutexRelease(serial->data.tx_mutex);
    xassert(ret_os == osOK);
#endif
    return XHAL_OK;
}

xhal_err_t xserial_get_config(xhal_periph_t *self, xhal_serial_config_t *config)
{
    xassert_not_null(self);
    xassert_not_null(config);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_UART);

    xhal_serial_t *serial = XSERIAL_CAST(self);

    xperiph_lock(self);
    *config = serial->data.config;
    xperiph_unlock(self);

    return XHAL_OK;
}

xhal_err_t xserial_set_config(xhal_periph_t *self, xhal_serial_config_t *config)
{
    xassert_not_null(self);
    xassert_not_null(config);
    xassert_info(IS_XSERIAL_DATA_BITS(config->data_bits), self->attr.name);
    xassert_info(IS_XSERIAL_STOP_BITS(config->stop_bits), self->attr.name);
    xassert_info(IS_XSERIAL_PARITY(config->parity), self->attr.name);
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_UART);

    xhal_serial_t *serial = XSERIAL_CAST(self);
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
    XPERIPH_CHECK_INIT(self, XHAL_ERR_NO_INIT);
    XPERIPH_CHECK_TYPE(self, XHAL_PERIPH_UART);

    xhal_serial_t *serial = XSERIAL_CAST(self);

    xperiph_lock(self);
    xhal_serial_config_t config = serial->data.config;
    xperiph_unlock(self);
    if (baudrate == config.baud_rate)
    {
        return XHAL_OK;
    }
    config.baud_rate = baudrate;

    return xserial_set_config(self, &config);
}
