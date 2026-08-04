/* --------------------------------------------------------------------------
 * Copyright (c) 2013-2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * --------------------------------------------------------------------------
 *
 * $Revision:   V10.7.1
 *
 * Project:     CMSIS-FreeRTOS
 * Title:       FreeRTOS configuration definitions
 *
 * --------------------------------------------------------------------------*/

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "xhal_config.h"
#define CMSIS_device_header XHAL_DEVICE_HEADER

#include "xhal_assert.h"
#define configASSERT(x) xassert_tag(x, "FreeRTOS")

#include "xhal_malloc.h"
#define pvPortMalloc xmalloc
#define vPortFree    xfree

#ifndef XOS_TICK_RATE_HZ
    #define XOS_TICK_RATE_HZ (1000)
#endif

/*-----------------------------------------------------------
 * Application specific definitions.
 *
 * These definitions should be adjusted for your particular hardware and
 * application requirements.
 *
 * THESE PARAMETERS ARE DESCRIBED WITHIN THE 'CONFIGURATION' SECTION OF THE
 * FreeRTOS API DOCUMENTATION AVAILABLE ON THE FreeRTOS.org WEB SITE.
 *
 * See http://www.freertos.org/a00110.html
 *----------------------------------------------------------*/

#if (defined(__ARMCC_VERSION) || defined(__GNUC__) || defined(__ICCARM__))
    #include <stdint.h>

    #include CMSIS_device_header
#endif

//-------- <<< Use Configuration Wizard in Context Menu >>> --------------------

//  <o>最小栈大小（字） <0-65535>
//  <i> 空闲任务和默认任务栈的栈大小（字）
//  <i> 默认：128
#define configMINIMAL_STACK_SIZE                  ((uint16_t)(128))

//  <o>总堆大小（字节） <0-0xFFFFFFFF>
//  <i> 堆内存大小（字节）
//  <i> 默认：8192
#define configTOTAL_HEAP_SIZE                     ((size_t)8192)

//  <o>内核节拍频率（Hz） <0-0xFFFFFFFF>
//  <i> 内核节拍频率（Hz）
//  <i> 默认：1000
#define configTICK_RATE_HZ                        ((TickType_t)XOS_TICK_RATE_HZ)

// 任务名字字符串长度
#define configMAX_TASK_NAME_LEN                   (16)

//  <o>可抢占中断优先级
//  <i> 可安全调用 FreeRTOS API 的中断的最高优先级
//  <i> 默认：128
#define configMAX_SYSCALL_INTERRUPT_PRIORITY      80
/*对于 configPRIO_BITS == 4 的情况，
系统可管理的最高中断优先级为 80 >> (8 - 4) = 5 */

//  <q>使用时间片轮转
//  <i> 启用时间片轮转设置
//  <i> 默认：1
#define configUSE_TIME_SLICING                    1

//  <q>使用低功耗空闲（tickless）
//  <i> 启用低功耗 tickless 模式，在空闲期间停止周期性节拍中断；
//  <i> 禁用则保持节拍中断始终运行
//  <i> 默认：0
#define configUSE_TICKLESS_IDLE                   0

//  <q>空闲任务让出 CPU
//  <i> 控制空闲任务的让出（Yield）行为
//  <i> 默认：1
#define configIDLE_SHOULD_YIELD                   1

/***********************************************************************
                FreeRTOS与软件定时器有关的配置选项
**********************************************************************/

//  <o>定时器任务栈深度（字） <0-65535>
//  <i> 定时器任务栈大小（字）
//  <i> 默认：128
#define configTIMER_TASK_STACK_DEPTH              (configMINIMAL_STACK_SIZE * 2)

//  <o>定时器任务优先级 <0-56>
//  <i> 定时器任务优先级
//  <i> 默认：40（高）
#define configTIMER_TASK_PRIORITY                 (configMAX_PRIORITIES - 1)

//  <o>定时器队列长度 <0-1024>
//  <i> 定时器命令队列长度
//  <i> 默认：5
#define configTIMER_QUEUE_LENGTH                  5

/***************************************************************
             FreeRTOS与钩子函数有关的配置选项
**************************************************************/

