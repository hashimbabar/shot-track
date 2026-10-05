/*
 * FreeRTOSConfig.h - kernel settings for shottrack on the STM32L432KC.
 *
 * Every FreeRTOS project has one of these. The kernel source is generic;
 * this file tells it how fast the CPU runs, how often to tick, how much RAM
 * it may use, and which interrupt priorities it is allowed to mask.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* SystemCoreClock is a C variable, but this header is also pulled into
 * assembly files, so only declare it when compiling C. */
#if defined(__GNUC__) && !defined(__ASSEMBLER__)
#include <stdint.h>
extern uint32_t SystemCoreClock;
void fatal_error(const char *what);
#endif

/* ---- scheduler ---- */
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configCPU_CLOCK_HZ                      (SystemCoreClock) /* 80 MHz after clock setup */
#define configTICK_RATE_HZ                      1000  /* 1 ms tick, same as the HAL tick */
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                128   /* in words (4 bytes each), so 512 bytes */
#define configMAX_TASK_NAME_LEN                 12
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TIME_SLICING                  1

/* ---- memory ---- */
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0
#define configTOTAL_HEAP_SIZE                   (20 * 1024) /* out of 64 KB of SRAM: task stacks + queues */

/* ---- features ---- */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               0
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_CO_ROUTINES                   0

/* Software timers are not used (the 200 Hz tick is a hardware timer), so
 * no timer service task is created. */
#define configUSE_TIMERS                        0

/* ---- hooks and safety checks ---- */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            1 /* calls vApplicationMallocFailedHook() */
#define configCHECK_FOR_STACK_OVERFLOW          2 /* calls vApplicationStackOverflowHook() */
#define configASSERT(x)                         do { if (!(x)) fatal_error("configASSERT"); } while (0)

/* ---- interrupt priorities ----
 * On Cortex-M a LOWER number means a MORE urgent interrupt. The STM32L4
 * implements 4 priority bits, so priorities go from 0 (most urgent) to 15.
 *
 * - The kernel's own interrupts (SysTick, PendSV) run at 15, the least urgent.
 * - Any interrupt that calls a FreeRTOS "FromISR" function must have a
 *   priority number of 5 or higher. Interrupts at 0-4 are never masked by
 *   the kernel, but must not touch FreeRTOS at all. */
#define configPRIO_BITS                              4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY      (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* ---- API functions to include ---- */
#define INCLUDE_vTaskPrioritySet                0
#define INCLUDE_uxTaskPriorityGet               0
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_uxTaskGetStackHighWaterMark     1

/* ---- map the kernel's handlers onto the names in the vector table ----
 * The startup file's vector table points at SVC_Handler and
 * PendSV_Handler. Renaming the port's functions makes them land there.
 * SysTick_Handler is written by hand in stm32l4xx_it.c because the HAL
 * tick needs it as well. */
#define vPortSVCHandler    SVC_Handler
#define xPortPendSVHandler PendSV_Handler

#endif /* FREERTOS_CONFIG_H */
