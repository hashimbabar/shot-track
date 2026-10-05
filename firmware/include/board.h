/*
 * board.h - everything specific to the NUCLEO-L432KC: clocks, pins and
 * peripheral handles. Wiring is in docs/wiring.md; keep the two in sync.
 */
#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include "stm32l4xx_hal.h"

/* Green user LED LD3 on PB3 (header pin D13). */
#define LED_GPIO_PORT      GPIOB
#define LED_PIN            GPIO_PIN_3

/* Push button between PB0 (D3) and GND, internal pull-up, so it reads 0
 * when pressed. Falling edge -> EXTI0 interrupt. */
#define BUTTON_GPIO_PORT   GPIOB
#define BUTTON_PIN         GPIO_PIN_0
#define BUTTON_IRQn        EXTI0_IRQn

/* MPU-6050 on I2C1: SCL = PA9 (D1), SDA = PA10 (D0), alternate function 4.
 * PB6/PB7 would be the usual I2C1 pins, but on this Nucleo they are
 * bridged to PA6/PA5 (solder bridges SB16/SB18), which SPI1 needs. */
#define IMU_I2C            I2C1

/* microSD on SPI1: SCK = PA5 (A4), MISO = PA6 (A5), MOSI = PA7 (A6),
 * alternate function 5. Chip select is a plain GPIO, PA4 (A3). */
#define SD_SPI             SPI1
#define SD_CS_GPIO_PORT    GPIOA
#define SD_CS_PIN          GPIO_PIN_4

/* Debug pins for the logic analyzer:
 *   PA8  (D9)  high while the sampling task reads the IMU
 *   PA11 (D10) high while the logger writes to the SD card */
#define DBG_GPIO_PORT      GPIOA
#define DBG_SAMPLE_PIN     GPIO_PIN_8
#define DBG_SD_PIN         GPIO_PIN_11

/* USART2 goes to the ST-LINK's USB serial port: TX = PA2, RX = PA15. */
#define CONSOLE_BAUD       115200

/* 200 Hz sample timer. TIM6 is a basic timer on APB1 (80 MHz):
 * 80 MHz / (7999 + 1) = 10 kHz, then 10 kHz / (49 + 1) = 200 Hz. */
#define SAMPLE_TIMER       TIM6
#define SAMPLE_TIMER_IRQn  TIM6_DAC_IRQn
#define SAMPLE_RATE_HZ     200u
#define SAMPLE_PERIOD_MS   (1000u / SAMPLE_RATE_HZ)

/*
 * Interrupt priorities. Lower number = more urgent. Anything that calls
 * a FreeRTOS ...FromISR() function must be numerically >= 5
 * (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY), or it could corrupt
 * the kernel's data.
 */
#define IRQ_PRIO_SAMPLE_TIMER  6
#define IRQ_PRIO_BUTTON        7

extern UART_HandleTypeDef huart2;
extern I2C_HandleTypeDef hi2c1;
extern SPI_HandleTypeDef hspi1;
extern TIM_HandleTypeDef htim6;

/* Clocks: MSI 4 MHz -> PLL -> 80 MHz. Returns true if the 32.768 kHz
 * crystal (LSE) started and is now trimming the MSI. */
bool board_clock_init(void);
void board_gpio_init(void);
void board_uart_init(void);
void board_i2c_init(void);
void board_spi_init(void);
void board_timer_init(void);

void led_set(bool on);
bool button_is_pressed(void);

/* Something went wrong that we can't recover from: print why, then blink
 * the LED fast forever. Safe to call with interrupts disabled. */
void fatal_error(const char *what);

/* ST's HAL calls this name on errors, so keep it. */
void Error_Handler(void);

#endif /* BOARD_H */
