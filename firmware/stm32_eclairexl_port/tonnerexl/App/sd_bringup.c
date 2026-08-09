/*
 * sd_bringup.c — step (i) 4-bit SDIO bring-up: detect, init, read blocks.
 *
 * BOARD-ONLY (touches HAL_SD). Polls the card-detect pin (PC13, plain GPIO
 * input — not EXTI), and on a stable insert: inits SDIO, switches to 4-bit,
 * reads block 0 (blocking, signature check), then mounts via FileX and lists
 * the root directory. On remove: unmounts and deinits. Repeatable.
 *
 * Two read paths are exercised: the block-0 smoke test uses the BLOCKING HAL
 * read (no DMA/IRQ/semaphore — isolates "can we read a block"); the FileX mount
 * then uses the DMA path (HAL_SD_ReadBlocks_DMA via DMA2 Stream3 + the SDIO/DMA
 * IRQ handlers), so a successful mount proves the DMA plumbing too.
 *
 * DMA buffer placement: the F407 SDIO DMA CANNOT reach CCM RAM (0x10000000).
 * s_block_buf below is an ordinary static (lands in .bss in regular SRAM at
 * 0x20000000) and is 4-byte aligned — safe for when DMA reads use it too.
 * Do NOT move it to CCM.
 */
#include "tx_api.h"
#include "logger.h"
#include "main.h"          /* SD_DETECT_Pin / _GPIO_Port, HAL */
#include "fx_api.h"
#include "fx_stm32_sd_driver.h"   /* fx_stm32_sd_driver entry */
#include "simplefile_filex.h"     /* simplefile_bind_media / unbind */
#include "simpledir.h"            /* dir_entries / dir_next / ... */

/* Card-detect polarity. Most microSD sockets pull the detect pin LOW when a
 * card is present (switch to GND). If your socket is the opposite, flip this. */
#ifndef SD_CD_PRESENT_STATE
#define SD_CD_PRESENT_STATE  GPIO_PIN_RESET
#endif

extern SD_HandleTypeDef hsd;

/* Initialise the SDIO/card ourselves rather than calling MX_SDIO_SD_Init():
 * that generated function calls Error_Handler() (which does __disable_irq();
 * while(1)) on any failure — a hard hang. On a marginal/bouncing insert we want
 * to fail gracefully and retry on the next poll, not freeze the board.
 * Returns HAL_OK on success. */
static HAL_StatusTypeDef sd_hw_init(void)
{
    log_printf("  SD: HAL init begin (handle-state=%u tick=%lu)\r\n",
               (unsigned)hsd.State, (unsigned long)HAL_GetTick());

    hsd.Instance                 = SDIO;
    hsd.Init.ClockEdge           = SDIO_CLOCK_EDGE_RISING;
    hsd.Init.ClockBypass         = SDIO_CLOCK_BYPASS_DISABLE;
    hsd.Init.ClockPowerSave      = SDIO_CLOCK_POWER_SAVE_DISABLE;
    hsd.Init.BusWide             = SDIO_BUS_WIDE_1B;   /* ID at 1-bit, widen after */
    hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd.Init.ClockDiv            = 0;

    log_puts("  SD: calling HAL_SD_Init (1-bit identification)...\r\n");
    HAL_StatusTypeDef s = HAL_SD_Init(&hsd);
    log_printf("  SD: HAL_SD_Init returned %d (state=%u error=0x%08lX tick=%lu)\r\n",
               (int)s, (unsigned)hsd.State, (unsigned long)hsd.ErrorCode,
               (unsigned long)HAL_GetTick());
    if (s != HAL_OK) return s;

    /* Switch to 4-bit. If this fails, deinit so the next attempt starts clean. */
    log_puts("  SD: switching bus to 4-bit...\r\n");
    s = HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B);
    log_printf("  SD: 4-bit switch returned %d (state=%u error=0x%08lX)\r\n",
               (int)s, (unsigned)hsd.State, (unsigned long)hsd.ErrorCode);
    if (s != HAL_OK) {
        log_puts("  SD: deinitialising after 4-bit switch failure...\r\n");
        HAL_SD_DeInit(&hsd);
        return s;
    }

    return HAL_OK;
}

/* DMA-safe scratch buffer: static (regular SRAM, not CCM), word-aligned. */
static uint8_t s_block_buf[512] __attribute__((aligned(4)));

