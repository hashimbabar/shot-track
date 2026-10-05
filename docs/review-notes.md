# Datasheet review notes

My own check of each driver against its datasheet. One row per thing
checked. Fill in "What I found" in my own words, with the datasheet page or
section, and mark it ✅ (matches), ❌ (mismatch, link the fix) or ❓ (not
sure yet).

Documents:
- MPU-6050 Register Map, RM-MPU-6000A-00 rev 4.2
- MPU-6050 Product Specification, PS-MPU-6000A-00 rev 3.4
- STM32L432KC datasheet (DS11451) and reference manual (RM0394)
- NUCLEO-L432KC user manual (UM1956)
- SD Physical Layer Simplified Specification (SPI mode chapter)

## MPU-6050 driver (`lib/mpu6050`)

| Item | Code location | Datasheet section | What I found | Status |
|---|---|---|---|---|
| WHO_AM_I address and expected value | | | | |
| PWR_MGMT_1 reset bit and wait after reset | | | | |
| Wake from sleep, clock source choice | | | | |
| CONFIG / DLPF_CFG values and bandwidths | | | | |
| SMPLRT_DIV formula (1 kHz vs 8 kHz base) | | | | |
| GYRO_CONFIG FS_SEL bits and sensitivity | | | | |
| ACCEL_CONFIG AFS_SEL bits and sensitivity | | | | |
| Burst read start register and byte order | | | | |
| Temperature formula | | | | |
| I2C address with AD0 low/high | | | | |
| Max I2C clock (400 kHz) | | | | |

## STM32 board setup (`firmware/src/board.c`, `stm32l4xx_hal_msp.c`)

| Item | Code location | Datasheet section | What I found | Status |
|---|---|---|---|---|
| PA9/PA10 AF4 = I2C1 | | | | |
| PA5/PA6/PA7 AF5 = SPI1 | | | | |
| PA2 AF7 / PA15 AF3 = USART2 | | | | |
| I2C TIMINGR 0x00702991 for 400 kHz at 80 MHz | | | | |
| TIM6 prescaler/period for 200 Hz | | | | |
| Flash latency 4 WS at 80 MHz, range 1 | | | | |
| SB16/SB18 solder bridge behaviour | | | | |

## SD card driver (`firmware/src/sd_spi.c`)

| Item | Code location | Datasheet section | What I found | Status |
|---|---|---|---|---|
| Power-up: ≥ 74 clocks with CS high | | | | |
| CMD0 / CMD8 CRC values | | | | |
| ACMD41 HCS bit | | | | |
| CMD58 CCS bit (block vs byte addressing) | | | | |
| Data start token and data response token | | | | |
| Init clock ≤ 400 kHz | | | | |

## Other notes

<!-- Anything else noticed while reading datasheets. -->
