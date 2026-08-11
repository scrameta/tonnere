/*
 * spi_sd.c — SD/microSD over SPI. See spi_sd.h.
 *
 * Protocol summary (SD Physical Layer / SPI mode):
 *   - Power-up: >=74 clocks with CS high and MOSI high to put the card in a
 *     known state, then CS low and CMD0 (GO_IDLE) to enter SPI/idle.
 *   - CMD8 (SEND_IF_COND): probes voltage range; success => v2 card (may be SDHC).
 *   - ACMD41 (APP_SEND_OP_COND) with HCS bit, polled until the card leaves idle.
 *   - CMD58 (READ_OCR): CCS bit says byte-addressed (SDSC) vs block-addressed
 *     (SDHC/SDXC). SDSC needs CMD16 to force 512-byte blocks.
 *   - CMD17/CMD18 read, CMD24/CMD25 write, with data tokens + CRC (CRC ignored
 *     in SPI mode after CMD0, except CMD0/CMD8 which we send correct CRCs for).
 *
 * All transfers are polled (HAL_SPI_TransmitReceive). No DMA, no interrupts, no
 * semaphores — SPI SD is simple and slow; correctness over speed here.
 */
#include "spi_sd.h"
#include "main.h"     /* hspi1, SPI_CS1_Pin/_GPIO_Port, HAL */
#include "logger.h"

#if defined(FPGA_BUS_STM32)

/* Diagnostic: log every spi_sd_read (LBA, count, result, first bytes) to find
 * which read fails during FileX mount/enumerate. Off by default. */
#ifndef SPI_SD_TRACE_READS
#define SPI_SD_TRACE_READS 0
#endif

extern SPI_HandleTypeDef hspi1;

/* ---- card state ---------------------------------------------------------- */
static int      s_present;
static int      s_block_addressed;   /* 1 = SDHC/SDXC (addr in blocks) */
static uint32_t s_block_count;

/* ---- low-level SPI ------------------------------------------------------- */
#define CS_LOW()   HAL_GPIO_WritePin(SPI_CS1_GPIO_Port, SPI_CS1_Pin, GPIO_PIN_RESET)
#define CS_HIGH()  HAL_GPIO_WritePin(SPI_CS1_GPIO_Port, SPI_CS1_Pin, GPIO_PIN_SET)

static uint8_t xfer(uint8_t out)
{
    uint8_t in = 0xFF;
    HAL_SPI_TransmitReceive(&hspi1, &out, &in, 1, 100);
    return in;
}

/* Clock out 0xFF bytes (card uses these idle clocks to do work). */
static void clock_bytes(int n)
{
    while (n-- > 0) (void)xfer(0xFF);
}

/* Wait for the card to return non-0xFF (a response byte). Returns it, or 0xFF
 * on timeout. */
static uint8_t wait_ready_byte(int tries)
{
    uint8_t r;
    do { r = xfer(0xFF); } while (r == 0xFF && --tries > 0);
    return r;
}

/* CRC7 for CMD0/CMD8 (only these are checked by the card in SPI mode). */
static uint8_t crc7(const uint8_t *d, int len)
{
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        uint8_t b = d[i];
        for (int j = 0; j < 8; j++) {
            crc <<= 1;
            if ((b & 0x80) ^ (crc & 0x80)) crc ^= 0x09;
            b <<= 1;
        }
    }
    return (crc << 1) | 1;
}

/* Send a command; returns the R1 response byte (0xFF on timeout). */
static uint8_t send_cmd(uint8_t cmd, uint32_t arg)
{
    uint8_t buf[6];
    buf[0] = 0x40 | cmd;
    buf[1] = (uint8_t)(arg >> 24);
    buf[2] = (uint8_t)(arg >> 16);
    buf[3] = (uint8_t)(arg >> 8);
    buf[4] = (uint8_t)(arg);
    buf[5] = crc7(buf, 5);

    /* An extra 0xFF before a command gives the card a clock to finish prior work. */
    (void)xfer(0xFF);
    for (int i = 0; i < 6; i++) (void)xfer(buf[i]);

    /* R1 arrives within 1..8 bytes; top bit clear marks the response. */
    uint8_t r;
    for (int i = 0; i < 8; i++) { r = xfer(0xFF); if (!(r & 0x80)) break; }
    return r;
}

