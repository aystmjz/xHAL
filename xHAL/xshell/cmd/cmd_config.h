#ifndef __CMD_CONFIG_H_
#define __CMD_CONFIG_H_

#define SHELL_CMD_ENABLE_ALL      (1)

#define SHELL_CMD_ENABLE_TIME     (1)
#define SHELL_CMD_ENABLE_DUMP     (1)
#define SHELL_CMD_ENABLE_MEM      (1)
#define SHELL_CMD_ENABLE_LOG      (1)
#define SHELL_CMD_ENABLE_VER      (1)
#define SHELL_CMD_ENABLE_REBOOT   (1)
#define SHELL_CMD_ENABLE_SHUTDOWN (1)
#define SHELL_CMD_ENABLE_TASKS    (1)
#define SHELL_CMD_ENABLE_KILL     (1)

#define SHELL_CMD_IS_ENABLED(cmd) \
    (SHELL_CMD_ENABLE_ALL && SHELL_CMD_ENABLE_##cmd)

#define SHELL_CMD_IF_ENABLED(cmd, code) \
    do                                  \
    {                                   \
        if (SHELL_CMD_ENABLE_##cmd)     \
        {                               \
            code                        \
        }                               \
    } while (0)

#define SHELL_CMD_IF_DISABLED(cmd, code) \
    do                                   \
    {                                    \
        if (!SHELL_CMD_ENABLE_##cmd)     \
        {                                \
            code                         \
        }                                \
    } while (0)

#endif /* __CMD_CONFIG_H_ */
