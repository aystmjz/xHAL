#include "xhal_export.h"
#include "xhal_serial.h"

extern const xhal_serial_ops_t serial_ops_driver;

static xhal_serial_t usart1;

static uint8_t tx_buf[512];
static uint8_t rx_buf[1024];

static xhal_serial_attr_t attr = XSERIAL_ATTR_DEFAULT;

static void driver_usart(void)
{
    xserial_inst(&usart1, "usart1", &serial_ops_driver, "USART1", &attr, tx_buf,
                 rx_buf, sizeof(tx_buf), sizeof(rx_buf));
}
INIT_EXPORT(driver_usart, EXPORT_LEVEL_BSP);