/* ACMD = CMD55 then the app command. */
static uint8_t send_acmd(uint8_t cmd, uint32_t arg)
{
    send_cmd(55, 0);
    return send_cmd(cmd, arg);
}

/* ---- init ---------------------------------------------------------------- */
/* SD requires a slow clock (<=400kHz) during the CMD0/CMD8/ACMD41 init
 * handshake, then runs fast for data. CubeMX sets SPI3 to a fast prescaler for
 * throughput, so drop to the slowest prescaler (/256) for init and restore
 * after. Re-Init the peripheral to apply the new prescaler. */
static uint32_t s_saved_prescaler;
static void spi_set_slow(void)
{
    s_saved_prescaler = hspi1.Init.BaudRatePrescaler;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
    HAL_SPI_Init(&hspi1);
}
static void spi_set_fast(void)
{
    hspi1.Init.BaudRatePrescaler = s_saved_prescaler;
    HAL_SPI_Init(&hspi1);
}

/* Diagnostic probe: distinguishes "bus held by something else" vs "card not
 * selected" vs "card absent but bus free" before the CMD0 attempt. Call with
 * the slow clock already set. */
static void spi_sd_probe(void)
{
    /* 1. MISO level with CS1 HIGH (card deselected) and bus idle. A free bus
     *    floats/pulls high → reading the pin gives 1. If it reads 0, something
     *    (FPGA logic or the flash on CS2) is actively driving MISO low. */
    CS_HIGH();
    HAL_GPIO_WritePin(SPI_CS2_GPIO_Port, SPI_CS2_Pin, GPIO_PIN_SET); /* flash deselected too */
    HAL_Delay(1);
    GPIO_PinState miso_idle = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4);   /* PB4 = SPI3 MISO */

    /* 2. Clock 4 idle bytes with NOTHING selected. A free bus returns 0xFF. */
    uint8_t idle0 = xfer(0xFF), idle1 = xfer(0xFF);

    /* 3. CS1 is open-drain with a 47K external pull-up (by design — avoids a
     *    second push-pull driver on this line). Open-drain + weak pull-up rises
     *    slowly (RC), so read the "high" state BOTH immediately and after a
     *    settle delay:
     *      hi_now=0, hi_settled=1  => just slow RC rise, harmless (CS is fine at
     *                                 the <=400kHz init rate — a bit time is 2.5us,
     *                                 plenty for a 47K pull-up to settle).
     *      hi_now=0, hi_settled=0  => line STUCK low: something else holds it, a
     *                                 real fault (bad solder, short, or a second
     *                                 driver). */
    CS_LOW();
    GPIO_PinState cs_low_rb = HAL_GPIO_ReadPin(SPI_CS1_GPIO_Port, SPI_CS1_Pin);
    CS_HIGH();
    GPIO_PinState cs_hi_now = HAL_GPIO_ReadPin(SPI_CS1_GPIO_Port, SPI_CS1_Pin);
    HAL_Delay(1);   /* let the 47K pull-up bring the line up */
    GPIO_PinState cs_hi_settled = HAL_GPIO_ReadPin(SPI_CS1_GPIO_Port, SPI_CS1_Pin);

    log_printf("  SPI-SD: probe MISO-idle=%d idle-bytes=%02X %02X "
               "CS1(lo=%d hi_now=%d hi_settled=%d)\r\n",
               (int)miso_idle, idle0, idle1,
               (int)cs_low_rb, (int)cs_hi_now, (int)cs_hi_settled);
    log_puts("    (bus free if MISO-idle=1 & idle=FF; CS ok if hi_settled=1; "
             "hi_settled=0 => CS stuck low = real fault)\r\n");
}

