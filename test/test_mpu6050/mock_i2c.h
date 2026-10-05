/*
 * mock_i2c.h - a fake I2C bus for testing the MPU-6050 driver on a PC.
 *
 * The mock holds a 128-byte register file that reads come from, records
 * every write in order, and can be told to fail (NACK) on a chosen
 * transaction. It plugs into the driver through the same mpu6050_i2c_t
 * function pointers the real STM32 code uses.
 */
#ifndef MOCK_I2C_H
#define MOCK_I2C_H

#include <stdint.h>
#include "mpu6050.h"

#define MOCK_MAX_WRITES 32

typedef struct {
    uint8_t reg;
    uint8_t value;
} mock_write_t;

typedef struct {
    uint8_t regs[128];               /* what reads return */
    mock_write_t writes[MOCK_MAX_WRITES];
    int n_writes;

    int n_reads;
    uint8_t last_read_reg;
    uint16_t last_read_len;

    int n_transactions;              /* reads + writes, in order */
    int nack_on_transaction;         /* 1-based; 0 = never fail */

    uint8_t last_addr;
    uint32_t total_delay_ms;
} mock_i2c_t;

/* Reset the mock: register file zeroed, WHO_AM_I set to 0x68. */
void mock_i2c_reset(mock_i2c_t *m);

/* Fill in an mpu6050_i2c_t that routes calls to this mock. */
mpu6050_i2c_t mock_i2c_bus(mock_i2c_t *m);

#endif /* MOCK_I2C_H */
