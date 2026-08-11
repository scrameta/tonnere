/*
 * fx_spi_sd_driver.c — FileX media driver over the SPI SD block layer.
 *
 * Mirrors the request dispatch of the SDIO driver (fx_stm32_sd_driver) but the
 * transport is synchronous SPI (spi_sd_read/write) — so there is NO DMA, no
 * transfer semaphore and no completion callbacks. Each request reads/writes
 * blocks inline and returns.
 *
 * Partition-aware like the SDIO driver: BOOT_READ follows an MBR to the first
 * partition's boot sector via _fx_partition_offset_calculate, and FileX then
 * applies fx_media_hidden_sectors to subsequent READ/WRITE itself.
 */
#include "fx_api.h"
#include "spi_sd.h"

#if defined(FPGA_BUS_STM32)

/* Provided by FileX (same forward-decl the SDIO driver uses). */
UINT _fx_partition_offset_calculate(void *partition_sector, UINT partition,
                                    ULONG *partition_start, ULONG *partition_size);

/* Scratch for the boot-sector partition probe. */
static UCHAR s_boot[512] __attribute__((aligned(4)));

VOID fx_spi_sd_driver(FX_MEDIA *media_ptr)
{
    ULONG lba;
    spi_sd_status_t st;

    switch (media_ptr->fx_media_driver_request) {

    case FX_DRIVER_INIT:
        /* Card was already brought up by spi_sd_init() at boot; nothing to do
         * here but report success/failure. */
        media_ptr->fx_media_driver_status =
            spi_sd_present() ? FX_SUCCESS : FX_IO_ERROR;
        break;

    case FX_DRIVER_UNINIT:
        media_ptr->fx_media_driver_status = FX_SUCCESS;
        break;

    case FX_DRIVER_BOOT_READ: {
        /* Read sector 0, follow an MBR partition table if present. */
        if (spi_sd_read(0, s_boot, 1) != SPI_SD_OK) {
            media_ptr->fx_media_driver_status = FX_IO_ERROR; break;
        }
        ULONG pstart = 0, psize = 0;
        UINT s = _fx_partition_offset_calculate(s_boot, 0, &pstart, &psize);
        if (s != FX_SUCCESS) { media_ptr->fx_media_driver_status = FX_IO_ERROR; break; }
        /* Hand FileX the partition's boot sector (or sector 0 if unpartitioned). */
        st = spi_sd_read(pstart, media_ptr->fx_media_driver_buffer, 1);
        media_ptr->fx_media_driver_status = (st == SPI_SD_OK) ? FX_SUCCESS : FX_IO_ERROR;
        break;
    }

    case FX_DRIVER_BOOT_WRITE:
        lba = media_ptr->fx_media_hidden_sectors;
        st = spi_sd_write(lba, media_ptr->fx_media_driver_buffer, 1);
        media_ptr->fx_media_driver_status = (st == SPI_SD_OK) ? FX_SUCCESS : FX_IO_ERROR;
        break;

    case FX_DRIVER_READ:
        lba = media_ptr->fx_media_driver_logical_sector +
              media_ptr->fx_media_hidden_sectors;
        st = spi_sd_read(lba, media_ptr->fx_media_driver_buffer,
                         media_ptr->fx_media_driver_sectors);
        media_ptr->fx_media_driver_status = (st == SPI_SD_OK) ? FX_SUCCESS : FX_IO_ERROR;
        break;

    case FX_DRIVER_WRITE:
        lba = media_ptr->fx_media_driver_logical_sector +
              media_ptr->fx_media_hidden_sectors;
        st = spi_sd_write(lba, media_ptr->fx_media_driver_buffer,
                          media_ptr->fx_media_driver_sectors);
        media_ptr->fx_media_driver_status = (st == SPI_SD_OK) ? FX_SUCCESS : FX_IO_ERROR;
        break;

    case FX_DRIVER_FLUSH:
    case FX_DRIVER_ABORT:
        /* Synchronous writes — nothing buffered to flush. */
        media_ptr->fx_media_driver_status = FX_SUCCESS;
        break;

    default:
        media_ptr->fx_media_driver_status = FX_IO_ERROR;
        break;
    }
}

#endif /* FPGA_BUS_STM32 */
