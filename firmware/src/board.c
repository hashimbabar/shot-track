/*
 * board.c - clock, GPIO, UART, I2C, SPI and timer setup for the
 * NUCLEO-L432KC. Pin choices are explained in board.h.
 */
#include <string.h>
#include "board.h"

UART_HandleTypeDef huart2;
I2C_HandleTypeDef hi2c1;
SPI_HandleTypeDef hspi1;
TIM_HandleTypeDef htim6;

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

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* LED and debug pins: push-pull outputs, start low. */
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, GPIO_PIN_RESET);
    gpio.Pin = LED_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_PORT, &gpio);

    HAL_GPIO_WritePin(DBG_GPIO_PORT, DBG_SAMPLE_PIN | DBG_SD_PIN, GPIO_PIN_RESET);
    gpio.Pin = DBG_SAMPLE_PIN | DBG_SD_PIN;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH; /* sharp edges for the logic analyzer */
    HAL_GPIO_Init(DBG_GPIO_PORT, &gpio);

    /* SD chip select: output, idle high (card not selected). */
    HAL_GPIO_WritePin(SD_CS_GPIO_PORT, SD_CS_PIN, GPIO_PIN_SET);
    gpio.Pin = SD_CS_PIN;
    HAL_GPIO_Init(SD_CS_GPIO_PORT, &gpio);

    /* Button: input with pull-up, interrupt on the falling edge (press).
     * The EXTI interrupt is enabled later by the UI task, once there is a
     * task for it to wake. */
    gpio.Pin = BUTTON_PIN;
    gpio.Mode = GPIO_MODE_IT_FALLING;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BUTTON_GPIO_PORT, &gpio);
    HAL_NVIC_SetPriority(BUTTON_IRQn, IRQ_PRIO_BUTTON, 0);
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

/*
 * I2C1 at 400 kHz ("fast mode"). The L4 sets I2C timing with one 32-bit
 * TIMINGR value instead of a simple clock divider. 0x00702991 is the
 * value ST's timing tool gives for 400 kHz from an 80 MHz clock. Decoded
 * (RM0394, I2C_TIMINGR): PRESC = 0 -> 12.5 ns ticks, SCLL = 145 -> 1.84 us
 * low, SCLH = 41 -> 0.53 us high (plus sync and rise time), SCLDEL = 7,
 * SDADEL = 0. One period is about 2.5 us = 400 kHz.
 */
void board_i2c_init(void)
{
    hi2c1.Instance = IMU_I2C;
    hi2c1.Init.Timing = 0x00702991;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        Error_Handler();
    }
    /* Analog noise filter on: ignores glitches shorter than ~50 ns, which
     * helps on a breadboard with long jumper wires. */
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
        Error_Handler();
    }
}

/*
 * SPI1 in mode 0 (clock idles low, sample on the rising edge), which is
 * what SD cards use in SPI mode. It starts slow (80 MHz / 256 = 312 kHz)
 * because a card must be initialised below 400 kHz; sd_spi.c speeds it
 * up once the card is ready.
 */
void board_spi_init(void)
{
    hspi1.Instance = SD_SPI;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT; /* chip select driven by hand */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 7;
    hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
    hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
        Error_Handler();
    }
}

/* TIM6 counts up at 10 kHz and overflows every 50 counts = 200 Hz. Each
 * overflow fires TIM6_DAC_IRQHandler (stm32l4xx_it.c), which wakes the
 * sampling task. Started later by the sampling task with
 * HAL_TIM_Base_Start_IT(), once the IMU is ready. */
void board_timer_init(void)
{
    htim6.Instance = SAMPLE_TIMER;
    htim6.Init.Prescaler = 7999;
    htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim6.Init.Period = 49;
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim6) != HAL_OK) {
        Error_Handler();
    }
}

void led_set(bool on)
{
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool button_is_pressed(void)
{
    return HAL_GPIO_ReadPin(BUTTON_GPIO_PORT, BUTTON_PIN) == GPIO_PIN_RESET;
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

    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {.Pin = LED_PIN, .Mode = GPIO_MODE_OUTPUT_PP};
    HAL_GPIO_Init(LED_GPIO_PORT, &gpio);
    for (;;) {
        HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_PIN);
        busy_wait(200000);
    }
}

void Error_Handler(void)
{
    fatal_error("Error_Handler");
}
