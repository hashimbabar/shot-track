# Wiring

NUCLEO-L432KC on a breadboard, powered over its USB cable. Pin names match
`firmware/include/board.h`; if one changes, change both.

The L432KC uses Arduino Nano style header labels (D0-D13, A0-A7). The table
gives both the label printed on the board and the STM32 pin.

## MPU-6050 (GY-521 breakout), I2C

| GY-521 pin | Nucleo pin | STM32 pin | Suggested wire | Notes |
|---|---|---|---|---|
| VCC | 3V3 | - | red | The GY-521 has its own regulator and also accepts 5 V, but 3.3 V keeps its I2C pull-ups at 3.3 V, which is the STM32's logic level |
| GND | GND | - | black | |
| SCL | D1 | PA9 (I2C1_SCL, AF4) | yellow | |
| SDA | D0 | PA10 (I2C1_SDA, AF4) | green | |
| AD0 | GND | - | black | Sets the I2C address to 0x68. Most GY-521 boards already pull AD0 low |
| INT | not connected | - | - | Sampling is timed by TIM6 instead (see architecture.md) |
| XDA, XCL | not connected | - | - | Auxiliary I2C bus for an external magnetometer |

The GY-521 board has 4.7 kΩ pull-up resistors on SDA and SCL, so no external
ones are needed.

Why not the usual I2C1 pins PB6/PB7 (D5/D4)? On the Nucleo-32, solder
bridges SB16 and SB18 connect PB7 to PA5 and PB6 to PA6, so that A4/A5 can
act as I2C on Arduino Nano shields (UM1956, the Nucleo-32 user manual).
PA5 and PA6 are needed for SPI1 below. PA9/PA10 avoid that clash, and they
are free because the ST-LINK serial port uses USART2 (PA2/PA15), not USART1.
**Leave D4 and D5 unconnected**, because they're bridged to A4/A5.

## microSD breakout, SPI

| SD breakout pin | Nucleo pin | STM32 pin | Suggested wire | Notes |
|---|---|---|---|---|
| VCC (3V or 5V) | 3V3 | - | red | Check your breakout. Adafruit's #254 accepts 3-5 V. Bare 3.3 V-only modules must not get 5 V |
| GND | GND | - | black | |
| CLK / SCK | A4 | PA5 (SPI1_SCK, AF5) | orange | |
| DO / MISO | A5 | PA6 (SPI1_MISO, AF5) | blue | Internal pull-up enabled in firmware |
| DI / MOSI | A6 | PA7 (SPI1_MOSI, AF5) | purple | |
| CS | A3 | PA4 (GPIO output) | white | Driven by firmware, not the SPI hardware |
| CD (card detect) | not connected | - | - | |

Format the card as FAT32 (cards up to 32 GB). exFAT is turned off in the
firmware to keep it small.

## Button, debug pins, LED

| Part | Nucleo pin | STM32 pin | Suggested wire | Notes |
|---|---|---|---|---|
| Push button leg 1 | D3 | PB0 | brown | Internal pull-up, falling-edge interrupt |
| Push button leg 2 | GND | - | black | Pressed = PB0 reads 0. Starts/stops a logging session |
| Logic analyzer CH0 | D9 | PA8 | grey | High while the IMU is read (one pulse per sample) |
| Logic analyzer CH1 | D10 | PA11 | grey | High while the logger writes to the SD card |
| Logic analyzer GND | GND | - | black | Always connect the analyzer's ground |
| LED | - | PB3 (D13) | - | On-board green LD3. Nothing to wire |

## Serial console

Nothing to wire. USART2 (PA2 TX, PA15 RX) goes to the on-board ST-LINK and
shows up on the laptop as a USB serial port at 115200 baud.

## Checklist before powering on

- [ ] Nothing connected to D4/D5 (bridged to A4/A5)
- [ ] Every module's GND goes to a Nucleo GND
- [ ] Nothing powered from 5V that should be 3.3 V
- [ ] SD card formatted FAT32