spi_sd_status_t spi_sd_init(uint32_t timeout_ms)
{
    s_present = 0; s_block_addressed = 0; s_block_count = 0;

    spi_set_slow();    /* <=400kHz for the init handshake */

    spi_sd_probe();    /* report bus state before CMD0 */

    /* Power-up clocks with CS HIGH (card must see >=74 clocks deselected).
     * Send a generous train — some cards need more than the minimum. */
    CS_HIGH();
    clock_bytes(20);   /* 160 clocks */

    CS_LOW();

    /* CMD0: go idle (enter SPI mode). Expect R1 = 0x01 (idle). Retry a few
     * times — the first attempts right after power-up can miss while the card
     * finishes its internal power-on, and a stray clock can desync the first.
     * Also watch MISO across the whole attempt: if it NEVER goes low, the card
     * isn't driving the line at all (power / MISO wiring / card absent), which
     * is a different fault from a CS problem. */
    uint8_t r = 0xFF;
    int miso_ever_low = 0;
    for (int attempt = 0; attempt < 10; attempt++) {
        r = send_cmd(0, 0);
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) == GPIO_PIN_RESET) miso_ever_low = 1;
        if (r == 0x01) break;
        clock_bytes(2);   /* nudge the card and retry */
    }
    if (r != 0x01) {
        /* No or wrong response => treat as no card so boot proceeds quickly. */
        CS_HIGH(); (void)xfer(0xFF);
        spi_set_fast();
        log_printf("  SPI-SD: no card (CMD0 r=0x%02X, MISO-went-low=%d)\r\n",
                   r, miso_ever_low);
        log_puts("    (MISO-went-low=0 => card never drove the line: power/"
                 "MISO-wiring/absent, not CS)\r\n");
        return SPI_SD_NO_CARD;
    }

    /* CMD8: voltage check. 0x1AA = 2.7-3.6V, check pattern 0xAA.
     * v2 cards echo the low 12 bits in the R7 trailer. */
    int v2 = 0;
    r = send_cmd(8, 0x000001AA);
    if (r == 0x01) {
        uint8_t t[4]; for (int i = 0; i < 4; i++) t[i] = xfer(0xFF);
        if (t[2] == 0x01 && t[3] == 0xAA) v2 = 1;   /* voltage+pattern echoed */
    }
    /* r with illegal-command bit => v1 card; that's fine, v2 stays 0. */

    /* ACMD41 ready-wait. HCS bit (0x40000000) only meaningful for v2. */
    uint32_t hcs = v2 ? 0x40000000u : 0;
    uint32_t start = HAL_GetTick();
    do {
        r = send_acmd(41, hcs);
        if (r == 0x00) break;                 /* left idle => ready */
        if (HAL_GetTick() - start > timeout_ms) {
            CS_HIGH(); (void)xfer(0xFF);
            spi_set_fast();
            log_printf("  SPI-SD: init timeout (ACMD41 r=0x%02X)\r\n", r);
            return SPI_SD_INIT_FAIL;
        }
    } while (1);

    /* CMD58: read OCR, check CCS (bit30) for block vs byte addressing. */
    if (v2) {
        r = send_cmd(58, 0);
        if (r == 0x00) {
            uint8_t ocr[4]; for (int i = 0; i < 4; i++) ocr[i] = xfer(0xFF);
            s_block_addressed = (ocr[0] & 0x40) ? 1 : 0;   /* CCS */
        }
    }

    /* SDSC (byte-addressed): force 512-byte block length. */
    if (!s_block_addressed) {
        r = send_cmd(16, SPI_SD_BLOCK_SIZE);
        if (r != 0x00) {
            CS_HIGH(); (void)xfer(0xFF);
            spi_set_fast();
            log_printf("  SPI-SD: CMD16 set-blocklen failed (r=0x%02X)\r\n", r);
            return SPI_SD_INIT_FAIL;
        }
    }

    /* Read the CSD (CMD9) to compute capacity. */
    s_block_count = 0;
    if (send_cmd(9, 0) == 0x00) {
        /* wait for data token 0xFE, then 16 CSD bytes + 2 CRC */
        uint8_t tok = wait_ready_byte(2000);
        if (tok == 0xFE) {
            uint8_t csd[16];
            for (int i = 0; i < 16; i++) csd[i] = xfer(0xFF);
            (void)xfer(0xFF); (void)xfer(0xFF);   /* CRC */
            if ((csd[0] >> 6) == 1) {
                /* CSD v2 (SDHC/SDXC): C_SIZE is 22 bits in csd[7..9]. */
                uint32_t c_size = ((uint32_t)(csd[7] & 0x3F) << 16) |
                                  ((uint32_t)csd[8] << 8) | csd[9];
                s_block_count = (c_size + 1) * 1024u;   /* (C_SIZE+1)*512KB / 512 */
            } else {
                /* CSD v1 (SDSC): classic C_SIZE / C_SIZE_MULT formula. */
                uint32_t c_size = ((uint32_t)(csd[6] & 0x03) << 10) |
                                  ((uint32_t)csd[7] << 2) | (csd[8] >> 6);
                uint32_t mult   = ((csd[9] & 0x03) << 1) | (csd[10] >> 7);
                uint32_t rdblen = csd[5] & 0x0F;
                uint32_t blocknr = (c_size + 1) << (mult + 2);
                uint32_t blocklen = 1u << rdblen;
                s_block_count = blocknr * (blocklen / SPI_SD_BLOCK_SIZE);
            }
        }
    }

    CS_HIGH(); (void)xfer(0xFF);   /* release + trailing clock */
    spi_set_fast();                /* handshake done — run data phase fast */
    s_present = 1;
    log_printf("  SPI-SD: ready (%s, %lu blocks, %lu MB)\r\n",
               s_block_addressed ? "SDHC" : "SDSC",
               (unsigned long)s_block_count,
               (unsigned long)((uint64_t)s_block_count * 512u / (1024u*1024u)));
    return SPI_SD_OK;
}