/* FileX media object + its working buffer. FileX uses this as its logical-
 * sector cache; for FAT32 with a large root directory, one sector (512) is not
 * enough — the mount succeeds but directory enumeration comes back empty. The
 * host build uses 8 sectors for exactly this reason, so match it. Must be in
 * regular SRAM (not CCM) since the FileX SD driver reads into it via DMA. */
static FX_MEDIA s_media;
static uint8_t  s_media_buf[512 * 8] __attribute__((aligned(4)));
static int      s_media_open;

/* SimpleDir does not allocate memory: dir_init() must give it an arena before
 * the first dir_entries() call.  The host interactive program did this, but
 * the board path did not, so every on-board listing returned NULL without
 * ever asking FileX to read the directory.  8 KiB holds roughly 24 entries;
 * a larger directory is returned as a safely truncated list.  Do not put this
 * in CCM: USBX already reserves that entire 64 KiB bank on this target. */
#define SD_DIR_ARENA_SIZE (8u * 1024u)
static uint8_t s_dir_arena[SD_DIR_ARENA_SIZE] __attribute__((aligned(4)));

static int card_present(void)
{
    return HAL_GPIO_ReadPin(SD_DETECT_GPIO_Port, SD_DETECT_Pin) == SD_CD_PRESENT_STATE;
}

/* Wait for the card to reach the idle TRANSFER state (4). After init and the
 * 4-bit bus-width switch the card can briefly sit in SENDING (5) or other
 * states; reading before it settles fails. Returns 1 if TRANSFER reached. */
static int sd_wait_transfer(ULONG timeout_ticks)
{
    ULONG t0 = tx_time_get();
    for (;;) {
        HAL_SD_CardStateTypeDef st = HAL_SD_GetCardState(&hsd);
        if (st == HAL_SD_CARD_TRANSFER) return 1;
        if (tx_time_get() - t0 > timeout_ticks) {
            log_printf("  SD: not TRANSFER-ready (state=%lu)\r\n", (unsigned long)st);
            return 0;
        }
        tx_thread_sleep(1);   /* yield ~10ms and re-check */
    }
}

/* Read one 512-byte block. Uses the BLOCKING (polling) HAL read, not the DMA
 * variant: this is a pre-FileX smoke test, and the DMA path's transfer
 * semaphore is only created when FileX initialises the driver. Blocking read
 * needs no semaphore/IRQ, so it cleanly isolates "can we read a block at all"
 * from the DMA plumbing (which FileX exercises later when it mounts).
 * Returns 1 on success. */
static int sd_read_block(uint32_t block)
{
    if (!sd_wait_transfer(50 /* ~500ms */)) return 0;   /* card must be idle first */

    if (HAL_SD_ReadBlocks(&hsd, s_block_buf, block, 1, 1000 /* ms */) != HAL_OK) {
        log_printf("  SD: ReadBlocks failed (state=%lu)\r\n",
                   (unsigned long)HAL_SD_GetCardState(&hsd));
        return 0;
    }
    return sd_wait_transfer(50);   /* let it return to idle after the read */
}

/* Mount the card via FileX and bind it to the SimpleFile/SimpleDir layer. This
 * is what exercises the DMA read/write path (the FileX SD driver uses
 * HAL_SD_ReadBlocks_DMA). Returns 1 on success. */
