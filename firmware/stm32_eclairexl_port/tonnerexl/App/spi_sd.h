/*
 * spi_sd.h — SD/microSD card over SPI (step (ii): internal "hard drive").
 *
 * SD in SPI mode on SPI3 (PB3 SCK / PB4 MISO / PB5 MOSI), chip-select CS1=PB6.
 * (CS2=PB7 is the FPGA flash on the same bus — kept deselected while we talk to
 * the card.) Board-only. Init is one-shot at boot: if the card doesn't come up
 * quickly it's declared absent for this boot cycle (no detect pin, no retry).
 *
 * This is the raw block layer. FileX sits on top via fx_spi_sd_driver (a
 * separate media driver), and the Atari OS will later access the same card as
 * a raw disk.
 */
#ifndef SPI_SD_H
#define SPI_SD_H

#include <stdint.h>

#define SPI_SD_BLOCK_SIZE 512u

typedef enum {
    SPI_SD_OK = 0,
    SPI_SD_NO_CARD,        /* no response to init (absent / not powered) */
    SPI_SD_INIT_FAIL,      /* card present but init sequence failed */
    SPI_SD_IO_ERROR,       /* read/write transaction failed */
    SPI_SD_UNINIT,         /* called before a successful spi_sd_init */
} spi_sd_status_t;

/* One-shot init. Returns SPI_SD_OK if a card is up and addressable.
 * timeout_ms bounds the ACMD41 ready-wait (keep small: "quickly or give up"). */
spi_sd_status_t spi_sd_init(uint32_t timeout_ms);

/* True once spi_sd_init has succeeded this boot. */
int spi_sd_present(void);

/* Total addressable 512-byte blocks (valid after a successful init). */
uint32_t spi_sd_block_count(void);

/* Read/write whole 512-byte blocks. block is a linear block address (the
 * driver handles byte- vs block-addressing internally for SDSC vs SDHC).
 * buf must hold count*512 bytes. Returns SPI_SD_OK on success. */
spi_sd_status_t spi_sd_read(uint32_t block, uint8_t *buf, uint32_t count);
spi_sd_status_t spi_sd_write(uint32_t block, const uint8_t *buf, uint32_t count);

#endif /* SPI_SD_H */
