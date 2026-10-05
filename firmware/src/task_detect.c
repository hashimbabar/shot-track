/*
 * task_detect.c - runs lib/detect on every sample.
 *
 * All the detection logic is in lib/detect (plain C, unit tested on the
 * PC). This task only converts units, feeds samples in, and passes shots
 * on to the UI (LED flash) and the logger.
 */
#include "app.h"
#include "board.h"
#include "detect.h"

void detect_task(void *arg)
{
    static detect_t det; /* static: keeps it off the task stack */
    imu_sample_t s;

    (void)arg;
    detect_init(&det);

    for (;;) {
        detect_sample_t in;
        detect_shot_t shot;

        xQueueReceive(g_detect_q, &s, portMAX_DELAY);

        in.t_ms = s.t_ms;
        in.ax_g = mpu6050_accel_g(&g_imu, s.raw.ax);
        in.ay_g = mpu6050_accel_g(&g_imu, s.raw.ay);
        in.az_g = mpu6050_accel_g(&g_imu, s.raw.az);
        in.gx_dps = mpu6050_gyro_dps(&g_imu, s.raw.gx);
        in.gy_dps = mpu6050_gyro_dps(&g_imu, s.raw.gy);
        in.gz_dps = mpu6050_gyro_dps(&g_imu, s.raw.gz);

        if (!detect_update(&det, &in, &shot)) {
            continue;
        }

        g_stats.shots = detect_shot_count(&det);
        xTaskNotify(g_ui_task, UI_EVT_SHOT, eSetBits);

        if (g_stats.logging) {
            log_record_t rec;
            rec.type = LOG_REC_SHOT;
            rec.u.shot.t_ms = shot.t_ms;
            rec.u.shot.peak_dps = (uint16_t)shot.peak_gyro_dps;
            rec.u.shot.peak_mg = (uint16_t)(shot.peak_accel_g * 1000.0f);
            if (xQueueSend(g_log_q, &rec, 0) != pdTRUE) {
                g_stats.shot_log_drops++;
            }
        }
    }
}