static int sd_mount(void)
{
    /* FileX requires a one-time fx_system_initialize() before any fx_media_open
     * — it sets the system build-options / version globals that fx_media_open
     * checks (otherwise it returns FX_NOT_IMPLEMENTED 0x22). The CubeMX
     * MX_FileX_Init stub never calls it, so do it here, once. Safe to call once
     * at startup; guarded so repeated mounts don't repeat it. */
    static int s_fx_inited = 0;
    if (!s_fx_inited) { fx_system_initialize(); s_fx_inited = 1; }

    /* fx_media_open(media, name, driver, driver_info, mem_ptr, mem_size).
     * The stm32 SD driver ignores driver_info; mem buffer is one sector. */
    UINT s = fx_media_open(&s_media, "sd", fx_stm32_sd_driver, FX_NULL,
                           s_media_buf, sizeof(s_media_buf));
    if (s != FX_SUCCESS) {
        log_printf("  SD: fx_media_open failed (0x%02X)\r\n", s);
        return 0;
    }
    s_media_open = 1;
    simplefile_bind_media(&s_media);

    /* Diagnostics: show what FileX worked out about the volume, so a wrong
     * partition offset / geometry is visible rather than silent. */
    log_printf("  SD: mounted — hidden=%lu total=%lu bytes/sec=%lu sec/clus=%lu\r\n",
               (unsigned long)s_media.fx_media_hidden_sectors,
               (unsigned long)s_media.fx_media_total_sectors,
               (unsigned long)s_media.fx_media_bytes_per_sector,
               (unsigned long)s_media.fx_media_sectors_per_cluster);

    unsigned fat_bits = (s_media.fx_media_FAT_type == FX_FAT12) ? 12u :
                        (s_media.fx_media_FAT_type == FX_FAT16) ? 16u :
                        (s_media.fx_media_FAT_type == FX_FAT32) ? 32u : 0u;
    log_printf("  SD: FAT%u (type=0x%02X) root-sector=%lu root-cluster=%lu root-sectors=%u\r\n",
               fat_bits, (unsigned)s_media.fx_media_FAT_type,
               (unsigned long)s_media.fx_media_root_sector_start,
               (unsigned long)s_media.fx_media_root_cluster_32,
               (unsigned)s_media.fx_media_root_sectors);

    /* This is a required part of SimpleDir initialisation, independent of
     * FileX media mounting.  Do it on every mount so a future implementation
     * can reset any arena bookkeeping when a card is exchanged. */
    if (dir_init(s_dir_arena, sizeof(s_dir_arena)) != SimpleFile_OK) {
        log_printf("  SD: dir_init failed (arena=%p, bytes=%u)\r\n",
                   (void *)s_dir_arena, (unsigned)sizeof(s_dir_arena));
        simplefile_unbind_media();
        fx_media_close(&s_media);
        s_media_open = 0;
        return 0;
    }
    log_printf("  SD: SimpleDir arena ready (%u bytes)\r\n",
               (unsigned)sizeof(s_dir_arena));

    return 1;
}

static void sd_unmount(void)
{
    if (s_media_open) {
        simplefile_unbind_media();
        fx_media_close(&s_media);
        s_media_open = 0;
    }
}

/* List the root directory through the SimpleDir layer (the same path the menu
 * will use). Logs each entry with size / <dir> marker. */
static void sd_list_root(void)
{
    struct SimpleDirEntry *e = dir_entries("/");
    if (e == NULL) {
        log_puts("  SD: (root empty or unreadable)\r\n");
        return;
    }
    int n = 0;
    for (struct SimpleDirEntry *p = e; p != NULL; p = dir_next(p)) {
        if (dir_is_subdir(p))
            log_printf("    <dir>  %s\r\n", dir_filename(p));
        else
            log_printf("    %6d  %s\r\n", dir_filesize(p), dir_filename(p));
        if (++n >= 64) { log_puts("    ...(truncated)\r\n"); break; }
    }
    if (n == 0) log_puts("  SD: (root empty)\r\n");
}

/* Standalone DMA-read probe: isolates the HAL_SD_ReadBlocks_DMA path (the one
 * FileX uses and that hangs) from FileX itself, with full logging. Creates its
 * own semaphore, fires a single-block DMA read, and reports exactly where it
 * gets stuck: start error, completion timeout (IRQ/callback not firing), or
 * success. Uses the same transfer_semaphore + callbacks the FileX driver uses,
 * so the completion path is identical. */
extern TX_SEMAPHORE transfer_semaphore;
extern volatile uint32_t s_last_sd_error;   /* set by HAL_SD_ErrorCallback in glue */