/* 置1：使用空闲钩子（Idle Hook类似于回调函数）；置0：忽略空闲钩子
 *
 * 空闲任务钩子是一个函数，这个函数由用户来实现，
 * FreeRTOS规定了函数的名字和参数：void vApplicationIdleHook(void )，
 * 这个函数在每个空闲任务周期都会被调用
 * 对于已经删除的RTOS任务，空闲任务可以释放分配给它们的堆栈内存。
 * 因此必须保证空闲任务可以被CPU执行
 * 使用空闲钩子函数设置CPU进入省电模式是很常见的
 * 不可以调用会引起空闲任务阻塞的API函数
 */
#define configUSE_IDLE_HOOK                       0

/* 置1：使用时间片钩子（Tick Hook）；置0：忽略时间片钩子
 *
 *
 * 时间片钩子是一个函数，这个函数由用户来实现，
 * FreeRTOS规定了函数的名字和参数：void vApplicationTickHook(void )
 * 时间片中断可以周期性的调用
 * 函数必须非常短小，不能大量使用堆栈，
 * 不能调用以”FromISR" 或 "FROM_ISR”结尾的API函数
 */
/*xTaskIncrementTick函数是在xPortSysTickHandler中断函数中被调用的。因此，vApplicationTickHook()函数执行的时间必须很短才行*/
#define configUSE_TICK_HOOK                       0

//  <q>使用守护任务启动钩子
//  <i> 定时器服务启动时调用回调函数
//  <i> 启用守护任务启动钩子时需实现回调函数 vApplicationDaemonTaskStartupHook
//  <i> 默认：0
#define configUSE_DAEMON_TASK_STARTUP_HOOK        0

//  <q>使用内存分配失败钩子
//  <i> 动态内存不足时调用回调函数
//  <i> 启用内存分配失败钩子时需实现回调函数 vApplicationMallocFailedHook
//  <i> 默认：0
#define configUSE_MALLOC_FAILED_HOOK              1

//  <o>栈溢出检查
//    <0=>禁用 <1=>方法一 <2=>方法二
//  <i> 启用或禁用栈溢出检查
//  <i> 启用栈检查时需实现回调函数 vApplicationStackOverflowHook
//  <i> 默认：0
#define configCHECK_FOR_STACK_OVERFLOW            2

/*****************************************************************
// <h>事件记录器（Event Recorder）配置
// <i> 初始化并设置事件记录器的记录级别过滤
// <i> 未使用事件记录器时，以下设置无效
*****************************************************************/

//  <q>初始化事件记录器
//  <i> 在 FreeRTOS 内核启动前初始化事件记录器
//  <i> 默认：1
#define configEVR_INITIALIZE                      1

//  <e>设置记录级别过滤
//  <i> 启用 FreeRTOS 事件记录级别的配置
//  <i> 默认：1
#define configEVR_SETUP_LEVEL                     1

//  <o>任务函数
//  <i> 定义任务函数产生事件的记录级别位掩码
//    <0x00=>关闭 <0x01=>错误 <0x03=>错误+API <0x05=>错误+操作
//    <0x07=>错误+API+操作 <0x0F=>全部
//  <i> 0x00=关闭 0x01=错误 0x03=错误+API 0x05=错误+操作
//  <i> 0x07=错误+API+操作 0x0F=全部
//  <i> 默认：0x05
#define configEVR_LEVEL_TASKS                     0x05

//  <o>队列函数
//  <i> 定义队列函数产生事件的记录级别位掩码
//    <0x00=>关闭 <0x01=>错误 <0x03=>错误+API <0x05=>错误+操作
//    <0x07=>错误+API+操作 <0x0F=>全部
//  <i> 0x00=关闭 0x01=错误 0x03=错误+API 0x05=错误+操作
//  <i> 0x07=错误+API+操作 0x0F=全部
//  <i> 默认：0x05
#define configEVR_LEVEL_QUEUE                     0x05

