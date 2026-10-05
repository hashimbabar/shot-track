/*
 * mpu6050_regs.h - MPU-6050 register addresses and bit fields.
 *
 * Source: InvenSense "MPU-6000 and MPU-6050 Register Map and Descriptions",
 * RM-MPU-6000A-00 rev 4.2. Section numbers below refer to that document
 * ("RM 4.x"). Register numbers are given in decimal too, because that is
 * how the register map titles them ("Register 107 - Power Management 1").
 *
 * Only the registers this driver uses are listed.
 */
#ifndef MPU6050_REGS_H
#define MPU6050_REGS_H

/* RM 4.2, Register 25 - Sample Rate Divider.
 * Sample rate = gyro output rate / (1 + SMPLRT_DIV).
 * Gyro output rate is 1 kHz when the DLPF is on (DLPF_CFG 1..6),
 * 8 kHz when it is off (DLPF_CFG 0 or 7). */
#define MPU6050_REG_SMPLRT_DIV      0x19

/* RM 4.3, Register 26 - Configuration.
 * Bits 2:0 DLPF_CFG pick the digital low-pass filter bandwidth.
 * Bits 5:3 EXT_SYNC_SET (frame sync input) - left at 0 (disabled). */
#define MPU6050_REG_CONFIG          0x1A
#define MPU6050_DLPF_CFG_MASK       0x07

/* RM 4.4, Register 27 - Gyroscope Configuration.
 * Bits 4:3 FS_SEL: 0 = +-250, 1 = +-500, 2 = +-1000, 3 = +-2000 deg/s.
 * Bits 7:5 are self-test enables - left at 0. */
#define MPU6050_REG_GYRO_CONFIG     0x1B
#define MPU6050_FS_SEL_SHIFT        3

/* RM 4.5, Register 28 - Accelerometer Configuration.
 * Bits 4:3 AFS_SEL: 0 = +-2, 1 = +-4, 2 = +-8, 3 = +-16 g.
 * Bits 7:5 are self-test enables - left at 0. */
#define MPU6050_REG_ACCEL_CONFIG    0x1C
#define MPU6050_AFS_SEL_SHIFT       3

/* RM 4.17, Registers 59 to 64 - Accelerometer Measurements.
 * RM 4.18, Registers 65 and 66 - Temperature Measurement.
 * RM 4.19, Registers 67 to 72 - Gyroscope Measurements.
 * These 14 registers are consecutive, so one burst read starting at
 * ACCEL_XOUT_H returns all of them from the same sample instant:
 *   AX_H AX_L AY_H AY_L AZ_H AZ_L  T_H T_L  GX_H GX_L GY_H GY_L GZ_H GZ_L
 * Each value is a 16-bit two's complement number, high byte first. */
#define MPU6050_REG_ACCEL_XOUT_H    0x3B
#define MPU6050_BURST_LEN           14

/* RM 4.28, Register 107 - Power Management 1.
 * Bit 7 DEVICE_RESET: resets all registers to defaults, clears itself.
 * Bit 6 SLEEP: the chip powers up asleep (this bit is 1 after reset).
 * Bits 2:0 CLKSEL: 1 = PLL with X-axis gyro reference. The product spec
 * recommends a gyro reference over the internal 8 MHz oscillator for
 * better clock stability. */
#define MPU6050_REG_PWR_MGMT_1      0x6B
#define MPU6050_PWR1_DEVICE_RESET   0x80
#define MPU6050_PWR1_SLEEP          0x40
#define MPU6050_PWR1_CLKSEL_PLL_XG  0x01

/* RM 4.32, Register 117 - Who Am I.
 * Bits 6:1 hold the upper 6 bits of the I2C address, 0b110100. The
 * register reads 0x68 whatever the AD0 pin is set to. */
#define MPU6050_REG_WHO_AM_I        0x75
#define MPU6050_WHO_AM_I_VALUE      0x68

#endif /* MPU6050_REGS_H */