int      spi_sd_present(void)      { return s_present; }
uint32_t spi_sd_block_count(void)  { return s_block_count; }

/* ---- read ---------------------------------------------------------------- */
static spi_sd_status_t read_one(uint32_t addr, uint8_t *buf)
{
    if (send_cmd(17, addr) != 0x00) return SPI_SD_IO_ERROR;

    /* Wait for the start-block token 0xFE. */
    uint8_t tok = wait_ready_byte(20000);
    if (tok != 0xFE) return SPI_SD_IO_ERROR;

    for (uint32_t i = 0; i < SPI_SD_BLOCK_SIZE; i++) buf[i] = xfer(0xFF);
    (void)xfer(0xFF); (void)xfer(0xFF);   /* discard CRC */
    return SPI_SD_OK;
}

spi_sd_status_t spi_sd_read(uint32_t block, uint8_t *buf, uint32_t count)
{
    if (!s_present) return SPI_SD_UNINIT;
    CS_LOW();
    spi_sd_status_t st = SPI_SD_OK;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t lba = block + i;
        uint32_t addr = s_block_addressed ? lba : lba * SPI_SD_BLOCK_SIZE;
        st = read_one(addr, buf + i * SPI_SD_BLOCK_SIZE);
        if (st != SPI_SD_OK) break;
    }
    CS_HIGH(); (void)xfer(0xFF);
#if SPI_SD_TRACE_READS
    log_printf("  [spi rd] lba=%lu count=%lu st=%d first=%02X %02X %02X %02X\r\n",
               (unsigned long)block, (unsigned long)count, (int)st,
               buf[0], buf[1], buf[2], buf[3]);
#endif
    return st;
}

/* ---- write --------------------------------------------------------------- */
static spi_sd_status_t write_one(uint32_t addr, const uint8_t *buf)
{
    if (send_cmd(24, addr) != 0x00) return SPI_SD_IO_ERROR;

    (void)xfer(0xFF);      /* one gap byte before the data token */
    (void)xfer(0xFE);      /* start-block token */
    for (uint32_t i = 0; i < SPI_SD_BLOCK_SIZE; i++) (void)xfer(buf[i]);
    (void)xfer(0xFF); (void)xfer(0xFF);   /* dummy CRC */

    /* Data-response token: xxx00101 = accepted. */
    uint8_t resp = xfer(0xFF);
    if ((resp & 0x1F) != 0x05) return SPI_SD_IO_ERROR;

    /* Card holds MISO low while programming; wait for it to release (non-zero). */
    if (wait_ready_byte(50000) == 0xFF) { /* 0xFF means ready again */ }
    return SPI_SD_OK;
}

spi_sd_status_t spi_sd_write(uint32_t block, const uint8_t *buf, uint32_t count)
{
    if (!s_present) return SPI_SD_UNINIT;
    CS_LOW();
    spi_sd_status_t st = SPI_SD_OK;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t lba = block + i;
        uint32_t addr = s_block_addressed ? lba : lba * SPI_SD_BLOCK_SIZE;
        st = write_one(addr, buf + i * SPI_SD_BLOCK_SIZE);
        if (st != SPI_SD_OK) break;
    }
    CS_HIGH(); (void)xfer(0xFF);
    return st;
}

#endif /* FPGA_BUS_STM32 */