//  <o>定时器函数
//  <i> 定义定时器函数产生事件的记录级别位掩码
//    <0x00=>关闭 <0x01=>错误 <0x03=>错误+API <0x05=>错误+操作
//    <0x07=>错误+API+操作 <0x0F=>全部
//  <i> 0x00=关闭 0x01=错误 0x03=错误+API 0x05=错误+操作
//  <i> 0x07=错误+API+操作 0x0F=全部
//  <i> 默认：0x05
#define configEVR_LEVEL_TIMERS                    0x05

//  <o>事件组函数
//  <i> 定义事件组函数产生事件的记录级别位掩码
//    <0x00=>关闭 <0x01=>错误 <0x03=>错误+API <0x05=>错误+操作
//    <0x07=>错误+API+操作 <0x0F=>全部
//  <i> 0x00=关闭 0x01=错误 0x03=错误+API 0x05=错误+操作
//  <i> 0x07=错误+API+操作 0x0F=全部
//  <i> 默认：0x05
#define configEVR_LEVEL_EVENTGROUPS               0x05

//  <o>堆函数
//  <i> 定义堆函数产生事件的记录级别位掩码
//    <0x00=>关闭 <0x01=>错误 <0x03=>错误+API <0x05=>错误+操作
//    <0x07=>错误+API+操作 <0x0F=>全部
//  <i> 0x00=关闭 0x01=错误 0x03=错误+API 0x05=错误+操作
//  <i> 0x07=错误+API+操作 0x0F=全部
//  <i> 默认：0x05
#define configEVR_LEVEL_HEAP                      0x05

//  <o>流缓冲区函数
//  <i> 定义流缓冲区函数产生事件的记录级别位掩码
//    <0x00=>关闭 <0x01=>错误 <0x03=>错误+API <0x05=>错误+操作
//    <0x07=>错误+API+操作 <0x0F=>全部
//  <i> 0x00=关闭 0x01=错误 0x03=错误+API 0x05=错误+操作
//  <i> 0x07=错误+API+操作 0x0F=全部
//  <i> 默认：0x05
#define configEVR_LEVEL_STREAMBUFFER              0x05
//  </e>
// </h>

/*****************************************************************
// <h>内存分配配置
// <i> 启用并配置内存分配相关特性
// <i> 要配置 FreeRTOS 堆大小，请使用 configTOTAL_HEAP_SIZE
*****************************************************************/

//  <q>支持静态内存分配
//  <i> 启用或禁用静态内存分配
//  <i> 启用后可使用应用程序提供的 RAM 创建 RTOS 对象
//  <i> 默认：1
#define configSUPPORT_STATIC_ALLOCATION           1

//  <q>支持动态内存分配
//  <i> 启用或禁用动态内存分配
//  <i> 启用后可从 FreeRTOS 堆自动分配 RAM 创建 RTOS 对象
//  <i> 默认：1
#define configSUPPORT_DYNAMIC_ALLOCATION          1

//  <q>使用内核提供的静态内存
//  <i> 启用后 FreeRTOS 内核为空闲任务和定时器任务提供静态内存；
//  <i> 否则用户需提供 vApplicationGetIdleTaskMemory、
//  <i> vApplicationGetTimerTaskMemory 以及（SMP
//  时）vApplicationGetPassiveIdleTaskMemory 的实现 <i> 默认：1
#define configKERNEL_PROVIDED_STATIC_MEMORY       1

//  <q>使用应用程序分配的堆
//  <i> 使用应用程序分配的堆时，须在外部提供全局堆缓冲区，
//  <i> 缓冲区须声明为：uint8_t ucHeap[configTOTAL_HEAP_SIZE]
//  <i> 默认：0
#define configAPPLICATION_ALLOCATED_HEAP          0

//  <q>使用独立堆分配栈空间
//  <i> 启用或禁用从独立堆为任意任务分配栈空间
//  <i> 使用独立堆时需提供 pvPortMallocStack 和 vPortFreeStack 的线程安全实现
//  <i> 默认：0
#define configSTACK_ALLOCATION_FROM_SEPARATE_HEAP 0

//  <q>使用堆保护
//  <i> 启用或禁用堆块指针的边界检查与混淆；
//  <i> 该设置仅适用于 Heap_4 和 Heap_5
//  <i> 默认：0
#define configENABLE_HEAP_PROTECTOR               0
// </h>

