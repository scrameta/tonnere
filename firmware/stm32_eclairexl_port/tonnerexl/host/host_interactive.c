/*
 * host_interactive.c — interactive host runner for the TonnereXL port.
 *
 * Unlike test_main.c (which runs unit suites and exits), this boots the real
 * app_main() and gives you a REPL to DRIVE it: keypresses are injected into the
 * same input queue the USB thread feeds, so they flow through the genuine port
 * path (usbin_service_step -> fpga_kbd_set -> fake backend). A display command
 * shows current port/FPGA state.
 *
 * Runs on the host only (fake fpga backend, FileX RAM disk today). Two seams are
 * marked TODO for the things Mark flagged as coming next:
 *   (i)  SD-card image  -> mount a file-backed FileX media instead of RAM disk
 *   (ii) menu display   -> render the Atari memory window instead of state dump
 */
#include "tx_api.h"
#include "fx_api.h"
#include "app_main.h"
#include "app_threads.h"
#include "fpga_bus.h"
#include "platform.h"
#include "simplefile_filex.h"
#include "simplefile.h"
#include "simpledir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

/* ---- fake-backend introspection (host build) ---- */
uint16_t fake_fpga_get_reg(enum fpga_reg_index idx);
const uint8_t* fake_fpga_atari_ptr(void);
void fake_fpga_set_console_phys(uint16_t bits);
void fake_fpga_raise_irq(uint16_t bits);

/* ---- app pool ---- */
static TX_BYTE_POOL app_pool;
static UCHAR        app_pool_mem[64*1024];

/* ---- SD-card image, seam (i): a file-backed FileX RAM media ---- */
VOID _fx_ram_driver(FX_MEDIA *media_ptr);
static FX_MEDIA  s_sd_media;
static UCHAR     s_sd_media_mem[512];
static UCHAR    *s_sd_image;          /* the "card" contents in RAM        */
static long      s_sd_image_bytes;
static char      s_sd_image_path[256];
static int       s_sd_mounted;

/* Mount a disk-image file as the SD card: load the file into a RAM buffer,
 * run FileX on it, and bind it to the SimpleFile layer so the port's menu /
 * drive code sees a real filesystem. If the file doesn't exist, format a fresh
 * blank image of default size. */
static int sd_mount(const char *path) {
    if (s_sd_mounted) { printf("  already mounted; unmount first\n"); return -1; }

    long want = 4*1024*1024;   /* default 4 MB blank card */
    FILE *f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END); want = ftell(f); fseek(f, 0, SEEK_SET);
    }
    if (want < 64*1024) want = 64*1024;
    s_sd_image = malloc((size_t)want);
    if (!s_sd_image) { if (f) fclose(f); printf("  OOM\n"); return -1; }
    memset(s_sd_image, 0, (size_t)want);
    s_sd_image_bytes = want;

    int fresh = 1;
    if (f) {
        size_t got = fread(s_sd_image, 1, (size_t)want, f);
        fclose(f);
        fresh = (got == 0);
        printf("  loaded %zu bytes from %s\n", got, path);
    } else {
        printf("  %s not found; formatting blank %ld MB image\n", path, want/(1024*1024));
    }

    static int fx_inited = 0;
    if (!fx_inited) { fx_system_initialize(); fx_inited = 1; }

    if (fresh) {
        UINT s = fx_media_format(&s_sd_media, _fx_ram_driver, s_sd_image,
                                 s_sd_media_mem, sizeof s_sd_media_mem,
                                 "SDCARD", 2, 512, 0, (ULONG)(want/512), 512, 1, 1, 1);
        if (s != FX_SUCCESS) { printf("  format failed: 0x%02X\n", s); free(s_sd_image); return -1; }
    }
    UINT s = fx_media_open(&s_sd_media, "sd", _fx_ram_driver, s_sd_image,
                           s_sd_media_mem, sizeof s_sd_media_mem);
    if (s != FX_SUCCESS) { printf("  open failed: 0x%02X\n", s); free(s_sd_image); return -1; }

    simplefile_bind_media(&s_sd_media);
    strncpy(s_sd_image_path, path, sizeof s_sd_image_path - 1);
    s_sd_mounted = 1;
    printf("  mounted %s as SD card\n", path);
    return 0;
}

/* Unmount: flush FileX, unbind, write the RAM image back to the file so changes
 * persist across runs. */
