/*
 * task_ui.c - button, LED and the once-a-second stats line.
 *
 * LED:
 *   fast blink (5 Hz)   IMU not responding
 *   slow blink (1 Hz)   ready, not logging
 *   solid on            logging a session
 *   short flash         a shot was just detected (inverts the LED briefly)
 *
 * Button: starts or stops a logging session.
 */
#include "app.h"
#include "board.h"

#define LOOP_MS          20
#define DEBOUNCE_MS      30
#define BUTTON_LOCKOUT_MS 300
#define SHOT_FLASH_MS    150
#define STATS_PERIOD_MS  1000

void app_button_isr(void)
{
    BaseType_t woke = pdFALSE;

    __HAL_GPIO_EXTI_CLEAR_IT(BUTTON_PIN);
    xTaskNotifyFromISR(g_ui_task, UI_EVT_BUTTON, eSetBits, &woke);
    portYIELD_FROM_ISR(woke);
}

static void print_stats(void)
{
    console_printf("samples %lu missed %lu i2c_err %lu | shots %lu | "
                   "log %s rows %lu q_full %lu sd_err %lu\r\n",
                   (unsigned long)g_stats.samples,
                   (unsigned long)g_stats.missed_ticks,
                   (unsigned long)g_stats.i2c_errors,
                   (unsigned long)g_stats.shots,
                   g_stats.logging ? "on" : "off",
                   (unsigned long)g_stats.log_rows,
                   (unsigned long)(g_stats.log_q_full + g_stats.shot_log_drops),
                   (unsigned long)g_stats.sd_errors);
}

void ui_task(void *arg)
{
    TickType_t flash_until = 0;
    TickType_t button_ignore_until = 0;
    TickType_t next_stats = xTaskGetTickCount() + pdMS_TO_TICKS(STATS_PERIOD_MS);

    (void)arg;

    /* Safe to enable now: this task exists to receive the notification. */
    HAL_NVIC_EnableIRQ(BUTTON_IRQn);

    for (;;) {
        uint32_t events = 0;
        TickType_t now;
        bool base, on;

        /* Wake on an event, or every 20 ms to update the LED. */
        xTaskNotifyWait(0, UINT32_MAX, &events, pdMS_TO_TICKS(LOOP_MS));
        now = xTaskGetTickCount();

        if ((events & UI_EVT_BUTTON) && (int32_t)(now - button_ignore_until) >= 0) {
            /* A mechanical button bounces for a few ms. Wait, then check it
             * is still pressed before acting on it. */
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));
            if (button_is_pressed()) {
                xTaskNotify(g_logger_task, LOG_CMD_TOGGLE, eSetBits);
                button_ignore_until = now + pdMS_TO_TICKS(BUTTON_LOCKOUT_MS);
            }
        }
        if (events & UI_EVT_SHOT) {
            flash_until = now + pdMS_TO_TICKS(SHOT_FLASH_MS);
        }

        if (!g_stats.imu_ready) {
            base = ((now / pdMS_TO_TICKS(100)) & 1u) != 0;   /* 5 Hz */
        } else if (g_stats.logging) {
            base = true;
        } else {
            base = ((now / pdMS_TO_TICKS(500)) & 1u) != 0;   /* 1 Hz */
        }
        on = base;
        if ((int32_t)(flash_until - now) > 0) {
            on = !base;
        }
        led_set(on);

        if ((int32_t)(now - next_stats) >= 0) {
            next_stats += pdMS_TO_TICKS(STATS_PERIOD_MS);
            print_stats();
        }
    }
}
