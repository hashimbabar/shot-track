/*
 * i2c_bus.c - connects the MPU-6050 driver's bus interface to the STM32
 * HAL. This is the only file that knows the driver runs on an STM32.
 */
#include "app.h"
#include "board.h"

/* Long enough for a 14-byte read at 400 kHz (~0.4 ms) with plenty of
 * margin, short enough that a missing sensor doesn't stall sampling. */
#define I2C_TIMEOUT_MS 5

/* The HAL wants the 7-bit address shifted left by one (the R/W bit). */
static int hal_write(void *ctx, uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len)
{
    I2C_HandleTypeDef *h = (I2C_HandleTypeDef *)ctx;
    return HAL_I2C_Mem_Write(h, (uint16_t)(dev_addr << 1), reg, I2C_MEMADD_SIZE_8BIT,
                             (uint8_t *)data, len, I2C_TIMEOUT_MS) == HAL_OK ? 0 : -1;
}

static int hal_read(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len)
{
    I2C_HandleTypeDef *h = (I2C_HandleTypeDef *)ctx;
    return HAL_I2C_Mem_Read(h, (uint16_t)(dev_addr << 1), reg, I2C_MEMADD_SIZE_8BIT,
                            data, len, I2C_TIMEOUT_MS) == HAL_OK ? 0 : -1;
}

/* vTaskDelay instead of HAL_Delay: the 100 ms reset wait lets other tasks
 * run instead of spinning. Only valid once the scheduler is running,
 * which is true because the IMU is set up from the sampling task. */
static void rtos_delay(void *ctx, uint32_t ms)
{
    (void)ctx;
    vTaskDelay(pdMS_TO_TICKS(ms));
}

const mpu6050_i2c_t g_imu_bus = {
    .write = hal_write,
    .read = hal_read,
    .delay_ms = rtos_delay,
    .ctx = &hi2c1,
};
