/*
 * sd_spi.c - microSD driver in SPI mode.
 *
 * Follows the SD Physical Layer Simplified Specification (SD Association),
 * section 7 "SPI Mode". Command numbers below are from that spec. The
 * structure is the usual one for SD-over-SPI (ChaN's FatFs examples use
 * the same flow).
 *
 * Every command is 6 bytes: 0x40 | index, a 32-bit argument (high byte
 * first), and a CRC byte. CRC is only checked for CMD0 and CMD8 in SPI
 * mode, so only those two need the real value.
 */
#include <string.h>
#include "sd_spi.h"
#include "board.h"

#define CMD0    0   /* GO_IDLE_STATE: reset into SPI mode */
#define CMD8    8   /* SEND_IF_COND: check voltage, detects v2 cards */
#define CMD16   16  /* SET_BLOCKLEN (only needed for byte-addressed cards) */
#define CMD17   17  /* READ_SINGLE_BLOCK */
#define CMD24   24  /* WRITE_BLOCK */
#define CMD55   55  /* APP_CMD: next command is an "ACMD" */
#define CMD58   58  /* READ_OCR: tells SDHC (block addresses) from SDSC */
#define ACMD41  (0x80 | 41) /* SD_SEND_OP_COND: start card initialisation */

#define R1_IDLE         0x01
#define TOKEN_START     0xFE  /* start of a data block (read and write) */
#define DATA_ACCEPTED   0x05

#define INIT_TIMEOUT_MS   1000
#define READY_TIMEOUT_MS  500
#define READ_TIMEOUT_MS   200

static bool ready;
static bool block_addressing; /* SDHC/SDXC: address = block number */

static void cs_low(void)  { HAL_GPIO_WritePin(SD_CS_GPIO_PORT, SD_CS_PIN, GPIO_PIN_RESET); }
static void cs_high(void) { HAL_GPIO_WritePin(SD_CS_GPIO_PORT, SD_CS_PIN, GPIO_PIN_SET); }

static uint8_t xfer(uint8_t out)
{
    uint8_t in = 0xFF;
    HAL_SPI_TransmitReceive(&hspi1, &out, &in, 1, 10);
    return in;
}

static void set_prescaler(uint32_t prescaler)
{
    __HAL_SPI_DISABLE(&hspi1);
    MODIFY_REG(hspi1.Instance->CR1, SPI_CR1_BR, prescaler);
    __HAL_SPI_ENABLE(&hspi1);
}

/* A busy card holds MISO low. Wait until it reads back 0xFF. */
static bool wait_ready(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    do {
        if (xfer(0xFF) == 0xFF) {
            return true;
        }
    } while (HAL_GetTick() - start < timeout_ms);
    return false;
}

static void sd_deselect(void)
{
    cs_high();
    xfer(0xFF); /* one extra clock so the card lets go of MISO */
}

static bool sd_select(void)
{
    cs_low();
    xfer(0xFF);
    if (wait_ready(READY_TIMEOUT_MS)) {
        return true;
    }
    sd_deselect();
    return false;
}

/* Send a command, return the R1 response (0xFF = no response). */
static uint8_t send_cmd(uint8_t cmd, uint32_t arg)
{
    uint8_t r1, crc = 0x01;

    if (cmd & 0x80) {           /* ACMDn = CMD55 then CMDn */
        cmd &= 0x7F;
        r1 = send_cmd(CMD55, 0);
        if (r1 > R1_IDLE) {
            return r1;
        }
    }

    sd_deselect();
    if (!sd_select()) {
        return 0xFF;
    }

    xfer(0x40 | cmd);
    xfer((uint8_t)(arg >> 24));
    xfer((uint8_t)(arg >> 16));
    xfer((uint8_t)(arg >> 8));
    xfer((uint8_t)arg);
    if (cmd == CMD0) {
        crc = 0x95;
    } else if (cmd == CMD8) {
        crc = 0x87;
    }
    xfer(crc);

    /* R1 arrives within 8 bytes; its top bit is 0. */
    for (int i = 0; i < 10; i++) {
        r1 = xfer(0xFF);
        if ((r1 & 0x80) == 0) {
            break;
        }
    }
    return r1;
}

