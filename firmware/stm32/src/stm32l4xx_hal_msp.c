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
