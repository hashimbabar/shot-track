/*
 * mpu6050.c - MPU-6050 driver. See mpu6050.h for the interface and
 * mpu6050_regs.h for the register map references.
 */
#include <stddef.h>
#include "mpu6050.h"
#include "mpu6050_regs.h"

/* Product spec PS-MPU-6000A-00 rev 3.4, section 6.2 (accel sensitivity),
 * indexed by AFS_SEL. */
static const float accel_lsb_per_g[] = {16384.0f, 8192.0f, 4096.0f, 2048.0f};

/* Product spec section 6.1 (gyro sensitivity), indexed by FS_SEL. */
static const float gyro_lsb_per_dps[] = {131.0f, 65.5f, 32.8f, 16.4f};

/* The register map gives no reset time. 100 ms is the delay commonly used
 * and is far longer than the chip needs, which is fine once at startup. */
#define RESET_DELAY_MS 100

static mpu6050_err_t write_reg(const mpu6050_t *dev, uint8_t reg, uint8_t value)
{
    if (dev->i2c->write(dev->i2c->ctx, dev->addr, reg, &value, 1) != 0) {
        return MPU6050_ERR_I2C;
    }
    return MPU6050_OK;
}

static mpu6050_err_t read_regs(const mpu6050_t *dev, uint8_t reg, uint8_t *buf, uint16_t len)
{
    if (dev->i2c->read(dev->i2c->ctx, dev->addr, reg, buf, len) != 0) {
        return MPU6050_ERR_I2C;
    }
    return MPU6050_OK;
}

mpu6050_err_t mpu6050_init(mpu6050_t *dev, const mpu6050_i2c_t *i2c,
                           uint8_t addr, const mpu6050_config_t *cfg)
{
    mpu6050_err_t err;
    uint8_t id;

    if (dev == NULL || i2c == NULL || cfg == NULL ||
        i2c->read == NULL || i2c->write == NULL || i2c->delay_ms == NULL ||
        (unsigned)cfg->accel_range > MPU6050_ACCEL_16G ||
        (unsigned)cfg->gyro_range > MPU6050_GYRO_2000DPS ||
        (unsigned)cfg->dlpf > MPU6050_DLPF_5HZ) {
        return MPU6050_ERR_PARAM;
    }

    dev->i2c = i2c;
    dev->addr = addr;

    /* 1. Make sure the right chip is there before writing anything. */
    err = read_regs(dev, MPU6050_REG_WHO_AM_I, &id, 1);
    if (err != MPU6050_OK) {
        return err;
    }
    if (id != MPU6050_WHO_AM_I_VALUE) {
        return MPU6050_ERR_WHO_AM_I;
    }

    /* 2. Reset so we start from known register values, even if the MCU
     *    was reset but the sensor kept its power. */
    err = write_reg(dev, MPU6050_REG_PWR_MGMT_1, MPU6050_PWR1_DEVICE_RESET);
    if (err != MPU6050_OK) {
        return err;
    }
    i2c->delay_ms(i2c->ctx, RESET_DELAY_MS);

    /* 3. Wake up (SLEEP = 0) and clock from the X gyro PLL. */
    err = write_reg(dev, MPU6050_REG_PWR_MGMT_1, MPU6050_PWR1_CLKSEL_PLL_XG);
    if (err != MPU6050_OK) {
        return err;
    }

    /* 4. Filter, sample rate and ranges. The DLPF goes first because it
     *    decides whether the divider counts from 1 kHz or 8 kHz. */
    err = write_reg(dev, MPU6050_REG_CONFIG, (uint8_t)cfg->dlpf & MPU6050_DLPF_CFG_MASK);
    if (err != MPU6050_OK) {
        return err;
    }
    err = write_reg(dev, MPU6050_REG_SMPLRT_DIV, cfg->sample_rate_div);
    if (err != MPU6050_OK) {
        return err;
    }
    err = write_reg(dev, MPU6050_REG_GYRO_CONFIG, (uint8_t)(cfg->gyro_range << MPU6050_FS_SEL_SHIFT));
    if (err != MPU6050_OK) {
        return err;
    }
    err = write_reg(dev, MPU6050_REG_ACCEL_CONFIG, (uint8_t)(cfg->accel_range << MPU6050_AFS_SEL_SHIFT));
    if (err != MPU6050_OK) {
        return err;
    }

    dev->accel_lsb_per_g = accel_lsb_per_g[cfg->accel_range];
    dev->gyro_lsb_per_dps = gyro_lsb_per_dps[cfg->gyro_range];
    return MPU6050_OK;
}

static int16_t be16(const uint8_t *p)
{
    /* High byte first. Build it unsigned, then convert, so the shift never
     * touches a negative number. */
    return (int16_t)(uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

void mpu6050_parse_burst(const uint8_t buf[14], mpu6050_raw_t *out)
{
    out->ax = be16(&buf[0]);
    out->ay = be16(&buf[2]);
    out->az = be16(&buf[4]);
    out->temp = be16(&buf[6]);
    out->gx = be16(&buf[8]);
    out->gy = be16(&buf[10]);
    out->gz = be16(&buf[12]);
}

mpu6050_err_t mpu6050_read_raw(const mpu6050_t *dev, mpu6050_raw_t *out)
{
    uint8_t buf[MPU6050_BURST_LEN];
    mpu6050_err_t err;

    if (dev == NULL || dev->i2c == NULL || out == NULL) {
        return MPU6050_ERR_PARAM;
    }

    /* One transaction for all 14 bytes. Reading the axes one at a time
     * could mix values from two different samples. */
    err = read_regs(dev, MPU6050_REG_ACCEL_XOUT_H, buf, MPU6050_BURST_LEN);
    if (err != MPU6050_OK) {
        return err;
    }
    mpu6050_parse_burst(buf, out);
    return MPU6050_OK;
}

float mpu6050_accel_g(const mpu6050_t *dev, int16_t raw)
{
    return (float)raw / dev->accel_lsb_per_g;
}

float mpu6050_gyro_dps(const mpu6050_t *dev, int16_t raw)
{
    return (float)raw / dev->gyro_lsb_per_dps;
}

/* RM 4.18: temperature in deg C = raw / 340 + 36.53 */
float mpu6050_temp_c(int16_t raw)
{
    return (float)raw / 340.0f + 36.53f;
}

const char *mpu6050_strerror(mpu6050_err_t err)
{
    switch (err) {
    case MPU6050_OK:           return "ok";
    case MPU6050_ERR_I2C:      return "i2c error (no ack?)";
    case MPU6050_ERR_WHO_AM_I: return "wrong WHO_AM_I";
    case MPU6050_ERR_PARAM:    return "bad parameter";
    }
    return "unknown error";
}