sd_err_t sd_init(void)
{
    uint8_t ocr[4];
    uint32_t start;
    sd_err_t result = SD_OK;

    ready = false;
    set_prescaler(SPI_BAUDRATEPRESCALER_256); /* 312 kHz: must be < 400 kHz here */

    /* At least 74 clocks with CS high so the card enters native mode. */
    cs_high();
    for (int i = 0; i < 10; i++) {
        xfer(0xFF);
    }

    if (send_cmd(CMD0, 0) != R1_IDLE) {
        sd_deselect();
        return SD_ERR_NO_CARD;
    }

    if (send_cmd(CMD8, 0x1AA) == R1_IDLE) {
        /* v2 card. It echoes the check pattern 0xAA and accepts 2.7-3.6 V. */
        for (int i = 0; i < 4; i++) {
            ocr[i] = xfer(0xFF);
        }
        if (ocr[2] != 0x01 || ocr[3] != 0xAA) {
            result = SD_ERR_UNSUPPORTED;
        } else {
            /* ACMD41 with HCS (bit 30) = "I support high-capacity cards".
             * Repeat until the card leaves the idle state. */
            start = HAL_GetTick();
            while (send_cmd(ACMD41, 1UL << 30) != 0) {
                if (HAL_GetTick() - start > INIT_TIMEOUT_MS) {
                    result = SD_ERR_TIMEOUT;
                    break;
                }
            }
            if (result == SD_OK) {
                if (send_cmd(CMD58, 0) != 0) {
                    result = SD_ERR_UNSUPPORTED;
                } else {
                    for (int i = 0; i < 4; i++) {
                        ocr[i] = xfer(0xFF);
                    }
                    /* OCR bit 30 (CCS): 1 = SDHC/SDXC. */
                    block_addressing = (ocr[0] & 0x40) != 0;
                }
            }
        }
    } else {
        /* v1 card (or MMC, which we don't support). */
        start = HAL_GetTick();
        while (send_cmd(ACMD41, 0) != 0) {
            if (HAL_GetTick() - start > INIT_TIMEOUT_MS) {
                result = SD_ERR_UNSUPPORTED;
                break;
            }
        }
        if (result == SD_OK && send_cmd(CMD16, 512) != 0) {
            result = SD_ERR_UNSUPPORTED;
        }
        block_addressing = false;
    }

    sd_deselect();
    if (result != SD_OK) {
        return result;
    }

    /* 80 MHz / 8 = 10 MHz. SD cards allow 25 MHz, but breadboard
     * jumpers ring at high speeds; raise this after checking the
     * signals on a scope. */
    set_prescaler(SPI_BAUDRATEPRESCALER_8);
    ready = true;
    return SD_OK;
}

bool sd_is_ready(void)
{
    return ready;
}

static uint32_t card_address(uint32_t lba)
{
    return block_addressing ? lba : lba * 512u;
}

static bool receive_block(uint8_t *buf)
{
    uint32_t start = HAL_GetTick();
    uint8_t token;

    do {
        token = xfer(0xFF);
    } while (token == 0xFF && HAL_GetTick() - start < READ_TIMEOUT_MS);
    if (token != TOKEN_START) {
        return false;
    }

    /* Clock in 512 bytes. MOSI must stay high (0xFF) while reading, so
     * fill the buffer with 0xFF and send it as we receive into it. Each
     * byte is sent before its slot is overwritten. */
    memset(buf, 0xFF, 512);
    if (HAL_SPI_TransmitReceive(&hspi1, buf, buf, 512, 100) != HAL_OK) {
        return false;
    }
    xfer(0xFF); /* CRC, ignored in SPI mode */
    xfer(0xFF);
    return true;
}

static bool send_block(const uint8_t *buf)
{
    uint8_t resp;

    if (!wait_ready(READY_TIMEOUT_MS)) {
        return false;
    }
    xfer(TOKEN_START);
    if (HAL_SPI_Transmit(&hspi1, (uint8_t *)buf, 512, 100) != HAL_OK) {
        return false;
    }
    /* Transmit-only leaves received bytes in the SPI RX FIFO. Empty it
     * so the next xfer() reads the card's reply, not old data. */
    HAL_SPIEx_FlushRxFifo(&hspi1);
    xfer(0xFF); /* dummy CRC */
    xfer(0xFF);
    resp = xfer(0xFF);
    /* Data response token: xxx0 0101 = accepted. The card then holds
     * MISO low while it programs the flash; wait_ready() handles that
     * before the next command. */
    return (resp & 0x1F) == DATA_ACCEPTED;
}

sd_err_t sd_read_blocks(uint8_t *buf, uint32_t lba, uint32_t count)
{
    if (!ready) {
        return SD_ERR_IO;
    }
    for (uint32_t i = 0; i < count; i++) {
        bool ok = send_cmd(CMD17, card_address(lba + i)) == 0 && receive_block(buf + 512u * i);
        sd_deselect();
        if (!ok) {
            return SD_ERR_IO;
        }
    }
    return SD_OK;
}

sd_err_t sd_write_blocks(const uint8_t *buf, uint32_t lba, uint32_t count)
{
    if (!ready) {
        return SD_ERR_IO;
    }
    for (uint32_t i = 0; i < count; i++) {
        bool ok = send_cmd(CMD24, card_address(lba + i)) == 0 && send_block(buf + 512u * i);
        sd_deselect();
        if (!ok) {
            return SD_ERR_IO;
        }
    }
    return SD_OK;
}

sd_err_t sd_sync(void)
{
    bool ok;

    if (!ready) {
        return SD_ERR_IO;
    }
    ok = sd_select(); /* sd_select() waits for the card to stop being busy */
    sd_deselect();
    return ok ? SD_OK : SD_ERR_IO;
}
