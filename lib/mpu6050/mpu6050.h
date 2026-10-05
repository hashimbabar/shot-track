/*
 * mpu6050.h - small driver for the InvenSense MPU-6050 6-axis IMU.
 *
 * The driver never touches hardware directly. It talks through an
 * mpu6050_i2c_t: three function pointers that the caller fills in. On the
 * STM32 they wrap HAL_I2C_Mem_Read/Write; in the unit tests they point at
 * a fake register file, so the driver can be tested on a PC.
 */
#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 7-bit I2C addresses, set by the AD0 pin. */
#define MPU6050_ADDR_AD0_LOW   0x68
#define MPU6050_ADDR_AD0_HIGH  0x69

typedef enum {
    MPU6050_OK = 0,
    MPU6050_ERR_I2C = -1,       /* bus error or NACK */
    MPU6050_ERR_WHO_AM_I = -2,  /* something answered, but it isn't an MPU-6050 */
    MPU6050_ERR_PARAM = -3      /* bad argument passed to the driver */
} mpu6050_err_t;

/*
 * Bus interface. read/write return 0 on success, anything else on failure
 * (e.g. the device didn't ACK). dev_addr is the 7-bit address.
 */
typedef struct {
    int (*write)(void *ctx, uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len);
    int (*read)(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len);
    void (*delay_ms)(void *ctx, uint32_t ms);
    void *ctx; /* passed back to every call, e.g. an I2C handle */
} mpu6050_i2c_t;

/* Enum values are the register field values, so they can be written
 * straight into the register. */
typedef enum {
    MPU6050_ACCEL_2G = 0,
    MPU6050_ACCEL_4G = 1,
    MPU6050_ACCEL_8G = 2,
    MPU6050_ACCEL_16G = 3
} mpu6050_accel_range_t;

typedef enum {
    MPU6050_GYRO_250DPS = 0,
    MPU6050_GYRO_500DPS = 1,
    MPU6050_GYRO_1000DPS = 2,
    MPU6050_GYRO_2000DPS = 3
} mpu6050_gyro_range_t;

/* Accelerometer bandwidth from the RM 4.3 table (the gyro's is close). */
typedef enum {
    MPU6050_DLPF_260HZ = 0, /* turns the filter off; gyro runs at 8 kHz */
    MPU6050_DLPF_184HZ = 1,
    MPU6050_DLPF_94HZ = 2,
    MPU6050_DLPF_44HZ = 3,
    MPU6050_DLPF_21HZ = 4,
    MPU6050_DLPF_10HZ = 5,
    MPU6050_DLPF_5HZ = 6
} mpu6050_dlpf_t;

typedef struct {
    mpu6050_accel_range_t accel_range;
    mpu6050_gyro_range_t gyro_range;
    mpu6050_dlpf_t dlpf;
    uint8_t sample_rate_div; /* rate = 1 kHz / (1 + div) with the DLPF on */
} mpu6050_config_t;

typedef struct {
    const mpu6050_i2c_t *i2c;
    uint8_t addr;
    float accel_lsb_per_g;
    float gyro_lsb_per_dps;
} mpu6050_t;

/* One sample in raw sensor counts. */
typedef struct {
    int16_t ax, ay, az;
    int16_t temp;
    int16_t gx, gy, gz;
} mpu6050_raw_t;

/*
 * Check WHO_AM_I, reset the chip, wake it up and apply cfg.
 * Nothing is written if the WHO_AM_I check fails.
 */
mpu6050_err_t mpu6050_init(mpu6050_t *dev, const mpu6050_i2c_t *i2c,
                           uint8_t addr, const mpu6050_config_t *cfg);

/* Read accel, temperature and gyro in one 14-byte burst. */
mpu6050_err_t mpu6050_read_raw(const mpu6050_t *dev, mpu6050_raw_t *out);

/* Turn the 14 burst bytes into numbers. Split out so it can be tested
 * on its own. */
void mpu6050_parse_burst(const uint8_t buf[14], mpu6050_raw_t *out);

float mpu6050_accel_g(const mpu6050_t *dev, int16_t raw);
float mpu6050_gyro_dps(const mpu6050_t *dev, int16_t raw);
float mpu6050_temp_c(int16_t raw);

const char *mpu6050_strerror(mpu6050_err_t err);

#ifdef __cplusplus
}
#endif

#endif /* MPU6050_H */
