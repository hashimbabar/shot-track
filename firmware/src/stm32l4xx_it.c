/*
 * stm32l4xx_it.c - interrupt and fault handlers.
 *
 * The startup file's vector table lists a handler name for every
 * interrupt. Each one defaults to an infinite loop unless a function with
 * that exact name is defined somewhere, like here.
 *
 * SVC_Handler and PendSV_Handler are defined by FreeRTOS's port.c (see the
 * renames at the bottom of FreeRTOSConfig.h).
 */
#include "app.h"
#include "board.h"
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS's tick handler, from port.c. */
extern void xPortSysTickHandler(void);

/*
 * SysTick fires every 1 ms and has two jobs:
 *  - HAL_IncTick() advances the HAL's millisecond counter, used by
 *    HAL_Delay(), HAL_GetTick() and every HAL timeout.
 *  - xPortSysTickHandler() is the FreeRTOS tick: it wakes tasks whose
 *    vTaskDelay() has expired and decides whether to switch tasks.
 *
 * The FreeRTOS part only runs once the scheduler has started. Before that,
 * the kernel has no tasks running and calling it would be an error.
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
}

void NMI_Handler(void)
{
    fatal_error("NMI");
}

/* Bad memory access, illegal instruction, divide by zero with trapping
 * on, stack overflow into invalid memory, and so on. */
void HardFault_Handler(void)
{
    fatal_error("HardFault");
}

void MemManage_Handler(void)
{
    fatal_error("MemManage fault");
}

void BusFault_Handler(void)
{
    fatal_error("BusFault");
}

void UsageFault_Handler(void)
{
    fatal_error("UsageFault");
}

void DebugMon_Handler(void)
{
}

/* ---- peripheral interrupts ----
 * Both are kept tiny: clear the flag and wake a task (app_*_isr). The
 * real work happens in the tasks. */

/* TIM6 update event, 200 times a second. The DAC shares this vector, but
 * the DAC is unused, so it is always the timer. */
void TIM6_DAC_IRQHandler(void)
{
    if (__HAL_TIM_GET_FLAG(&htim6, TIM_FLAG_UPDATE)) {
        app_sample_timer_isr();
    }
}

/* Button on PB0 -> EXTI line 0. */
void EXTI0_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(BUTTON_PIN)) {
        app_button_isr();
    }
}
