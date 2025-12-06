#include "../../xcore/xhal_export.h"
#include "../../xos/xhal_os.h"
#include "../xhal_shell.h"
#include "cmd_config.h"
#include XHAL_DEVICE_HEADER

#define TASK_LIST_LEN              32
#define OS_SUPPORT_THREAD_NAME     1
#define OS_SUPPORT_THREAD_PRIORITY 1
#define OS_SUPPORT_THREAD_STACK    0

#define CMD_TASKS_DESCRIPTION      "tasks: list all registered threads\r\n"

#if SHELL_CMD_IS_ENABLED(TASKS) && defined(XHAL_OS_SUPPORTING)
static int tasks_cmd(int argc, char *argv[])
{
    Shell *shell = shellGetCurrent();
    SHELL_ASSERT(shell, return -1);

    if (argc > 1)
    {
        shellPrint(shell, "usage:\r\n");
        shellPrint(shell, CMD_TASKS_DESCRIPTION);
        return -1;
    }

    osKernelState_t kernel_state = osKernelGetState();
    if (kernel_state != osKernelRunning)
    {
        shellPrint(shell,
                   "Task enumeration not available: kernel not running.\r\n");
        return 0;
    }

    osThreadId_t thread_list[TASK_LIST_LEN];
    uint32_t count = osThreadEnumerate(thread_list, TASK_LIST_LEN);

    shellPrint(shell, "Thread List (total: %lu)\r\n", count);
    shellPrint(
        shell,
        "Handle        Name           State      Priority   StackSize\r\n");
    shellPrint(
        shell,
        "-------------------------------------------------------------\r\n");

    for (uint32_t i = 0; i < count; i++)
    {
        osThreadState_t state = osThreadGetState(thread_list[i]);
        const char *name      = "Unnamed";
#if OS_SUPPORT_THREAD_NAME
        name = osThreadGetName(thread_list[i]);
#endif
        int prio = 0;
#if OS_SUPPORT_THREAD_PRIORITY
        prio = (int)osThreadGetPriority(thread_list[i]);
#endif
        uint32_t stack_size = 0;
#if OS_SUPPORT_THREAD_STACK
        stack_size = osThreadGetStackSize(thread_list[i]);
#endif
        const char *state_str = "UNKNOWN";
        switch (state)
        {
        case osThreadInactive:
            state_str = "INACTIVE";
            break;
        case osThreadReady:
            state_str = "READY";
            break;
        case osThreadRunning:
            state_str = "RUNNING";
            break;
        case osThreadBlocked:
            state_str = "BLOCKED";
            break;
        case osThreadTerminated:
            state_str = "TERMINATED";
            break;
        case osThreadError:
            state_str = "ERROR";
            break;
        default:
            state_str = "UNKNOWN";
            break;
        }

        shellPrint(shell, "0x%08lx    %-14s %-10s %-10d %lu\r\n",
                   (xhal_pointer_t)thread_list[i], name, state_str, prio,
                   stack_size);
    }

    if (count >= TASK_LIST_LEN)
    {
        shellPrint(shell, "...\r\n");
    }

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 tasks, tasks_cmd,
                 "\r\nList all registered threads\r\n" CMD_TASKS_DESCRIPTION);

#endif /* SHELL_CMD_IS_ENABLED(TASKS) */