static void sd_unmount(void) {
    if (!s_sd_mounted) { printf("  no SD image mounted\n"); return; }
    simplefile_unbind_media();
    fx_media_close(&s_sd_media);
    FILE *f = fopen(s_sd_image_path, "wb");
    if (f) { fwrite(s_sd_image, 1, (size_t)s_sd_image_bytes, f); fclose(f);
             printf("  wrote image back to %s\n", s_sd_image_path); }
    free(s_sd_image); s_sd_image = NULL;
    s_sd_mounted = 0;
}

/* ---- REPL state ---- */
static volatile int g_quit = 0;

/*
 * Map a typed ASCII character to an Atari KBCODE bit (0..63). This is a
 * PLACEHOLDER map good enough to drive the port interactively — the real
 * USB-HID -> KBCODE table (Phase 2) will replace it. Returns -1 if unmapped.
 * Only a handful are filled in; extend as needed for interactive testing.
 */
static int ascii_to_kbcode(int c) {
    /* TODO(port): replace with the real 800XL KBCODE table. These are
     * illustrative slots, not the true matrix positions. */
    if (c >= 'a' && c <= 'z') return (c - 'a');        /* 0..25  */
    if (c >= '0' && c <= '9') return 26 + (c - '0');   /* 26..35 */
    switch (c) {
        case ' ':  return 36;  /* space  */
        case '\n': return 37;  /* return */
        default:   return -1;
    }
}

/* Inject a keypress+release into the input queue, as the USB thread would. */
static void inject_key(int kbcode) {
    ULONG down = (1u << 31) | (uint32_t)(kbcode & 0x3f);
    ULONG up   = (uint32_t)(kbcode & 0x3f);
    tx_queue_send(&g_input_queue, &down, TX_NO_WAIT);
    tx_thread_sleep(1);
    tx_queue_send(&g_input_queue, &up, TX_NO_WAIT);
}

/* ---- display: dump current port/FPGA state (menu-display seam) ---- */
static void show_state(void) {
    printf("\n---- TonnereXL state ----\n");
    printf("CONTROL      = 0x%04X\n", fake_fpga_get_reg(REG_CONTROL));
    printf("VIDEO        = 0x%04X\n", fake_fpga_get_reg(REG_VIDEO));
    printf("KBD matrix   = %04X %04X %04X %04X\n",
           fake_fpga_get_reg(REG_KBD0), fake_fpga_get_reg(REG_KBD1),
           fake_fpga_get_reg(REG_KBD2), fake_fpga_get_reg(REG_KBD3));
    printf("CONSOLE inj  = 0x%04X   phys = 0x%04X\n",
           fake_fpga_get_reg(REG_CONSOLE_INJECT), fpga_console_phys_read());
    printf("IRQ enable   = 0x%04X   pending = 0x%04X\n",
           fpga_irq_enabled(), fpga_irq_pending());
    /*
     * TODO(port, menu display): once the menu renders into the Atari memory
     * window, dump a text view of it here instead of (or beside) the register
     * state. The window is at fake_fpga_atari_ptr(); a menu screen would be an
     * 40x24 region we can print as ASCII.
     */
    printf("-------------------------\n\n");
}

/* List a directory through the SimpleDir layer — the same API the menu's file
 * selector uses, so this exercises the real port path over the mounted image. */
static void cmd_ls(const char *path) {
    static char arena[8192];
    if (dir_init(arena, sizeof arena) != SimpleFile_OK) { printf("  dir_init failed\n"); return; }
    struct SimpleDirEntry *e = dir_entries(path);
    if (!e) { printf("  (empty or not mounted)\n"); return; }
    printf("  %s:\n", path);
    for (; e; e = dir_next(e)) {
        printf("    %-20s %s %d\n", dir_filename(e),
               dir_is_subdir(e) ? "<dir>" : "     ", dir_filesize(e));
    }
}

static void show_help(void) {
    printf(
      "\nCommands:\n"
      "  k <text>   inject keypresses (each char -> KBCODE, press+release)\n"
      "  s <b>      inject console key: start|select|option|reset (physical)\n"
      "  i          show state (registers, matrix, console, irq)\n"
      "  m <path>   mount a disk-image file as the SD card (blank if missing)\n"
      "  u          unmount SD image (writes changes back to the file)\n"
      "  l [path]   list a directory on the mounted card (default /)\n"
      "  t <name>   create an empty file on the card (to test the selector)\n"
      "  d <name>   create a directory on the card\n"
      "  h          help\n"
      "  q          quit\n\n");
}

/* ---- the REPL runs in its own ThreadX thread so the port threads schedule ---- */
static TX_THREAD repl_thread;
static UCHAR     repl_stack[32*1024];

