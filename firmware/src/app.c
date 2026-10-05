/*
 * app.c - creates the queues and tasks, plus the shared console.
 */
#include <stdarg.h>
#include <stdio.h>
#include "app.h"
#include "board.h"
#include "semphr.h"

app_stats_t g_stats;
QueueHandle_t g_detect_q;
QueueHandle_t g_log_q;
TaskHandle_t g_sampling_task;
TaskHandle_t g_ui_task;
TaskHandle_t g_logger_task;
mpu6050_t g_imu;

static SemaphoreHandle_t console_mutex;

static void create_task(TaskFunction_t fn, const char *name, uint16_t stack,
                        UBaseType_t prio, TaskHandle_t *handle)
{
    if (xTaskCreate(fn, name, stack, NULL, prio, handle) != pdPASS) {
        fatal_error("xTaskCreate failed (heap too small?)");
    }
}

void app_start(void)
{
    console_mutex = xSemaphoreCreateMutex();
    g_detect_q = xQueueCreate(DETECT_Q_LEN, sizeof(imu_sample_t));
    g_log_q = xQueueCreate(LOG_Q_LEN, sizeof(log_record_t));
    if (console_mutex == NULL || g_detect_q == NULL || g_log_q == NULL) {
        fatal_error("queue/mutex create failed");
    }

    create_task(sampling_task, "sample", STACK_SAMPLING, PRIO_SAMPLING, &g_sampling_task);
    create_task(detect_task, "detect", STACK_DETECT, PRIO_DETECT, NULL);
    create_task(ui_task, "ui", STACK_UI, PRIO_UI, &g_ui_task);
    create_task(logger_task, "logger", STACK_LOGGER, PRIO_LOGGER, &g_logger_task);
}

void console_printf(const char *fmt, ...)
{
    /* static: keeps the 128-byte buffer off every caller's stack. Safe
     * because only the mutex holder touches it. */
    static char buf[128];
    va_list ap;
    int n;

    xSemaphoreTake(console_mutex, portMAX_DELAY);
    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) {
        if (n >= (int)sizeof(buf)) {
            n = sizeof(buf) - 1; /* truncated */
        }
        HAL_UART_Transmit(&huart2, (uint8_t *)buf, (uint16_t)n, HAL_MAX_DELAY);
    }
    xSemaphoreGive(console_mutex);
}
