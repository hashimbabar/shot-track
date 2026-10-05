/*
 * mock_i2c.c - fake I2C bus, see mock_i2c.h.
 */
#include <string.h>
#include "mock_i2c.h"

void mock_i2c_reset(mock_i2c_t *m)
{
    memset(m, 0, sizeof(*m));
    m->regs[0x75] = 0x68; /* WHO_AM_I of a real MPU-6050 */
}

/* Count the transaction and decide whether this one should fail. */
static int next_transaction_fails(mock_i2c_t *m)
{
    m->n_transactions++;
    return m->nack_on_transaction != 0 && m->n_transactions == m->nack_on_transaction;
}

static int mock_write(void *ctx, uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len)
{
    mock_i2c_t *m = (mock_i2c_t *)ctx;

    m->last_addr = dev_addr;
    if (next_transaction_fails(m)) {
        return -1; /* like HAL_I2C_Mem_Write returning HAL_ERROR on a NACK */
    }
    for (uint16_t i = 0; i < len; i++) {
        if (m->n_writes < MOCK_MAX_WRITES) {
            m->writes[m->n_writes].reg = (uint8_t)(reg + i);
            m->writes[m->n_writes].value = data[i];
            m->n_writes++;
        }
        m->regs[(reg + i) & 0x7F] = data[i];
    }
    return 0;
}

static int mock_read(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t *data, uint16_t len)
{
    mock_i2c_t *m = (mock_i2c_t *)ctx;

    m->last_addr = dev_addr;
    m->n_reads++;
    m->last_read_reg = reg;
    m->last_read_len = len;
    if (next_transaction_fails(m)) {
        return -1;
    }
    /* The real chip auto-increments the register address during a burst. */
    for (uint16_t i = 0; i < len; i++) {
        data[i] = m->regs[(reg + i) & 0x7F];
    }
    return 0;
}

static void mock_delay(void *ctx, uint32_t ms)
{
    ((mock_i2c_t *)ctx)->total_delay_ms += ms;
}

mpu6050_i2c_t mock_i2c_bus(mock_i2c_t *m)
{
    mpu6050_i2c_t bus = {
        .write = mock_write,
        .read = mock_read,
        .delay_ms = mock_delay,
        .ctx = m,
    };
    return bus;
}
