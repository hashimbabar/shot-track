/*
 * diskio.c - the five functions FatFs needs to reach a disk, implemented
 * on top of sd_spi.c. FatFs only ever sees "drive 0".
 */
#include "ff.h"
#include "diskio.h"
#include "sd_spi.h"

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != 0) {
        return STA_NOINIT;
    }
    return sd_is_ready() ? 0 : STA_NOINIT;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0) {
        return STA_NOINIT;
    }
    return sd_init() == SD_OK ? 0 : STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0 || count == 0) {
        return RES_PARERR;
    }
    if (!sd_is_ready()) {
        return RES_NOTRDY;
    }
    return sd_read_blocks(buff, (uint32_t)sector, count) == SD_OK ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0 || count == 0) {
        return RES_PARERR;
    }
    if (!sd_is_ready()) {
        return RES_NOTRDY;
    }
    return sd_write_blocks(buff, (uint32_t)sector, count) == SD_OK ? RES_OK : RES_ERROR;
}

/* With formatting turned off and a fixed 512-byte sector size, FatFs only
 * asks for CTRL_SYNC. */
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    (void)buff;
    if (pdrv != 0) {
        return RES_PARERR;
    }
    if (cmd == CTRL_SYNC) {
        return sd_sync() == SD_OK ? RES_OK : RES_ERROR;
    }
    return RES_PARERR;
}
