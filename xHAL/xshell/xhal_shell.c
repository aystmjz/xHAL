#include "xhal_shell.h"
#include "../xcore/xhal_coro.h"
#include "../xcore/xhal_def.h"
#include "../xcore/xhal_export.h"
#include "../xperiph/xhal_serial.h"
#include "Shell/shell.h"

#if (XHAL_OS_SUPPORTING == 1)
    #include "../xos/xhal_os.h"
#endif

#define POLL_PERIOD_MS (10)

static Shell shell;
static char shell_buffer[XSHELL_BUFFER_SIZE];

extern int16_t shell_read(char *buff, uint16_t size);
extern int16_t shell_write(char *data, uint16_t size);

XHAL_WEAK int16_t shell_read(char *buff, uint16_t size)
{
    xhal_periph_t *uart = xperiph_find(XHAL_DEFAULT_UART);
    if (uart != NULL)
    {
        return (int16_t)xserial_read(uart, (void *)buff, (uint32_t)size, 0);
    }
    else
    {
        return 0;
    }
}

XHAL_WEAK int16_t shell_write(char *data, uint16_t size)
{
    xhal_periph_t *uart = xperiph_find(XHAL_DEFAULT_UART);
    if (uart != NULL)
    {
        return (int16_t)xserial_write(uart, data, (uint32_t)size,
                                      XHAL_WAIT_FOREVER);
    }
    else
    {
        return 0;
    }
}

#if (XHAL_OS_SUPPORTING == 1)
static const osThreadAttr_t shell_task_attr = {
    .name       = "shell_task",
    .priority   = osPriorityNormal,
    .stack_size = 2048,
};
static void shell_task(void *argument)
{
    while (1)
    {
        shellTask(argument);
        osDelay(XOS_MS_TO_TICKS(POLL_PERIOD_MS));
    }
}
#else
static xcoro_handle_t shell_coro_handle;
static void shell_coro(xcoro_handle_t *handle)
{
    XCORO_BEGIN(handle);
    while (1)
    {
        shellTask(&shell);
        XCORO_DELAY_MS(handle, POLL_PERIOD_MS);
    }
    XCORO_END(handle);
}
#endif

Shell *shell_get(void)
{
    return &shell;
}

xhal_err_t shell_init(void)
{
    shell.read  = shell_read;
    shell.write = shell_write;

    shellInit(&shell, shell_buffer, sizeof(shell_buffer));

#if (XHAL_OS_SUPPORTING == 1)
    osThreadId_t thread_id = osThreadNew(shell_task, &shell, &shell_task_attr);
    if (thread_id == NULL)
    {
        return XHAL_ERROR;
    }
#else
    xhal_err_t ret;
    XTRY(ret, xcoro_handle_init(&shell_coro_handle, shell_coro,
                                XCORO_PRIO_NORMAL, NULL));
    XTRY(ret, xcoro_register(&g_coro_manager, &shell_coro_handle));
#endif

    return XHAL_OK;
}