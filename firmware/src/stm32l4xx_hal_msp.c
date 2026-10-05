/*
 * stm32l4xx_hal_msp.c - low-level setup the HAL asks for.
 *
 * "MSP" is ST's name for MCU Support Package. HAL_xxx_Init() configures a
 * peripheral's registers, then calls HAL_xxx_MspInit() so the project can
 * turn on that peripheral's clock and route its pins. Keeping pin setup
 * here means the drivers themselves don't depend on the board layout.
 */
#include "board.h"

/* Called from HAL_Init(). */
void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio = {0};

    if (huart->Instance != USART2) {
        return;
    }

    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* Each pin can be wired inside the chip to one of up to 16 "alternate
     * functions" (AF0-AF15). The datasheet's AF table says USART2_TX is AF7
     * on PA2, and USART2_RX is AF3 on PA15. */
    gpio.Pin = GPIO_PIN_2;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = GPIO_PIN_15;
    gpio.Pull = GPIO_PULLUP; /* keep RX idle-high if nothing drives it */
    gpio.Alternate = GPIO_AF3_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    GPIO_InitTypeDef gpio = {0};

    if (hi2c->Instance != IMU_I2C) {
        return;
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* I2C lines are open-drain: devices only ever pull them low, and
     * pull-up resistors bring them high. The GY-521 board has 4.7k
     * pull-ups; the weak (~40k) internal ones are just a backup.
     * PA9 = I2C1_SCL, PA10 = I2C1_SDA, both AF4 (datasheet AF table). */
    gpio.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    gpio.Mode = GPIO_MODE_AF_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOA, &gpio);

    __HAL_RCC_I2C1_CLK_ENABLE();
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance != IMU_I2C) {
        return;
    }
    __HAL_RCC_I2C1_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);
}

void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    GPIO_InitTypeDef gpio = {0};

    if (hspi->Instance != SD_SPI) {
        return;
    }

    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA5 = SPI1_SCK, PA6 = SPI1_MISO, PA7 = SPI1_MOSI, all AF5.
     * Pull-up on MISO: an SD card leaves it floating while not selected. */
    gpio.Pin = GPIO_PIN_5 | GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = GPIO_PIN_6;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &gpio);
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != SAMPLE_TIMER) {
        return;
    }
    __HAL_RCC_TIM6_CLK_ENABLE();
    HAL_NVIC_SetPriority(SAMPLE_TIMER_IRQn, IRQ_PRIO_SAMPLE_TIMER, 0);
    HAL_NVIC_EnableIRQ(SAMPLE_TIMER_IRQn);
}
