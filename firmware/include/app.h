/*
 * app.h - the FreeRTOS application: tasks, queues and what flows between
 * them. The diagram in docs/architecture.md shows the same thing.
 *
 *   TIM6 ISR (200 Hz) --notify--> sampling task --detect_q--> detect task
 *                                     |                         |
 *                                     +------log_q----+---------+ (shot events)
 *                                                     v
 *   button ISR --notify--> ui task --notify--> logger task --> SD card
 */
#ifndef APP_H
#define APP_H

#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "mpu6050.h"

/* Task priorities: higher number = more urgent. The order matters:
 * sampling must never wait behind the SD card, so logging is lowest. */
#define PRIO_SAMPLING   4
#define PRIO_DETECT     3
#define PRIO_UI         2
#define PRIO_LOGGER     1

/* Stack sizes in words (4 bytes). printf/snprintf and FatFs are the big
 * users, so the UI and logger tasks get more. */
#define STACK_SAMPLING  256
#define STACK_DETECT    256
#define STACK_UI        384
#define STACK_LOGGER    512

/* Queue lengths. log_q is the buffer that rides out slow SD writes:
 * 128 samples at 200 Hz = 640 ms. SD cards can stall for hundreds of ms
 * while erasing internally. Sized from that, not measured yet. */
#define DETECT_Q_LEN    16
#define LOG_Q_LEN       128

/* One IMU sample as it leaves the sampling task. */
typedef struct {
    uint32_t seq;               /* timer tick number; gaps mean missed samples */
    uint32_t t_ms;              /* seq * 5 ms */
    mpu6050_raw_t raw;
} imu_sample_t;

/* What the logger receives: a sample row, or a shot event. */
typedef enum {
    LOG_REC_SAMPLE,
    LOG_REC_SHOT
} log_rec_type_t;

typedef struct {
    log_rec_type_t type;
    union {
        imu_sample_t sample;
        struct {
            uint32_t t_ms;
            uint16_t peak_dps;
            uint16_t peak_mg;   /* milli-g, to avoid printing floats */
        } shot;
    } u;
} log_record_t;

/* Task notification bits. */
#define UI_EVT_BUTTON   (1u << 0)
#define UI_EVT_SHOT     (1u << 1)
#define LOG_CMD_TOGGLE  (1u << 0)

/*
 * Counters for the stats line printed once a second. Each field has
 * exactly one task that writes it; others only read. 32-bit reads and
 * writes are single instructions on the Cortex-M4, so a reader can see
 * a slightly old value but never a half-written one. That's fine for
 * stats. Anything that needs every value in order goes through a queue.
 */
typedef struct {
    volatile uint32_t samples;        /* sampling task */
    volatile uint32_t missed_ticks;   /* sampling task: timer fired again before we read */
    volatile uint32_t i2c_errors;     /* sampling task */
    volatile uint32_t detect_q_full;  /* sampling task */
    volatile uint32_t log_q_full;     /* sampling task: samples the logger lost */
    volatile uint32_t shots;          /* detect task */
    volatile uint32_t shot_log_drops; /* detect task: shot events the logger lost */
    volatile uint32_t log_rows;       /* logger task */
    volatile uint32_t sd_errors;      /* logger task */
    volatile bool imu_ready;          /* sampling task */
    volatile bool logging;            /* logger task */
} app_stats_t;

extern app_stats_t g_stats;
extern QueueHandle_t g_detect_q;
extern QueueHandle_t g_log_q;
extern TaskHandle_t g_sampling_task;
extern TaskHandle_t g_ui_task;
extern TaskHandle_t g_logger_task;

/* The IMU driver handle. Written once by the sampling task before the
 * sample timer starts; read-only after that (the detect task uses its
 * scale factors). */
extern mpu6050_t g_imu;

/* Create queues and tasks. Called from main() before the scheduler. */
void app_start(void);

void sampling_task(void *arg);
void detect_task(void *arg);
void ui_task(void *arg);
void logger_task(void *arg);

/* Called from interrupt handlers in stm32l4xx_it.c. */
void app_sample_timer_isr(void);
void app_button_isr(void);

/* Thread-safe printf: one task's line can't get mixed into another's. */
void console_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* mpu6050_i2c_t that talks to the real I2C1 peripheral (i2c_bus.c). */
extern const mpu6050_i2c_t g_imu_bus;

#endif /* APP_H */
