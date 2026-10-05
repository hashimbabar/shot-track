/*
 * sd_spi.h - microSD card in SPI mode, 512-byte blocks.
 *
 * Supports SD v2 cards (SDHC/SDXC, i.e. any card from the last ~15 years)
 * and old SD v1 cards. MMC cards are not supported.
 */
#ifndef SD_SPI_H
#define SD_SPI_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SD_OK = 0,
    SD_ERR_NO_CARD = -1,     /* no answer to CMD0 */
    SD_ERR_UNSUPPORTED = -2, /* card answered but isn't a card we handle */
    SD_ERR_TIMEOUT = -3,     /* card didn't finish initialising in time */
    SD_ERR_IO = -4           /* read/write rejected or timed out */
} sd_err_t;

sd_err_t sd_init(void);
bool sd_is_ready(void);
sd_err_t sd_read_blocks(uint8_t *buf, uint32_t lba, uint32_t count);
sd_err_t sd_write_blocks(const uint8_t *buf, uint32_t lba, uint32_t count);
/* Wait until the card has finished its last write. */
sd_err_t sd_sync(void);

#endif /* SD_SPI_H */
