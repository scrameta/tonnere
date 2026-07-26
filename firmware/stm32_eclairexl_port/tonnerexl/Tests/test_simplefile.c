/*
 * test_simplefile.c — exercises SimpleFile/SimpleDir on a REAL FileX RAM disk
 * (upstream linux port). Proves the adapter's open/read/seek/write, directory
 * listing into the arena, and generation-based stale-handle invalidation across
 * a simulated card swap.
 */
#include "simplefile.h"
#include "simpledir.h"
#include "simplefile_filex.h"
#include "test_harness.h"
#include "fx_api.h"
#include <stdlib.h>
#include <string.h>

VOID _fx_ram_driver(FX_MEDIA *media_ptr);

static FX_MEDIA media;
static UCHAR    media_mem[512];
static UCHAR   *ram_disk;
#define DISK_BYTES (256*1024)

static void mount_fresh(void) {
    static int fx_inited = 0;
    if (!fx_inited) { fx_system_initialize(); fx_inited = 1; }
    if (!ram_disk) ram_disk = malloc(DISK_BYTES);
    fx_media_format(&media, _fx_ram_driver, ram_disk, media_mem, sizeof media_mem,
                    "PORT", 2, 512, 0, DISK_BYTES/512, 512, 1, 1, 1);
    fx_media_open(&media, "ram", _fx_ram_driver, ram_disk, media_mem, sizeof media_mem);
    simplefile_bind_media(&media);
}

static void test_write_read_roundtrip(void) {
    mount_fresh();
    /* create a file directly via FileX, then read it through SimpleFile */
    FX_FILE f;
    CHECK(fx_file_create(&media, "DISK1.ATR") == FX_SUCCESS);
    CHECK(fx_file_open(&media, &f, "DISK1.ATR", FX_OPEN_FOR_WRITE) == FX_SUCCESS);
    const char *payload = "ATRHEADER....sector-data";
    CHECK(fx_file_write(&f, (void*)payload, (ULONG)strlen(payload)) == FX_SUCCESS);
    fx_file_close(&f);

    struct SimpleFile *sf = calloc(1, (size_t)file_struct_size());
    CHECK(file_open_name("/DISK1.ATR", sf) == SimpleFile_OK);
    CHECK_EQ_U32((uint32_t)file_size(sf), (uint32_t)strlen(payload));
    char buf[64] = {0}; int got = 0;
    CHECK(file_read(sf, buf, sizeof buf, &got) == SimpleFile_OK);
    CHECK_EQ_U32((uint32_t)got, (uint32_t)strlen(payload));
    CHECK(memcmp(buf, payload, strlen(payload)) == 0);
    /* seek back and re-read a slice */
    CHECK(file_seek(sf, 0) == SimpleFile_OK);
    char buf2[4] = {0}; int got2 = 0;
    CHECK(file_read(sf, buf2, 3, &got2) == SimpleFile_OK);
    CHECK(got2 == 3 && memcmp(buf2, "ATR", 3) == 0);
    file_close(sf);
    free(sf);
}

static void test_simplefile_write(void) {
    mount_fresh();
    FX_FILE f; fx_file_create(&media, "SAVE.BIN"); (void)f;
    struct SimpleFile *sf = calloc(1, (size_t)file_struct_size());
    CHECK(file_open_name("/SAVE.BIN", sf) == SimpleFile_OK);
    CHECK(file_readonly(sf) == 0);
    uint8_t data[16]; for (int i=0;i<16;i++) data[i]=(uint8_t)(i*3);
    int wrote = 0;
    CHECK(file_write(sf, data, 16, &wrote) == SimpleFile_OK);
    CHECK_EQ_U32((uint32_t)wrote, 16);
    file_write_flush();
    file_close(sf);
    free(sf);
    /* reopen and verify */
    struct SimpleFile *sf2 = calloc(1, (size_t)file_struct_size());
    CHECK(file_open_name("/SAVE.BIN", sf2) == SimpleFile_OK);
    uint8_t rb[16] = {0}; int got=0;
    CHECK(file_read(sf2, rb, 16, &got) == SimpleFile_OK);
    CHECK(got==16 && memcmp(rb, data, 16)==0);
    file_close(sf2);
    free(sf2);
}

static void test_dir_listing(void) {
    mount_fresh();
    fx_file_create(&media, "A.ROM");
    fx_file_create(&media, "B.ROM");
    fx_directory_create(&media, "SUB");
    static char arena[4096];
    CHECK(dir_init(arena, sizeof arena) == SimpleFile_OK);
    struct SimpleDirEntry *e = dir_entries("/");
    int files=0, dirs=0;
    for (; e; e = dir_next(e)) {
        if (dir_is_subdir(e)) dirs++; else files++;
    }
    CHECK(files >= 2);
    CHECK(dirs >= 1);
}

static void test_generation_invalidation(void) {
    mount_fresh();
    fx_file_create(&media, "OLD.ATR");
    struct SimpleFile *sf = calloc(1, (size_t)file_struct_size());
    CHECK(file_open_name("/OLD.ATR", sf) == SimpleFile_OK);
    unsigned gen_before = simplefile_generation();

    /* simulate card removal + new card: unbind then bind bumps generation */
    simplefile_unbind_media();
    mount_fresh();  /* rebinds -> new generation */
    CHECK(simplefile_generation() > gen_before);

    /* the old handle is now stale: reads must fail cleanly, not crash */
    char buf[8]; int got = 0;
    CHECK(file_read(sf, buf, sizeof buf, &got) == SimpleFile_FAIL);
    CHECK(got == 0);
    file_close(sf);
    free(sf);
}

void run_simplefile_tests(void) {
    simplefile_init_lock();
    RUN(test_write_read_roundtrip);
    RUN(test_simplefile_write);
    RUN(test_dir_listing);
    RUN(test_generation_invalidation);
}
