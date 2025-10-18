#include "xhal_export.h"
#include "xhal_log.h"
#include "xhal_serial.h"
#include <string.h>

extern const xhal_serial_ops_t serial_ops_driver;

static xhal_serial_t debug_usart;
static xhal_serial_attr_t debug_usart_attr = XSERIAL_ATTR_DEFAULT;

static char debug_usart_tx_buf[1024];
static char debug_usart_rx_buf[256];

static void xlog_puts(const char *s)
{
    xserial_write(XPERIPH_CAST(&debug_usart), s, strlen(s));
}

static void debug_usart_driver(void)
{
    xserial_inst(&debug_usart, "debug_usart", &serial_ops_driver, "USART1",
                 &debug_usart_attr, debug_usart_tx_buf, debug_usart_rx_buf,
                 sizeof(debug_usart_tx_buf), sizeof(debug_usart_rx_buf));

    xlog_set_level(XLOG_LEVEL_DEBUG);
    xlog_set_puts_func(xlog_puts);
}
INIT_EXPORT(debug_usart_driver, EXPORT_LEVEL_CORE);

static xhal_serial_t usart;
static xhal_serial_attr_t usart_attr = XSERIAL_ATTR_DEFAULT;

static char usart_tx_buf[512];
static char usart_rx_buf[128];

static void usart_driver(void)
{
    xserial_inst(&usart, "usart", &serial_ops_driver, "USART2", &usart_attr,
                 usart_tx_buf, usart_rx_buf, sizeof(usart_tx_buf),
                 sizeof(usart_rx_buf));
}
INIT_EXPORT(usart_driver, EXPORT_LEVEL_PERIPH);