__attribute__((unused)) static void sd_dma_read_test(void)
{
    /* Make our own semaphore so we can test DMA before FileX inits. Delete
     * first in case it exists; ignore errors. */
    tx_semaphore_delete(&transfer_semaphore);
    if (tx_semaphore_create(&transfer_semaphore, "sd xfer test", 0) != TX_SUCCESS) {
        log_puts("  SD DMA test: semaphore create failed\r\n");
        return;
    }

    if (!sd_wait_transfer(50)) { log_puts("  SD DMA test: card not ready\r\n"); goto done; }

    log_puts("  SD DMA test: starting HAL_SD_ReadBlocks_DMA(block 0)...\r\n");
    HAL_StatusTypeDef st = HAL_SD_ReadBlocks_DMA(&hsd, s_block_buf, 0, 1);
    if (st != HAL_OK) {
        log_printf("  SD DMA test: start FAILED (HAL=%d, cardstate=%lu)\r\n",
                   (int)st, (unsigned long)HAL_SD_GetCardState(&hsd));
        goto done;
    }
    log_puts("  SD DMA test: DMA started, waiting for completion callback...\r\n");

    /* Wait up to ~2s for the Rx callback to post the semaphore. */
    UINT g = tx_semaphore_get(&transfer_semaphore, 200);
    if (g != TX_SUCCESS) {
        log_printf("  SD DMA test: TIMEOUT — completion callback never fired "
                   "(cardstate=%lu). DMA IRQ/callback path broken.\r\n",
                   (unsigned long)HAL_SD_GetCardState(&hsd));
        HAL_SD_Abort(&hsd);
        goto done;
    }
    log_printf("  SD DMA test: completion FIRED — first bytes %02X %02X %02X %02X, "
               "sig=0x%04X\r\n", s_block_buf[0], s_block_buf[1], s_block_buf[2],
               s_block_buf[3], (uint16_t)(s_block_buf[510] | (s_block_buf[511]<<8)));

    /* Now a MULTI-sector DMA read — FileX reads directory/FAT in multi-sector
     * bursts, and single vs multi-sector can behave differently (this is the
     * likely differentiator, since single-block FileX reads for geometry work
     * but directory enumeration hangs). Read 8 blocks into the media buffer. */
    if (!sd_wait_transfer(50)) { log_puts("  SD DMA test: not ready for multi\r\n"); goto done; }
    log_puts("  SD DMA test: starting 8-block DMA read...\r\n");
    st = HAL_SD_ReadBlocks_DMA(&hsd, s_media_buf, 0, 8);
    if (st != HAL_OK) {
        log_printf("  SD DMA test: 8-block start FAILED (HAL=%d)\r\n", (int)st);
        goto done;
    }
    g = tx_semaphore_get(&transfer_semaphore, 200);
    if (g != TX_SUCCESS) {
        log_printf("  SD DMA test: 8-block TIMEOUT (cardstate=%lu) — MULTI-sector "
                   "DMA is the problem.\r\n", (unsigned long)HAL_SD_GetCardState(&hsd));
        HAL_SD_Abort(&hsd);
        goto done;
    }
    log_puts("  SD DMA test: 8-block completion FIRED — multi-sector DMA OK\r\n");

    /* Read the sectors FileX actually touches during mount, which differ from
     * block 0: the FAT (right after the reserved+hidden area) and the root dir
     * (sector 736 per the host dump). If one of THESE errors, we catch it here
     * with the error code instead of hanging inside fx_media_open. */
    {
        uint32_t probe[] = { 247 /*partition boot*/, 248, 736 /*root dir*/ };
        for (unsigned k = 0; k < sizeof probe/sizeof probe[0]; k++) {
            if (!sd_wait_transfer(50)) { log_printf("  SD DMA test: not ready @%lu\r\n",(unsigned long)probe[k]); break; }
            s_last_sd_error = 0;
            st = HAL_SD_ReadBlocks_DMA(&hsd, s_block_buf, probe[k], 1);
            if (st != HAL_OK) { log_printf("  SD DMA test: sector %lu start FAIL HAL=%d\r\n",(unsigned long)probe[k],(int)st); continue; }
            g = tx_semaphore_get(&transfer_semaphore, 100);
            if (g != TX_SUCCESS) {
                log_printf("  SD DMA test: sector %lu TIMEOUT err=0x%08lX state=%lu\r\n",
                           (unsigned long)probe[k], (unsigned long)s_last_sd_error,
                           (unsigned long)HAL_SD_GetCardState(&hsd));
                HAL_SD_Abort(&hsd);
            } else if (s_last_sd_error) {
                log_printf("  SD DMA test: sector %lu ERROR 0x%08lX\r\n",
                           (unsigned long)probe[k], (unsigned long)s_last_sd_error);
            } else {
                log_printf("  SD DMA test: sector %lu OK (%02X %02X %02X %02X)\r\n",
                           (unsigned long)probe[k], s_block_buf[0], s_block_buf[1],
                           s_block_buf[2], s_block_buf[3]);
            }
        }
    }
done:
    tx_semaphore_delete(&transfer_semaphore);
}

