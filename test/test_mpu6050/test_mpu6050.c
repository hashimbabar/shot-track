/*
 * test_mpu6050.c - unit tests for lib/mpu6050, run on the PC with
 * `pio test -e native`. The I2C bus is the fake one in mock_i2c.c.
 */
#include <unity.h>
#include "mpu6050.h"
#include "mpu6050_regs.h"
#include "mock_i2c.h"

static mock_i2c_t mock;
static mpu6050_i2c_t bus;
static mpu6050_t imu;

static const mpu6050_config_t cfg = {
    .accel_range = MPU6050_ACCEL_16G,
    .gyro_range = MPU6050_GYRO_2000DPS,
    .dlpf = MPU6050_DLPF_44HZ,
    .sample_rate_div = 4, /* 1 kHz / 5 = 200 Hz */
};

void setUp(void)
{
    mock_i2c_reset(&mock);
    bus = mock_i2c_bus(&mock);
}

void tearDown(void)
{
}

/* ---- init ---- */

void test_init_writes_registers_in_expected_order(void)
{
    const mock_write_t expected[] = {
        {MPU6050_REG_PWR_MGMT_1, 0x80},  /* reset */
        {MPU6050_REG_PWR_MGMT_1, 0x01},  /* wake, PLL with X gyro */
        {MPU6050_REG_CONFIG, 0x03},      /* DLPF 44 Hz */
        {MPU6050_REG_SMPLRT_DIV, 4},     /* 200 Hz */
        {MPU6050_REG_GYRO_CONFIG, 0x18}, /* FS_SEL = 3 -> +-2000 dps */
        {MPU6050_REG_ACCEL_CONFIG, 0x18} /* AFS_SEL = 3 -> +-16 g */
    };
    const int n = (int)(sizeof(expected) / sizeof(expected[0]));

    TEST_ASSERT_EQUAL_INT(MPU6050_OK, mpu6050_init(&imu, &bus, MPU6050_ADDR_AD0_LOW, &cfg));
    TEST_ASSERT_EQUAL_INT(n, mock.n_writes);
    for (int i = 0; i < n; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(expected[i].reg, mock.writes[i].reg, "register");
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(expected[i].value, mock.writes[i].value, "value");
    }
}

void test_init_checks_who_am_i_first(void)
{
    TEST_ASSERT_EQUAL_INT(MPU6050_OK, mpu6050_init(&imu, &bus, MPU6050_ADDR_AD0_LOW, &cfg));
    /* The only read during init is WHO_AM_I. */
    TEST_ASSERT_EQUAL_INT(1, mock.n_reads);
    TEST_ASSERT_EQUAL_HEX8(MPU6050_REG_WHO_AM_I, mock.last_read_reg);
}

void test_init_waits_after_reset(void)
{
    TEST_ASSERT_EQUAL_INT(MPU6050_OK, mpu6050_init(&imu, &bus, MPU6050_ADDR_AD0_LOW, &cfg));
    TEST_ASSERT_TRUE(mock.total_delay_ms >= 100);
}

void test_init_uses_given_address(void)
{
    TEST_ASSERT_EQUAL_INT(MPU6050_OK, mpu6050_init(&imu, &bus, MPU6050_ADDR_AD0_HIGH, &cfg));
    TEST_ASSERT_EQUAL_HEX8(0x69, mock.last_addr);
}

void test_init_sets_scale_factors(void)
{
    TEST_ASSERT_EQUAL_INT(MPU6050_OK, mpu6050_init(&imu, &bus, MPU6050_ADDR_AD0_LOW, &cfg));
    TEST_ASSERT_EQUAL_FLOAT(2048.0f, imu.accel_lsb_per_g);
    TEST_ASSERT_EQUAL_FLOAT(16.4f, imu.gyro_lsb_per_dps);
}

/* ---- error paths ---- */

void test_wrong_who_am_i_returns_error_and_writes_nothing(void)
{
    mock.regs[MPU6050_REG_WHO_AM_I] = 0x72; /* e.g. an MPU-6500 clone */
    TEST_ASSERT_EQUAL_INT(MPU6050_ERR_WHO_AM_I, mpu6050_init(&imu, &bus, MPU6050_ADDR_AD0_LOW, &cfg));
    TEST_ASSERT_EQUAL_INT(0, mock.n_writes);
}

void test_nack_on_who_am_i_returns_i2c_error(void)
{
    mock.nack_on_transaction = 1; /* nothing on the bus */
    TEST_ASSERT_EQUAL_INT(MPU6050_ERR_I2C, mpu6050_init(&imu, &bus, MPU6050_ADDR_AD0_LOW, &cfg));
    TEST_ASSERT_EQUAL_INT(0, mock.n_writes);
}

void test_nack_partway_through_init_returns_i2c_error(void)
{
    /* Transaction 1 = WHO_AM_I read, 2 = reset, 3 = wake, 4 = CONFIG. */
    mock.nack_on_transaction = 4;
    TEST_ASSERT_EQUAL_INT(MPU6050_ERR_I2C, mpu6050_init(&imu, &bus, MPU6050_ADDR_AD0_LOW, &cfg));
    /* It stopped at the failure instead of carrying on. */
    TEST_ASSERT_EQUAL_INT(4, mock.n_transactions);
}

