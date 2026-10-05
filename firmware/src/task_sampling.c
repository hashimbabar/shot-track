/*
 * task_sampling.c - reads the IMU at exactly 200 Hz.
 *
 * TIM6 interrupts every 5 ms. The interrupt does almost nothing: it just
 * wakes this task with a task notification. The I2C read happens here, in
 * a task, because it takes ~0.4 ms and blocking that long inside an
 * interrupt would delay every other interrupt.
 *
 * This is the highest-priority task, so when the timer wakes it, it runs
 * straight away, whatever the other tasks are doing.
 */
#include "app.h"
#include "board.h"

/* Same settings the detector and the simulator assume. */
static const mpu6050_config_t imu_cfg = {
    .accel_range = MPU6050_ACCEL_16G,  /* wrist flicks can exceed 8 g */
    .gyro_range = MPU6050_GYRO_2000DPS,/* a wrist snap is fast */
    .dlpf = MPU6050_DLPF_44HZ,         /* below the 100 Hz Nyquist limit of 200 Hz sampling */
    .sample_rate_div = 4,              /* 1 kHz / (1 + 4) = 200 Hz inside the sensor */
};

/* After this many failed reads in a row, reset the bus and the sensor
 * (a loose jumper wire, for example). */
#define MAX_CONSECUTIVE_I2C_ERRORS 10

void app_sample_timer_isr(void)
{
    BaseType_t woke = pdFALSE;

    __HAL_TIM_CLEAR_IT(&htim6, TIM_IT_UPDATE);
    /* Adds 1 to the task's notification count. If the task hasn't
     * collected the last one yet, the count goes to 2, and the task can
     * tell that it missed a sample. */
    vTaskNotifyGiveFromISR(g_sampling_task, &woke);
    /* If the sampling task is now the highest-priority ready task, switch
     * to it as soon as this interrupt returns, not at the next tick. */
    portYIELD_FROM_ISR(woke);
}

/* Keep trying until the IMU answers, printing why it didn't. */
static void imu_bring_up(void)
{
    for (;;) {
        mpu6050_err_t err = mpu6050_init(&g_imu, &g_imu_bus, MPU6050_ADDR_AD0_LOW, &imu_cfg);
        if (err == MPU6050_OK) {
            console_printf("imu: ok (+-16 g, +-2000 dps, 200 Hz)\r\n");
            g_stats.imu_ready = true;
            return;
        }
        console_printf("imu: init failed: %s, retrying in 1 s\r\n", mpu6050_strerror(err));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void sampling_task(void *arg)
{
    uint32_t seq = 0;
    uint32_t bad_reads = 0;

    (void)arg;

    imu_bring_up();
    if (HAL_TIM_Base_Start_IT(&htim6) != HAL_OK) {
        fatal_error("sample timer start failed");
    }

    for (;;) {
        imu_sample_t s;
        log_record_t rec;

        /* Sleep until the timer fires. pdTRUE clears the count and returns
         * how many ticks had piled up. The 100 ms timeout only matters if
         * the timer stops. */
        uint32_t ticks = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
        if (ticks == 0) {
            continue;
        }
        if (ticks > 1) {
            g_stats.missed_ticks += ticks - 1;
        }
        seq += ticks;

        HAL_GPIO_WritePin(DBG_GPIO_PORT, DBG_SAMPLE_PIN, GPIO_PIN_SET);
        mpu6050_err_t err = mpu6050_read_raw(&g_imu, &s.raw);
        HAL_GPIO_WritePin(DBG_GPIO_PORT, DBG_SAMPLE_PIN, GPIO_PIN_RESET);

        if (err != MPU6050_OK) {
            g_stats.i2c_errors++;
            if (++bad_reads >= MAX_CONSECUTIVE_I2C_ERRORS) {
                console_printf("imu: %u reads failed in a row, resetting\r\n",
                               (unsigned)bad_reads);
                g_stats.imu_ready = false;
                HAL_TIM_Base_Stop_IT(&htim6);
                HAL_I2C_DeInit(&hi2c1);
                board_i2c_init();
                imu_bring_up();
                bad_reads = 0;
                HAL_TIM_Base_Start_IT(&htim6);
            }
            continue;
        }
        bad_reads = 0;

        s.seq = seq;
        s.t_ms = seq * SAMPLE_PERIOD_MS;
        g_stats.samples++;

        /* Never block here: a full queue means a slower task is behind,
         * and waiting for it would make us miss the next timer tick.
         * Count the loss instead, so it shows up in the stats. */
        if (xQueueSend(g_detect_q, &s, 0) != pdTRUE) {
            g_stats.detect_q_full++;
        }
        if (g_stats.logging) {
            rec.type = LOG_REC_SAMPLE;
            rec.u.sample = s;
            if (xQueueSend(g_log_q, &rec, 0) != pdTRUE) {
                g_stats.log_q_full++;
            }
        }
    }
}