/*****************************************************************
// <h>移植（Port）特定配置
// <i> 启用并配置移植（Port）特定特性
// <i> 有关适用于所用移植的定义，请查阅 FreeRTOS 文档
*****************************************************************/

//  <q>使用浮点单元（FPU）
//  <i> 使用浮点单元（FPU）会影响上下文处理
//  <i> 当应用程序使用浮点运算时启用 FPU
//  <i> 此设置仅适用于 ARMv8-M 移植
//  <i> 默认：1
#define configENABLE_FPU                          0

//  <q>使用 M-Profile 向量扩展（MVE）
//  <i> 使用 M-Profile 向量扩展（MVE）会影响上下文处理
//  <i> 当应用程序使用信号处理和机器学习算法时启用 MVE
//  <i> 此设置仅适用于 ARMv8-M 移植
//  <i> 默认：0
#define configENABLE_MVE                          0

//  <q>使用内存保护单元（MPU）
//  <i> 使用内存保护单元（MPU）需要定义详细的内存映射；
//  <i> 此设置仅适用于启用 MPU 的 ARMv8-M 移植
//  <i> 默认：0
#define configENABLE_MPU                          0

//  <q>仅使用 TrustZone 安全侧
//  <i> 此设置阻止 FreeRTOS 上下文切换到非安全侧
//  <i> 当 FreeRTOS 仅在安全侧运行时启用此设置
//  <i> 此设置仅适用于 ARMv8-M 移植
//  <i> 默认：1
#define configRUN_FREERTOS_SECURE_ONLY            1

//  <q>使用 TrustZone 安全扩展
//  <i> 使用 TrustZone 会影响上下文处理
//  <i> 当 FreeRTOS 在非安全侧运行并调用安全侧函数时启用 TrustZone；
//  <i> 此设置仅适用于 ARMv8-M 移植
//  <i> 默认：0
#define configENABLE_TRUSTZONE                    0

//  <o>最小安全栈大小（字） <0-65535>
//  <i> 空闲任务安全侧上下文的栈大小（字）
//  <i> 此设置仅在启用 TrustZone 扩展的 ARMv8-M 移植上有效
//  <i> 默认：128
#define configMINIMAL_SECURE_STACK_SIZE           ((uint32_t)128)

/*****************************************************************
// <h>对称多处理（SMP）配置
// <i> 为对称多处理（SMP）启用并配置 FreeRTOS
*****************************************************************/

//  <o>处理器核心数
//  <i> 设置可用处理器核心数
//  <i> 默认：1
#define configNUMBER_OF_CORES                     1

//  <q>使用处理器核心亲和性
//  <i> 启用任务在特定处理器核心上运行的控制
//  <i> 未设置处理器亲和性的任务可在任何可用核心上运行
//  <i> 默认：0
#define configUSE_CORE_AFFINITY                   0

//  <q>使用被动空闲钩子
//  <i> 每次空闲任务迭代时调用回调函数
//  <i> 启用空闲钩子时需实现回调函数 vApplicationPassiveIdleHook
//  <i> 默认：0
#define configUSE_PASSIVE_IDLE_HOOK               0
// </h>

/********************************************************************
          FreeRTOS与运行时间和任务状态收集有关的配置选项
**********************************************************************/
// 启用运行时间统计功能
#define configGENERATE_RUN_TIME_STATS             0

// 启用可视化跟踪调试
// #define configUSE_TRACE_FACILITY                1

/* 与宏configUSE_TRACE_FACILITY同时为1时会编译下面3个函数
 * prvWriteNameToBuffer()
 * vTaskList(),
 * vTaskGetRunTimeStats()
 */
#define configUSE_STATS_FORMATTING_FUNCTIONS      1

//  <o>队列注册表大小
//  <i> 定义出于调试目的注册的队列对象的最大数量
//  <i> 队列注册表供内核感知调试器定位队列和信号量结构并显示关联的文本名称
//  <i> 默认：0
#define configQUEUE_REGISTRY_SIZE                 5

/************************************************************************
 *               FreeRTOS CMSIS RTOS2配置选项
 *********************************************************************/
