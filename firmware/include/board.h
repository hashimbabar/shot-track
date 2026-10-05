/*
 * board.h - everything specific to the NUCLEO-L432KC: clocks, pins, UART.
 *
 * Later steps add the IMU, SD card and button pins here, so main.c and the
 * tasks never need to know which physical pin is which.
 */
#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include "stm32l4xx_hal.h"

/* Green user LED LD3, on pin PB3 (pin D13 on the Nucleo header). */
#define LED_GPIO_PORT GPIOB
#define LED_PIN       GPIO_PIN_3

/* USART2 is wired to the on-board ST-LINK, which passes it to the PC as a
 * USB serial port. TX = PA2, RX = PA15. */
#define CONSOLE_BAUD  115200

extern UART_HandleTypeDef huart2;

/* Clocks: MSI 4 MHz -> PLL -> 80 MHz. Returns true if the 32.768 kHz
 * crystal (LSE) started and is now trimming the MSI. */
bool board_clock_init(void);
void board_gpio_init(void);
void board_uart_init(void);

void led_toggle(void);

/* Something went wrong that we can't recover from: print why, then blink
 * the LED fast forever. Safe to call with interrupts disabled. */
void fatal_error(const char *what);

/* ST's HAL examples call this name on errors, so keep it. */
void Error_Handler(void);

#endif /* BOARD_H */
