#include "xhal_assert.h"
#include "xhal_export.h"
#include "xhal_log.h"
#include "xhal_serial.h"
#include "xhal_shell.h"

XLOG_TAG("xExportShell");

#define SHELL_POLL_PERIOD_MS (10)
#define SHELL_BUFFER_SIZE    (512)

Shell shell;

static char shell_buffer[SHELL_BUFFER_SIZE];
static xhal_periph_t *shell_usart;

static int16_t shell_read(void *buff, uint16_t size)
{
    return (int16_t)xserial_read(shell_usart, buff, (uint32_t)size, 2);
}

static int16_t shell_write(void *data, uint16_t size)
{
    return (int16_t)xserial_write(shell_usart, data, (uint32_t)size,
                                  XHAL_WAIT_FOREVER);
}

static void shell_driver(void)
{
    shell_usart = xperiph_find("debug_usart");
    xassert_not_null(shell_usart);

    shell.read  = (signed short (*)(char *, unsigned short))shell_read;
    shell.write = (signed short (*)(char *, unsigned short))shell_write;

    shellInit(&shell, shell_buffer, sizeof(shell_buffer));
}
INIT_EXPORT(shell_driver, EXPORT_LEVEL_APP);

static void shell_poll(void)
{
    char byte;
    while (shell.read(&byte, 1) == 1)
    {
        shellHandler(&shell, byte);
    }
}
POLL_EXPORT_OS(shell_poll, SHELL_POLL_PERIOD_MS, osPriorityNormal, 2048);
