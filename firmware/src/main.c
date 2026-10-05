/*
 * main.c - ShotTrack firmware entry point.
 *
 * Sets up the clock and peripherals, prints a banner, creates the
 * FreeRTOS tasks (app.c) and starts the scheduler. After that, everything
 * happens in the tasks:
 *
 *   sampling (prio 4)  200 Hz IMU reads, woken by the TIM6 interrupt
 *   detect   (prio 3)  shot detection on each sample
 *   ui       (prio 2)  button, LED, stats line on the serial port
 *   logger   (prio 1)  CSV to the microSD card
 *
 * See docs/architecture.md for the diagrams.
 */
#include <stdio.h>
#include "app.h"
#include "board.h"

int main(void)
{
    bool lse_ok;

    /* Resets peripherals, sets up the flash interface, the interrupt
     * priority grouping FreeRTOS expects, and a 1 ms SysTick. */
    HAL_Init();

    lse_ok = board_clock_init();
    board_gpio_init();
    board_uart_init();
    board_i2c_init();
    board_spi_init();
    board_timer_init();

    /* Turn off stdout buffering so printf output goes out immediately. */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Plain printf is fine here: no tasks exist yet. Once the scheduler
     * runs, tasks use console_printf(), which takes a mutex. */
    printf("\r\nShotTrack firmware\r\n");
    printf("system clock: %lu Hz\r\n", (unsigned long)SystemCoreClock);
    printf("32 kHz crystal: %s\r\n", lse_ok ? "ok" : "FAILED (clock is less accurate)");

    app_start();
    printf("starting scheduler, free heap %u bytes\r\n", (unsigned)xPortGetFreeHeapSize());
    vTaskStartScheduler();

    /* Only reached if there wasn't enough heap for the idle task. */
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
