#include "xhal_export.h"
#include "xhal_log.h"
#include "xhal_serial.h"
#include "xhal_shell.h"
#include <string.h>
#include XHAL_CMSIS_DEVICE_HEADER

static xhal_serial_t debug_usart;
static const xhal_serial_config_t debug_usart_config = XSERIAL_CONFIG_DEFAULT;

static char debug_usart_tx_buf[1024];
static char debug_usart_rx_buf[256];

extern const xhal_serial_ops_t serial_ops_driver;
extern Shell shell;

void xlog_output(const void *data, uint32_t size)
{
    shellRefreshLineStart(&shell);
    xserial_write(XPERIPH_CAST(&debug_usart), data, size, XHAL_WAIT_FOREVER);
    shellRefreshLineEnd(&shell);
}

static void debug_usart_driver(void)
{
    xserial_inst(&debug_usart, "debug_usart", &serial_ops_driver, "USART1",
                 &debug_usart_config, debug_usart_tx_buf, debug_usart_rx_buf,
                 sizeof(debug_usart_tx_buf), sizeof(debug_usart_rx_buf));
}
INIT_EXPORT(debug_usart_driver, EXPORT_LEVEL_CORE);

static xhal_serial_t usart;
static xhal_serial_config_t usart_config = XSERIAL_CONFIG_DEFAULT;

static char usart_tx_buf[512];
static char usart_rx_buf[128];

static void usart_driver(void)
{
    xserial_inst(&usart, "usart", &serial_ops_driver, "USART2", &usart_config,
                 usart_tx_buf, usart_rx_buf, sizeof(usart_tx_buf),
                 sizeof(usart_rx_buf));
}
INIT_EXPORT(usart_driver, EXPORT_LEVEL_PERIPH);