/* Init the inserted card and read block 0 as a smoke test. Returns 1 on OK. */
static int sd_init_and_probe(void)
{
    log_puts("SD: card inserted, initialising...\r\n");

    /* Let the card power up and the contacts settle before talking to it —
     * cuts down on init failures from a still-seating card. */
    tx_thread_sleep(5);   /* ~50ms */
    log_printf("  SD: power-up delay complete (tick=%lu, detect=%d)\r\n",
               (unsigned long)HAL_GetTick(), card_present());

    HAL_StatusTypeDef s = sd_hw_init();
    if (s != HAL_OK) {
        log_printf("  SD: init failed (HAL=%d) — will retry\r\n", (int)s);
        return 0;   /* graceful: retried on next poll, no Error_Handler hang */
    }

    HAL_SD_CardInfoTypeDef info;
    if (HAL_SD_GetCardInfo(&hsd, &info) == HAL_OK) {
        log_printf("  SD: type=%lu blocks=%lu blocksize=%lu (%lu MB)\r\n",
                   (unsigned long)info.CardType,
                   (unsigned long)info.LogBlockNbr,
                   (unsigned long)info.LogBlockSize,
                   (unsigned long)((uint64_t)info.LogBlockNbr * info.LogBlockSize / (1024*1024)));
    }

    if (!sd_read_block(0)) { HAL_SD_DeInit(&hsd); return 0; }

    /* MBR / boot sector signature is 0x55 0xAA at offset 510. */
    uint16_t sig = (uint16_t)(s_block_buf[510] | (s_block_buf[511] << 8));
    log_printf("  SD: block0 sig=0x%04X %s | first bytes %02X %02X %02X %02X\r\n",
               sig, (sig == 0xAA55) ? "(valid)" : "(no signature)",
               s_block_buf[0], s_block_buf[1], s_block_buf[2], s_block_buf[3]);

    /* Isolate the DMA read path (what FileX uses) with logging, BEFORE mounting,
     * so a DMA/IRQ/callback problem is pinpointed rather than hidden inside the
     * FileX mount. Set to 0 to skip (to check whether the test itself perturbs
     * SD/semaphore state and causes the subsequent mount hang). */
#ifndef SD_RUN_DMA_TEST
#define SD_RUN_DMA_TEST 0
#endif
#if SD_RUN_DMA_TEST
    sd_dma_read_test();
#endif

    /* Mount via FileX (exercises the DMA path) and list the root directory. */
    if (!sd_mount()) { HAL_SD_DeInit(&hsd); return 0; }
    log_puts("  SD: mounted, root:\r\n");
    sd_list_root();
    return 1;
}

/* Public entry: run the detect/init/read/remove lifecycle. Called in a loop
 * from the sdlife thread; polls the CD pin with simple debounce. */
/* Poll states: no card / mounted OK / tried-and-failed (wait for removal so we
 * don't re-init every 100ms). */
enum { SD_IDLE, SD_MOUNTED, SD_FAILED };

void sd_bringup_poll(void)
{
    static int s_state = SD_IDLE;
    static int s_last = -1;      /* debounce: require 2 equal reads */
    int now = card_present();

    if (now != s_last) { s_last = now; return; }   /* wait for a stable reading */

    switch (s_state) {
    case SD_IDLE:
        if (now) {
            if (sd_init_and_probe()) {
                log_puts("SD: ready\r\n");
                s_state = SD_MOUNTED;
            } else {
                /* Give up until the card is removed — avoids a re-init storm on
                 * a bad card/seat. DeInit so a fresh insert starts clean. */
                HAL_SD_DeInit(&hsd);
                s_state = SD_FAILED;
            }
        }
        break;
    case SD_MOUNTED:
        if (!now) {
            log_puts("SD: card removed\r\n");
            /* sd_unmount()'s fx_media_close issues FX_DRIVER_UNINIT, which
             * already calls HAL_SD_DeInit AND deletes transfer_semaphore (via
             * the driver's POST_DEINIT). Do NOT HAL_SD_DeInit again here — a
             * second deinit on already-deinited hardware can wedge the thread,
             * which stops all further polling (symptom: re-insert does nothing). */
            sd_unmount();
            s_state = SD_IDLE;
        }
        break;
    case SD_FAILED:
        if (!now) {           /* card pulled — allow a retry on next insert */
            s_state = SD_IDLE;
        }
        break;
    }
}