/* 置1：使能FPU（浮点运算单元）；置0：禁用FPU
 * 当MCU具有浮点运算单元时，可根据需要进行配置 */
#define configENABLE_FPU                          0

/* 置1：使能MPU（内存保护单元）；置0：禁用MPU
 * 当MCU具有内存保护单元时，可根据需要进行配置 */
#define configENABLE_MPU                          0

/* 置1：使能OS2线程挂起和恢复功能；置0：禁用该功能
 * 用于支持CMSIS RTOS2的线程挂起和恢复操作 */
#define configUSE_OS2_THREAD_SUSPEND_RESUME       1

/* 置1：使能OS2线程枚举功能；置0：禁用该功能
 * 用于支持CMSIS RTOS2的线程枚举操作 */
#define configUSE_OS2_THREAD_ENUMERATE            1

/* 置1：使能OS2事件标志从中断服务例程设置；置0：禁用该功能
 * 用于支持CMSIS RTOS2从中断中设置事件标志 */
#define configUSE_OS2_EVENTFLAGS_FROM_ISR         1

/* 置1：使能OS2线程标志功能；置0：禁用该功能
 * 用于支持CMSIS RTOS2的线程标志操作 */
#define configUSE_OS2_THREAD_FLAGS                1

/* 置1：使能OS2定时器功能；置0：禁用该功能
 * 用于支持CMSIS RTOS2的软件定时器功能 */
#define configUSE_OS2_TIMER                       1

/* 置1：使能OS2互斥锁功能；置0：禁用该功能
 * 用于支持CMSIS RTOS2的互斥锁操作 */
#define configUSE_OS2_MUTEX                       1

//------------- <<< end of configuration section >>> ---------------------------

/* Define to trap errors during development */
// #define configASSERT(x)               \
//     do                                \
//     {                                 \
//         if ((x) == 0)                 \
//         {                             \
//             taskDISABLE_INTERRUPTS(); \
//             for (;;)                  \
//             {                         \
//                 ;                     \
//             }                         \
//         }                             \
//     } while (0)

/* Defines needed by FreeRTOS to implement CMSIS RTOS2 API. Do not change! */
#define configCPU_CLOCK_HZ                        (XHAL_CPU_FREQ_HZ)
#define configUSE_PREEMPTION                      1
#define configUSE_TIMERS                          1
#define configUSE_MUTEXES                         1
#define configUSE_RECURSIVE_MUTEXES               1
#define configUSE_COUNTING_SEMAPHORES             1
#define configUSE_TASK_NOTIFICATIONS              1
#define configUSE_TRACE_FACILITY                  1
#define configUSE_16_BIT_TICKS                    0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION   0
#define configMAX_PRIORITIES                      56
#define configKERNEL_INTERRUPT_PRIORITY           255

/* Defines that include FreeRTOS functions which implement CMSIS RTOS2 API. Do
 * not change! */
#define INCLUDE_xEventGroupSetBitsFromISR         1
#define INCLUDE_xSemaphoreGetMutexHolder          1
#define INCLUDE_vTaskDelay                        1
#define INCLUDE_xTaskDelayUntil                   1
#define INCLUDE_vTaskDelete                       1
#define INCLUDE_xTaskGetCurrentTaskHandle         1
#define INCLUDE_xTaskGetSchedulerState            1
#define INCLUDE_uxTaskGetStackHighWaterMark       1
#define INCLUDE_uxTaskPriorityGet                 1
#define INCLUDE_vTaskPrioritySet                  1
#define INCLUDE_eTaskGetState                     1
#define INCLUDE_vTaskSuspend                      1
#define INCLUDE_xTaskAbortDelay                   1
#define INCLUDE_xTimerPendFunctionCall            1

/* Cortex-M specifics */
/* Map the FreeRTOS port interrupt handlers to their CMSIS standard names.
 */
#define xPortPendSVHandler                        PendSV_Handler
#define vPortSVCHandler                           SVC_Handler

/* Ensure Cortex-M port compatibility. */
#define SysTick_Handler                           xPortSysTickHandler

#endif /* FREERTOS_CONFIG_H */
