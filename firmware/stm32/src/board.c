/*
 * board.c - clock, GPIO and UART setup for the NUCLEO-L432KC.
 */
#include <string.h>
#include "board.h"

UART_HandleTypeDef huart2;

/*
 * Clock tree:
 *   MSI (internal RC oscillator) at 4 MHz
 *     -> PLL: x40 / 2  = 80 MHz system clock (the L432's maximum)
 *     -> AHB, APB1, APB2 all at 80 MHz
 *
 * The MSI is an RC oscillator, so on its own it is only accurate to about
 * 1%. The Nucleo has a 32.768 kHz watch crystal (LSE) fitted, and the L4
 * can use it to keep trimming the MSI ("MSI PLL mode"). That makes the
 * 80 MHz clock, and so every timer and baud rate, crystal-accurate.
 * Good enough timing matters later for the 200 Hz sampling.
 *
 * If the crystal doesn't start we still run, just less accurately, and
 * main() prints a warning.
 */
bool board_clock_init(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    bool lse_ok;

    /* Voltage range 1 is required to run above 26 MHz. */
    __HAL_RCC_PWR_CLK_ENABLE();
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK) {
        Error_Handler();
    }

    /* 1. Try to start the 32.768 kHz crystal. It lives in the backup
     *    domain, which is write-protected after reset. */
    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);
    osc.OscillatorType = RCC_OSCILLATORTYPE_LSE;
    osc.LSEState = RCC_LSE_ON;
    osc.PLL.PLLState = RCC_PLL_NONE;
    lse_ok = (HAL_RCC_OscConfig(&osc) == HAL_OK);

    /* 2. MSI at 4 MHz feeding the PLL. VCO = 4 MHz / M * N = 160 MHz,
     *    system clock = VCO / R = 80 MHz. */
    memset(&osc, 0, sizeof(osc));
    osc.OscillatorType = RCC_OSCILLATORTYPE_MSI;
    osc.MSIState = RCC_MSI_ON;
    osc.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    osc.MSIClockRange = RCC_MSIRANGE_6; /* 4 MHz */
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_MSI;
    osc.PLL.PLLM = 1;
    osc.PLL.PLLN = 40;
    osc.PLL.PLLP = RCC_PLLP_DIV7;
    osc.PLL.PLLQ = RCC_PLLQ_DIV2;
    osc.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    /* 3. Switch the system clock to the PLL. Flash can't be read in one
     *    cycle at 80 MHz, so it needs 4 wait states (from the reference
     *    manual's flash latency table for range 1). */
    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK) {
        Error_Handler();
    }

    /* 4. Let the crystal trim the MSI. */
    if (lse_ok) {
        HAL_RCCEx_EnableMSIPLLMode();
    }

    return lse_ok;
}

void board_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_RESET);
    gpio.Pin = LED_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_PORT, &gpio);
}

/* 115200 baud, 8 data bits, no parity, 1 stop bit ("8N1"). The pin setup
 * is in HAL_UART_MspInit() in stm32l4xx_hal_msp.c, which HAL_UART_Init()
 * calls for us. */
void board_uart_init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = CONSOLE_BAUD;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }
}

void led_toggle(void)
{
    HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);
}

/* Crude delay that works with interrupts off (HAL_Delay needs the tick
 * interrupt). Not calibrated; it only has to look like "fast blinking". */
static void busy_wait(uint32_t loops)
{
    for (volatile uint32_t i = 0; i < loops; i++) {
    }
}

void fatal_error(const char *what)
{
    static const char prefix[] = "\r\nFATAL: ";
    static const char suffix[] = "\r\n";

    __disable_irq();

    /* Polling transmit doesn't need interrupts. If the UART was never set
     * up, HAL_UART_Transmit just returns an error and we carry on. */
    HAL_UART_Transmit(&huart2, (uint8_t *)prefix, sizeof(prefix) - 1, HAL_MAX_DELAY);
    HAL_UART_Transmit(&huart2, (uint8_t *)what, (uint16_t)strlen(what), HAL_MAX_DELAY);
    HAL_UART_Transmit(&huart2, (uint8_t *)suffix, sizeof(suffix) - 1, HAL_MAX_DELAY);

    board_gpio_init();
    for (;;) {
        led_toggle();
        busy_wait(200000);
    }
}

void Error_Handler(void)
{
    fatal_error("Error_Handler");
}