void test_bad_config_returns_param_error(void)
{
    mpu6050_config_t bad = cfg;
    bad.gyro_range = (mpu6050_gyro_range_t)7;
    TEST_ASSERT_EQUAL_INT(MPU6050_ERR_PARAM, mpu6050_init(&imu, &bus, MPU6050_ADDR_AD0_LOW, &bad));
    TEST_ASSERT_EQUAL_INT(0, mock.n_transactions);
}

void test_null_arguments_return_param_error(void)
{
    TEST_ASSERT_EQUAL_INT(MPU6050_ERR_PARAM, mpu6050_init(NULL, &bus, MPU6050_ADDR_AD0_LOW, &cfg));
    TEST_ASSERT_EQUAL_INT(MPU6050_ERR_PARAM, mpu6050_init(&imu, NULL, MPU6050_ADDR_AD0_LOW, &cfg));
    TEST_ASSERT_EQUAL_INT(MPU6050_ERR_PARAM, mpu6050_init(&imu, &bus, MPU6050_ADDR_AD0_LOW, NULL));
}

/* ---- burst read ---- */

void test_read_raw_is_one_14_byte_burst_from_accel_xout_h(void)
{
    mpu6050_raw_t raw;

    TEST_ASSERT_EQUAL_INT(MPU6050_OK, mpu6050_init(&imu, &bus, MPU6050_ADDR_AD0_LOW, &cfg));
    mock.n_reads = 0;
    TEST_ASSERT_EQUAL_INT(MPU6050_OK, mpu6050_read_raw(&imu, &raw));
    TEST_ASSERT_EQUAL_INT(1, mock.n_reads);
    TEST_ASSERT_EQUAL_HEX8(0x3B, mock.last_read_reg);
    TEST_ASSERT_EQUAL_UINT16(14, mock.last_read_len);
}

void test_read_raw_parses_big_endian_signed_values(void)
{
    const uint8_t burst[14] = {
        0x08, 0x00, /* ax = +2048  (+1 g at +-16 g) */
        0xF8, 0x00, /* ay = -2048 */
        0x7F, 0xFF, /* az = +32767 (largest positive) */
        0xFF, 0x2E, /* temp = -210 */
        0x80, 0x00, /* gx = -32768 (largest negative) */
        0x00, 0xA4, /* gy = +164 (+10 dps at +-2000 dps) */
        0xFF, 0xFF  /* gz = -1 */
    };
    mpu6050_raw_t raw;

    TEST_ASSERT_EQUAL_INT(MPU6050_OK, mpu6050_init(&imu, &bus, MPU6050_ADDR_AD0_LOW, &cfg));
    for (int i = 0; i < 14; i++) {
        mock.regs[0x3B + i] = burst[i];
    }
    TEST_ASSERT_EQUAL_INT(MPU6050_OK, mpu6050_read_raw(&imu, &raw));

    TEST_ASSERT_EQUAL_INT16(2048, raw.ax);
    TEST_ASSERT_EQUAL_INT16(-2048, raw.ay);
    TEST_ASSERT_EQUAL_INT16(32767, raw.az);
    TEST_ASSERT_EQUAL_INT16(-210, raw.temp);
    TEST_ASSERT_EQUAL_INT16(-32768, raw.gx);
    TEST_ASSERT_EQUAL_INT16(164, raw.gy);
    TEST_ASSERT_EQUAL_INT16(-1, raw.gz);

    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, mpu6050_accel_g(&imu, raw.ax));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 10.0f, mpu6050_gyro_dps(&imu, raw.gy));
}

void test_read_raw_nack_returns_i2c_error(void)
{
    mpu6050_raw_t raw;

    TEST_ASSERT_EQUAL_INT(MPU6050_OK, mpu6050_init(&imu, &bus, MPU6050_ADDR_AD0_LOW, &cfg));
    mock.nack_on_transaction = mock.n_transactions + 1;
    TEST_ASSERT_EQUAL_INT(MPU6050_ERR_I2C, mpu6050_read_raw(&imu, &raw));
}

void test_temperature_conversion(void)
{
    /* RM 4.18: raw 0 -> 36.53 C; raw 340 -> one degree more. */
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 36.53f, mpu6050_temp_c(0));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 37.53f, mpu6050_temp_c(340));
}

void test_strerror_names_every_code(void)
{
    TEST_ASSERT_EQUAL_STRING("ok", mpu6050_strerror(MPU6050_OK));
    TEST_ASSERT_EQUAL_STRING("wrong WHO_AM_I", mpu6050_strerror(MPU6050_ERR_WHO_AM_I));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_writes_registers_in_expected_order);
    RUN_TEST(test_init_checks_who_am_i_first);
    RUN_TEST(test_init_waits_after_reset);
    RUN_TEST(test_init_uses_given_address);
    RUN_TEST(test_init_sets_scale_factors);
    RUN_TEST(test_wrong_who_am_i_returns_error_and_writes_nothing);
    RUN_TEST(test_nack_on_who_am_i_returns_i2c_error);
    RUN_TEST(test_nack_partway_through_init_returns_i2c_error);
    RUN_TEST(test_bad_config_returns_param_error);
    RUN_TEST(test_null_arguments_return_param_error);
    RUN_TEST(test_read_raw_is_one_14_byte_burst_from_accel_xout_h);
    RUN_TEST(test_read_raw_parses_big_endian_signed_values);
    RUN_TEST(test_read_raw_nack_returns_i2c_error);
    RUN_TEST(test_temperature_conversion);
    RUN_TEST(test_strerror_names_every_code);
    return UNITY_END();
}