static void handle_line(char *line) {
    /* strip newline */
    size_t n = strlen(line);
    while (n && (line[n-1]=='\n' || line[n-1]=='\r')) line[--n] = 0;
    if (n == 0) return;

    char cmd = line[0];
    char *arg = line + 1;
    while (*arg == ' ') arg++;

    switch (cmd) {
    case 'k':
        for (char *p = arg; *p; p++) {
            int kb = ascii_to_kbcode(tolower((unsigned char)*p));
            if (kb >= 0) { inject_key(kb); printf("  key '%c' -> kbcode %d\n", *p, kb); }
            else printf("  key '%c' unmapped\n", *p);
        }
        break;
    case 's': {
        uint16_t bit = 0;
        if      (!strncmp(arg,"start",5))  bit = 1u<<CONSOLE_START_BIT;
        else if (!strncmp(arg,"select",6)) bit = 1u<<CONSOLE_SELECT_BIT;
        else if (!strncmp(arg,"option",6)) bit = 1u<<CONSOLE_OPTION_BIT;
        else if (!strncmp(arg,"reset",5))  bit = 1u<<CONSOLE_RESET_BIT;
        else { printf("  unknown console key\n"); break; }
        /* simulate a physical press the menu can observe, then release */
        fake_fpga_set_console_phys(bit);
        printf("  console phys asserted 0x%04X (press enter to release)\n", bit);
        break;
    }
    case 'i': show_state(); break;
    case 'm':
        if (*arg) sd_mount(arg);
        else printf("  usage: m <path-to-disk-image>  (created blank if missing)\n");
        break;
    case 'u': sd_unmount(); break;
    case 'l': cmd_ls(*arg ? arg : "/"); break;
    case 't':   /* touch: create an empty file on the mounted card */
        if (!s_sd_mounted) { printf("  mount an image first\n"); break; }
        else {
            char fx[256]; size_t j=0;
            for (const char *p=arg; *p && j<sizeof fx-1; p++) fx[j++] = (*p=='/')?'\\':*p;
            fx[j]=0;
            UINT s = fx_file_create(&s_sd_media, fx);
            printf("  create %s: 0x%02X\n", arg, s);
            fx_media_flush(&s_sd_media);
        }
        break;
    case 'd':   /* mkdir on the mounted card */
        if (!s_sd_mounted) { printf("  mount an image first\n"); break; }
        else {
            char fx[256]; size_t j=0;
            for (const char *p=arg; *p && j<sizeof fx-1; p++) fx[j++] = (*p=='/')?'\\':*p;
            fx[j]=0;
            UINT s = fx_directory_create(&s_sd_media, fx);
            printf("  mkdir %s: 0x%02X\n", arg, s);
            fx_media_flush(&s_sd_media);
        }
        break;
    case 'h': show_help(); break;
    case 'q': g_quit = 1; break;
    default:  printf("  ? '%c' - type h for help\n", cmd); break;
    }
}


/* Read a line from stdin, retrying on EINTR. The ThreadX Linux port drives its
 * tick from a POSIX interval timer (SIGALRM), which interrupts blocking reads;
 * without this retry, fgets returns NULL on the first tick and the REPL exits
 * immediately (that's the "quits straight away" bug). Returns NULL only on real
 * EOF or error. */
static char *repl_getline(char *buf, int cap) {
    for (;;) {
        errno = 0;
        char *r = fgets(buf, cap, stdin);
        if (r) return r;
        if (errno == EINTR) { clearerr(stdin); continue; }  /* signal, retry */
        return NULL;                                        /* real EOF/error */
    }
}

static void repl_entry(ULONG arg) {
    (void)arg;
    /* start the real port */
    app_config_t cfg = { .thread_pool = &app_pool };
    if (app_main(&cfg) != TX_SUCCESS) {
        printf("app_main failed\n");
        exit(1);
    }
    tx_thread_sleep(4);   /* let threads reach their first wait */

    printf("\n=== TonnereXL interactive host ===\n");
    show_help();

    char line[256];
    while (!g_quit) {
        printf("txl> "); fflush(stdout);
        if (!repl_getline(line, sizeof line)) break;   /* real EOF only */
        handle_line(line);
    }
    printf("bye\n");
    exit(0);
}

void tx_application_define(void *first_unused) {
    (void)first_unused;
    tx_byte_pool_create(&app_pool, "app", app_pool_mem, sizeof app_pool_mem);
    tx_thread_create(&repl_thread, "repl", repl_entry, 0,
                     repl_stack, sizeof repl_stack,
                     16, 16, TX_NO_TIME_SLICE, TX_AUTO_START);
}

int main(void) { tx_kernel_enter(); return 0; }
