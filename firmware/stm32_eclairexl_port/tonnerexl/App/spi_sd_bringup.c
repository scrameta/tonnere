/*
 * spi_sd_bringup.c — step (ii) internal SPI microSD "hard drive" bring-up.
 *
 * The SPI card is a raw disk for the Atari OS (accessed as a raw device later,
 * NOT through the file API in normal use). There is no card-detect and no
 * hot-plug: we try once at boot, quickly, and if it doesn't come up it's absent
 * for this boot cycle.
 *
 * SPI_SD_TEST_DIR_LISTING (default 1 for now): a BRING-UP-ONLY test that mounts
 * the card via FileX and lists its root directory, to prove the block driver
 * reads real sectors. It temporarily "pinches" the shared SimpleFile binding
 * (which the external SDIO card also uses), so it must run before / separately
 * from the SDIO card's use of SimpleFile. Turn this off (0) once the raw-disk
 * path is in place — the Atari OS reaches the card through spi_sd_read/write,
 * not FileX.
 *
 * FLAG(mark): SPI_SD_TEST_DIR_LISTING borrows the single-media SimpleFile
 * binding. It's a scaffold for testing, not the final integration (Option C:
 * SPI drive stays out of SimpleFile). Remove once raw-disk access lands.
 */
#include "tx_api.h"
#include "main.h"      /* HAL, GPIO types + pin defs for the raw drive test */
#include "spi_sd.h"
#include "logger.h"

#if defined(FPGA_BUS_STM32)

#include "fx_api.h"
#include "simplefile_filex.h"
#include "simpledir.h"

/* One-shot init timeout — keep short ("quickly or give up"). */
#ifndef SPI_SD_INIT_TIMEOUT_MS
#define SPI_SD_INIT_TIMEOUT_MS 500u
#endif

#ifndef SPI_SD_TEST_DIR_LISTING
#define SPI_SD_TEST_DIR_LISTING 1
#endif

extern VOID fx_spi_sd_driver(FX_MEDIA *media_ptr);

#if SPI_SD_TEST_DIR_LISTING
static FX_MEDIA s_spi_media;
static uint8_t  s_spi_media_buf[512 * 8] __attribute__((aligned(4)));

/* SimpleDir stores the returned linked list in a caller-provided arena.  A
 * mounted/bound FileX medium is not enough: without dir_init(), dir_entries()
 * deliberately returns NULL before it asks FileX for the first entry.  Keep a
 * separate arena for this temporary SPI-media binding so the bring-up test is
 * self-contained and does not depend on the later SDIO initialisation. */
#define SPI_SD_DIR_ARENA_SIZE (8u * 1024u)
static uint8_t s_spi_dir_arena[SPI_SD_DIR_ARENA_SIZE] __attribute__((aligned(4)));

static void spi_sd_list_root(void)
{
    struct SimpleDirEntry *e = dir_entries("/");
    if (e == NULL) { log_puts("  SPI-SD: (root empty or unreadable)\r\n"); return; }
    int n = 0;
    for (struct SimpleDirEntry *p = e; p != NULL; p = dir_next(p)) {
        if (dir_is_subdir(p)) log_printf("    <dir>  %s\r\n", dir_filename(p));
        else                  log_printf("    %6d  %s\r\n", dir_filesize(p), dir_filename(p));
        if (++n >= 64) { log_puts("    ...(truncated)\r\n"); break; }
    }
    if (n == 0) log_puts("  SPI-SD: (root empty)\r\n");
}

static void spi_sd_test_mount(void)
{
    /* fx_system_initialize() is idempotent-safe to call again; the SDIO path
     * may already have called it, but a second call is harmless. */
    fx_system_initialize();

    UINT s = fx_media_open(&s_spi_media, "spisd", fx_spi_sd_driver, FX_NULL,
                           s_spi_media_buf, sizeof(s_spi_media_buf));
    if (s != FX_SUCCESS) {
        log_printf("  SPI-SD: fx_media_open failed (0x%02X)\r\n", s);
        return;
    }
    log_printf("  SPI-SD: mounted — hidden=%lu total=%lu bytes/sec=%lu sec/clus=%lu\r\n",
               (unsigned long)s_spi_media.fx_media_hidden_sectors,
               (unsigned long)s_spi_media.fx_media_total_sectors,
               (unsigned long)s_spi_media.fx_media_bytes_per_sector,
               (unsigned long)s_spi_media.fx_media_sectors_per_cluster);

    /* Pinch the shared SimpleFile binding just for this listing test. */
    simplefile_bind_media(&s_spi_media);

    if (dir_init(s_spi_dir_arena, sizeof(s_spi_dir_arena)) != SimpleFile_OK) {
        log_printf("  SPI-SD: dir_init failed (arena=%p, bytes=%u)\r\n",
                   (void *)s_spi_dir_arena,
                   (unsigned)sizeof(s_spi_dir_arena));
        simplefile_unbind_media();
        fx_media_close(&s_spi_media);
        return;
    }
    log_printf("  SPI-SD: SimpleDir arena ready (%u bytes)\r\n",
               (unsigned)sizeof(s_spi_dir_arena));

    log_puts("  SPI-SD: root:\r\n");
    spi_sd_list_root();
    simplefile_unbind_media();

    fx_media_close(&s_spi_media);
}
#endif /* SPI_SD_TEST_DIR_LISTING */

/* Call once from the boot thread. Brings up the SPI card and (if enabled) runs
 * the directory-listing self-test. */
void spi_sd_bringup(void)
{
    log_puts("SPI-SD: initialising internal card...\r\n");
    spi_sd_status_t st = spi_sd_init(SPI_SD_INIT_TIMEOUT_MS);
    if (st != SPI_SD_OK) {
        log_puts("SPI-SD: not available this boot\r\n");
        return;
    }

#if SPI_SD_TEST_DIR_LISTING
    spi_sd_test_mount();
#endif
}

#endif /* FPGA_BUS_STM32 */
