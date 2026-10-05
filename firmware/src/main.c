/*
 * main.c - shottrack firmware, phase 1 step 1: bring-up.
 *
 * Sets up the clock, LED and serial port, prints a hello message, then
 * starts FreeRTOS with two tasks:
 *   - blink:     toggles the green LED every 500 ms
 *   - heartbeat: prints the uptime and free heap once a second
 *
 * If both the LED and the serial output keep going, the toolchain, clock
 * setup, UART, interrupts and the RTOS scheduler all work.
 */
#include <stdio.h>
#include "board.h"
#include "FreeRTOS.h"
#include "task.h"

#define BLINK_PERIOD_MS     500
#define HEARTBEAT_PERIOD_MS 1000

static void blink_task(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        led_toggle();
        /* xTaskDelayUntil wakes at fixed 500 ms steps measured from the
         * last wake-up, so the period doesn't drift by however long the
         * loop body took. (vTaskDelay would wait 500 ms from *now*.) */
        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(BLINK_PERIOD_MS));
    }
}

static void heartbeat_task(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    unsigned long beat = 0;

    for (;;) {
        printf("beat %lu, uptime %lu ms, free heap %u bytes\r\n",
               beat++,
               (unsigned long)xTaskGetTickCount(),
               (unsigned)xPortGetFreeHeapSize());
        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(HEARTBEAT_PERIOD_MS));
    }
}

int main(void)
{
    bool lse_ok;

    /* Resets peripherals, sets up the flash interface, the interrupt
     * priority grouping FreeRTOS expects, and a 1 ms SysTick. */
    HAL_Init();

    lse_ok = board_clock_init();
    board_gpio_init();
    board_uart_init();

    /* By default the C library may hold printf output in a buffer until it
     * fills up. Turn buffering off so every printf goes out right away. */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Print before creating any tasks: once a task is created, interrupts
     * stay masked until the scheduler starts, so HAL_Delay() would hang. */
    printf("\r\nhello from shottrack\r\n");
    printf("system clock: %lu Hz\r\n", (unsigned long)SystemCoreClock);
    printf("32 kHz crystal: %s\r\n", lse_ok ? "ok" : "FAILED (clock is less accurate)");

    /* Stack sizes are in words (4 bytes). printf needs a fair amount of
     * stack, so heartbeat gets 4x the minimum. Both tasks get the same
     * low priority; nothing here is time-critical yet. */
    if (xTaskCreate(blink_task, "blink", configMINIMAL_STACK_SIZE, NULL,
                    tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
        fatal_error("could not create blink task");
    }
    if (xTaskCreate(heartbeat_task, "heartbeat", configMINIMAL_STACK_SIZE * 4, NULL,
                    tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
        fatal_error("could not create heartbeat task");
    }

    printf("starting scheduler\r\n");
    vTaskStartScheduler();

    /* Only reached if there wasn't enough heap for the idle/timer tasks. */
    fatal_error("scheduler returned");
}

/* ---- FreeRTOS hooks (turned on in FreeRTOSConfig.h) ---- */

/* A task wrote past the end of its stack. */
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    fatal_error("stack overflow");
}

/* pvPortMalloc() ran out of heap (configTOTAL_HEAP_SIZE). */
void vApplicationMallocFailedHook(void)
{
    fatal_error("out of FreeRTOS heap");
}
