#include "../../xos/xhal_os.h"
#include "../xhal_shell.h"
#include "cmd_config.h"
#include <stdlib.h>
#include <string.h>

#define TASK_FIND_MAX_LEN      32
#define OS_SUPPORT_THREAD_NAME 1

#define CMD_KILL_DESCRIPTION   "kill <thread_name> or kill 0x<handle>\r\n"

#if SHELL_CMD_IS_ENABLED(KILL) && defined(XHAL_OS_SUPPORTING)

static const char *protected_threads[] = {
    "idle",     "main",      "tmr svc", "timer",   "timer service",
    "timersvc", "tmrsvc",    "tmr_svc", "systick", "system timer",
    "isr",      "interrupt", NULL};
static int kill_cmd(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    SHELL_ASSERT(shell, return -1);

    if (argc != 2)
    {
        shellPrint(shell, "usage:\r\n");
        shellPrint(shell, CMD_KILL_DESCRIPTION);
        return -1;
    }

    osKernelState_t kernel_state = osKernelGetState();
    if (kernel_state != osKernelRunning)
    {
        shellPrint(shell,
                   "Kill command not available: kernel not running.\r\n");
        return -1;
    }

    char *target               = argv[1];
    osThreadId_t target_thread = NULL;
    uint8_t found              = 0;

    char *endptr;
    xhal_pointer_t handle_value = (xhal_pointer_t)strtoul(target, &endptr, 0);

    if (endptr != target && *endptr == '\0' && handle_value != 0)
    {
        target_thread = (osThreadId_t)handle_value;
    }

    osThreadId_t thread_list[TASK_FIND_MAX_LEN];
    uint32_t count = osThreadEnumerate(thread_list, TASK_FIND_MAX_LEN);

    for (uint32_t i = 0; i < count; i++)
    {
        const char *name = "Unnamed";
#if OS_SUPPORT_THREAD_NAME
        name = osThreadGetName(thread_list[i]);
#endif
        if ((name != NULL && strcmp(name, target) == 0))
        {
            target_thread = thread_list[i];
            found         = 1;
            break;
        }
        else if ((target_thread == thread_list[i]))
        {
            found = 1;
            break;
        }
    }

    if (!found)
    {
        shellPrint(shell, "Thread not found: %s\r\n", target);
        return -1;
    }

    osThreadState_t state   = osThreadGetState(target_thread);
    const char *thread_name = "Unnamed";
#if OS_SUPPORT_THREAD_NAME
    thread_name = osThreadGetName(target_thread);
#endif

    osThreadId_t current_thread = osThreadGetId();

    if (target_thread == current_thread)
    {
        shellPrint(shell, "Cannot kill current thread: %s (0x%08lx)\r\n",
                   thread_name, (xhal_pointer_t)target_thread);
        return -1;
    }

    for (uint32_t i = 0; protected_threads[i] != NULL; i++)
    {
        if (strcasecmp(thread_name, protected_threads[i]) == 0)
        {
            shellPrint(shell,
                       "Cannot kill system critical thread: %s (0x%08lx)\r\n",
                       thread_name, (xhal_pointer_t)target_thread);
            return -1;
        }
    }

    osStatus_t status = osThreadTerminate(target_thread);

    if (status == osOK)
    {
        shellPrint(shell, "Thread terminated: %s (0x%08lx)\r\n", thread_name,
                   (xhal_pointer_t)target_thread);
        return 0;
    }
    else
    {
        shellPrint(shell, "Failed to terminate thread: %s (error: %d)\r\n",
                   thread_name, status);
        return -1;
    }
}

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), kill,
    kill_cmd,
    "\r\nTerminate a thread by name or handle\r\n" CMD_KILL_DESCRIPTION);

#endif /* SHELL_CMD_IS_ENABLED(KILL) */
