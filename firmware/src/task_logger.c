/*
 * task_logger.c - writes sessions to the microSD card as CSV.
 *
 * Lowest priority on purpose. An SD card write can take anything from
 * under a millisecond to a few hundred (the card erases flash blocks
 * internally whenever it feels like it). While this task is stuck in a
 * write, the sampling task keeps preempting it on schedule and keeps
 * filling log_q. As long as the queue doesn't fill up, no samples are
 * lost. If it does, the sampling task counts the drops in
 * g_stats.log_q_full.
 *
 * Text is collected into a 512-byte block and written one full block at
 * a time, matching the card's sector size, instead of one small write
 * per row.
 *
 * File format (LOGnnnnn.CSV):
 *   # comment lines start with '#'
 *   seq,t_ms,ax,ay,az,gx,gy,gz        raw sensor counts
 *   # shot,t_ms,peak_dps,peak_mg      one line per detected shot
 */
#include <stdio.h>
#include <string.h>
#include "app.h"
#include "board.h"
#include "ff.h"

#define BLOCK_SIZE     512
#define SYNC_PERIOD_MS 5000  /* bounds how much a power cut can lose */
#define MAX_LOG_FILES  99999

static FATFS fs;
static FIL file;
static bool mounted;
static char block[BLOCK_SIZE];
static size_t block_used;

static bool write_bytes(const void *data, UINT len)
{
    UINT written = 0;
    FRESULT fr;

    HAL_GPIO_WritePin(DBG_GPIO_PORT, DBG_SD_PIN, GPIO_PIN_SET);
    fr = f_write(&file, data, len, &written);
    HAL_GPIO_WritePin(DBG_GPIO_PORT, DBG_SD_PIN, GPIO_PIN_RESET);

    if (fr != FR_OK || written != len) {
        g_stats.sd_errors++;
        return false;
    }
    return true;
}

/* Add text to the block buffer, writing out each block as it fills. */
static void append(const char *text, size_t len)
{
    while (len > 0) {
        size_t n = BLOCK_SIZE - block_used;
        if (n > len) {
            n = len;
        }
        memcpy(&block[block_used], text, n);
        block_used += n;
        text += n;
        len -= n;
        if (block_used == BLOCK_SIZE) {
            write_bytes(block, BLOCK_SIZE);
            block_used = 0;
        }
    }
}

static void write_record(const log_record_t *r)
{
    char line[96];
    int n;

    if (r->type == LOG_REC_SAMPLE) {
        const imu_sample_t *s = &r->u.sample;
        n = snprintf(line, sizeof(line), "%lu,%lu,%d,%d,%d,%d,%d,%d\n",
                     (unsigned long)s->seq, (unsigned long)s->t_ms,
                     s->raw.ax, s->raw.ay, s->raw.az,
                     s->raw.gx, s->raw.gy, s->raw.gz);
        g_stats.log_rows++;
    } else {
        n = snprintf(line, sizeof(line), "# shot,%lu,%u,%u\n",
                     (unsigned long)r->u.shot.t_ms,
                     (unsigned)r->u.shot.peak_dps, (unsigned)r->u.shot.peak_mg);
    }
    if (n > 0 && n < (int)sizeof(line)) {
        append(line, (size_t)n);
    }
}

static bool start_session(void)
{
    static const char header[] =
        "# shottrack log: MPU-6050 raw counts, 200 Hz, accel +-16 g (2048 LSB/g), "
        "gyro +-2000 dps (16.4 LSB/dps)\n"
        "seq,t_ms,ax,ay,az,gx,gy,gz\n";
    char name[16];
    FRESULT fr;

    if (!mounted) {
        fr = f_mount(&fs, "", 1);
        if (fr != FR_OK) {
            console_printf("sd: mount failed (FatFs error %d). Card in? FAT32?\r\n", (int)fr);
            g_stats.sd_errors++;
            return false;
        }
        mounted = true;
    }

    /* First free name: LOG00001.CSV, LOG00002.CSV, ... (8.3 names because
     * long file names are turned off to save RAM). */
    for (unsigned i = 1; i <= MAX_LOG_FILES; i++) {
        snprintf(name, sizeof(name), "LOG%05u.CSV", i);
        fr = f_open(&file, name, FA_WRITE | FA_CREATE_NEW);
        if (fr == FR_OK) {
            break;
        }
        if (fr != FR_EXIST) {
            console_printf("sd: open %s failed (FatFs error %d)\r\n", name, (int)fr);
            g_stats.sd_errors++;
            mounted = false; /* card may have been pulled; remount next time */
            return false;
        }
    }
    if (fr != FR_OK) {
        console_printf("sd: no free file names\r\n");
        return false;
    }

    block_used = 0;
    append(header, sizeof(header) - 1);
    console_printf("log: started %s\r\n", name);
    return true;
}

static void stop_session(void)
{
    log_record_t r;

    /* Write whatever is still queued, then the partial last block. */
    while (xQueueReceive(g_log_q, &r, 0) == pdTRUE) {
        write_record(&r);
    }
    if (block_used > 0) {
        write_bytes(block, (UINT)block_used);
        block_used = 0;
    }
    if (f_close(&file) != FR_OK) {
        g_stats.sd_errors++;
    }
    console_printf("log: stopped, %lu rows so far\r\n", (unsigned long)g_stats.log_rows);
}

void logger_task(void *arg)
{
    TickType_t last_sync = 0;

    (void)arg;

    for (;;) {
        uint32_t cmd = 0;
        log_record_t r;

        /* Commands from the UI task, without waiting. */
        xTaskNotifyWait(0, UINT32_MAX, &cmd, 0);
        if (cmd & LOG_CMD_TOGGLE) {
            if (!g_stats.logging) {
                if (start_session()) {
                    last_sync = xTaskGetTickCount();
                    g_stats.logging = true; /* sampling task starts queueing now */
                }
            } else {
                g_stats.logging = false;    /* stop new samples first */
                stop_session();
            }
        }

        if (xQueueReceive(g_log_q, &r, pdMS_TO_TICKS(50)) == pdTRUE && g_stats.logging) {
            write_record(&r);
        }

        /* f_sync updates the FAT and directory entry, so a session is
         * readable even if the power is cut before the button is pressed. */
        if (g_stats.logging && xTaskGetTickCount() - last_sync >= pdMS_TO_TICKS(SYNC_PERIOD_MS)) {
            last_sync = xTaskGetTickCount();
            if (f_sync(&file) != FR_OK) {
                g_stats.sd_errors++;
            }
        }
    }
}
