/*
 * fpga_bus_stm32.c — real FSMC backend for fpga_bus.h (v0.2, on-target only).
 *
 * Native 16-bit accesses to the FSMC window. A priority-inheritance ThreadX
 * mutex serialises read-modify-write sequences. No half composition.
 */
#include "fpga_bus.h"
#include <string.h>

#if defined(FPGA_BUS_STM32)

#include "tx_api.h"

static TX_MUTEX s_bus_mutex;
static int      s_inited;
static uint16_t s_kbd_shadow[4];

static void lock(void)   { if (s_inited) tx_mutex_get(&s_bus_mutex, TX_WAIT_FOREVER); }
static void unlock(void) { if (s_inited) tx_mutex_put(&s_bus_mutex); }

static inline uint16_t rd(enum fpga_reg_index idx) { return *FPGA_REG_ADDR(idx); }
static inline void     wr(enum fpga_reg_index idx, uint16_t v) { *FPGA_REG_ADDR(idx) = v; }
static inline uint16_t urd(enum fpga_sio_index i) { return *FPGA_SIO_ADDR(i); }
static inline void     uwr(enum fpga_sio_index i, uint16_t v) { *FPGA_SIO_ADDR(i) = v; }

fpga_status_t fpga_bus_init(void) {
    if (!s_inited) { tx_mutex_create(&s_bus_mutex, "fpga_bus", TX_INHERIT); s_inited = 1; }
    memset(s_kbd_shadow, 0, sizeof s_kbd_shadow);
    if (fpga_iface_magic() != FPGA_IFACE_MAGIC) return FPGA_ERR_MAGIC;
    return FPGA_OK;
}
uint16_t fpga_iface_magic(void)   { return rd(REG_IFACE_MAGIC); }
uint16_t fpga_iface_version(void) { return rd(REG_IFACE_VERSION); }

uint16_t fpga_reg_read(enum fpga_reg_index idx) { lock(); uint16_t v = rd(idx); unlock(); return v; }
void     fpga_reg_write(enum fpga_reg_index idx, uint16_t value) { lock(); wr(idx, value); unlock(); }
void     fpga_reg_rmw(enum fpga_reg_index idx, uint16_t mask, uint16_t value) {
    lock(); uint16_t cur = rd(idx); wr(idx, (uint16_t)((cur & ~mask) | (value & mask))); unlock();
}

static void bit_set(enum fpga_reg_index idx, int bit, int on) {
    fpga_reg_rmw(idx, (uint16_t)(1u<<bit), on?(uint16_t)(1u<<bit):0);
}
void fpga_core_set_reset(int on)       { bit_set(REG_CONTROL, CTRL_RESET_BIT, on); }
void fpga_core_set_pause(int on)       { bit_set(REG_CONTROL, CTRL_PAUSE_BIT, on); }
void fpga_core_set_freezer(int on)     { bit_set(REG_CONTROL, CTRL_FREEZER_EN_BIT, on); }
void fpga_core_set_atari800(int on)    { bit_set(REG_CONTROL, CTRL_ATARI800_BIT, on); }
void fpga_set_ramconfig(uint16_t sel)  { fpga_reg_write(REG_RAMCONFIG, (uint16_t)(sel & RAMCFG_SEL_MASK)); }
void fpga_set_performance(uint16_t speed, int vbl) {
    uint16_t v = (uint16_t)(speed & PERF_SPEED_MASK);
    if (vbl) v |= (uint16_t)(1u<<PERF_VBL_RESTRICT_BIT);
    fpga_reg_write(REG_PERFORMANCE, v);
}
void fpga_set_cart(uint16_t c)         { fpga_reg_write(REG_CART, (uint16_t)(c & CART_SEL_MASK)); }
void fpga_set_video(uint16_t mode, int pal, int scan, int csync) {
    uint16_t v = (uint16_t)(mode & VIDEO_MODE_MASK);
    if (pal)   v |= (uint16_t)(1u<<VIDEO_PAL_BIT);
    if (scan)  v |= (uint16_t)(1u<<VIDEO_SCANLINES_BIT);
    if (csync) v |= (uint16_t)(1u<<VIDEO_CSYNC_BIT);
    fpga_reg_write(REG_VIDEO, v);
}

void fpga_kbd_matrix_write(uint16_t k0, uint16_t k1, uint16_t k2, uint16_t k3) {
    lock(); wr(REG_KBD0,k0); wr(REG_KBD1,k1); wr(REG_KBD2,k2); wr(REG_KBD3,k3); unlock();
    s_kbd_shadow[0]=k0; s_kbd_shadow[1]=k1; s_kbd_shadow[2]=k2; s_kbd_shadow[3]=k3;
}
void fpga_kbd_set(uint8_t kbcode, int pressed) {
    if (kbcode > 63) return;
    uint16_t *w = &s_kbd_shadow[kbcode >> 4];
    uint16_t m = (uint16_t)(1u << (kbcode & 15));
    if (pressed) *w |= m; else *w &= (uint16_t)~m;
}
void fpga_kbd_clear_all(void) { memset(s_kbd_shadow, 0, sizeof s_kbd_shadow); }
void fpga_kbd_flush(void) {
    fpga_kbd_matrix_write(s_kbd_shadow[0], s_kbd_shadow[1], s_kbd_shadow[2], s_kbd_shadow[3]);
}
void fpga_kbd_special(int shift, int ctrl, int brk) {
    uint16_t v = 0;
    if (shift) v |= (uint16_t)(1u<<KBD_SPECIAL_SHIFT_BIT);
    if (ctrl)  v |= (uint16_t)(1u<<KBD_SPECIAL_CTRL_BIT);
    if (brk)   v |= (uint16_t)(1u<<KBD_SPECIAL_BREAK_BIT);
    fpga_reg_write(REG_KBD_SPECIAL, v);
}

void     fpga_console_inject(uint16_t bits) { fpga_reg_write(REG_CONSOLE_INJECT, bits); }
uint16_t fpga_console_phys_read(void)       { return fpga_reg_read(REG_CONSOLE_PHYS); }

void fpga_joy_write(int pair, uint16_t a, uint16_t b) {
    uint16_t v = (uint16_t)(((a & JOY_FIELD_MASK) << JOY_A_SHIFT) |
                            ((b & JOY_FIELD_MASK) << JOY_B_SHIFT));
    fpga_reg_write(pair ? REG_JOY23 : REG_JOY01, v);
}
uint16_t fpga_joy_phys_read(int pair) {
    return fpga_reg_read(pair ? REG_JOY23_PHYS : REG_JOY01_PHYS);
}
void fpga_paddle_write(int pair, uint8_t a, uint8_t b) {
    uint16_t v = (uint16_t)((uint16_t)a | ((uint16_t)b << PADDLE_B_SHIFT));
    fpga_reg_write(pair ? REG_PADDLE23 : REG_PADDLE01, v);
}

void fpga_freeze_addr(uint16_t addr) { fpga_reg_write(REG_FREEZE_ADDR, addr); }
void fpga_freeze_data_ctrl(uint8_t data, int rd_, int wr_, int match) {
    uint16_t v = data;
    if (rd_)   v |= (uint16_t)(1u<<FREEZE_READ_BIT);
    if (wr_)   v |= (uint16_t)(1u<<FREEZE_WRITE_BIT);
    if (match) v |= (uint16_t)(1u<<FREEZE_MATCH_BIT);
    fpga_reg_write(REG_FREEZE_DATA_CTRL, v);
}

fpga_status_t fpga_atari_write(uint32_t off, const void *src, size_t len) {
    if (off > FPGA_WIN_ATARI_BYTES || len > FPGA_WIN_ATARI_BYTES - off) return FPGA_ERR_RANGE;
    const uint8_t *s = (const uint8_t *)src;
    lock();
    volatile uint16_t *base = FPGA_ATARI_ADDR;
    size_t i = 0;
    for (; i + 1 < len; i += 2)
        base[(off + i) >> 1] = (uint16_t)(s[i] | ((uint16_t)s[i+1] << 8));
    if (i < len) { /* odd trailing byte */
        volatile uint16_t *p = &base[(off + i) >> 1];
        *p = (uint16_t)((*p & 0xff00u) | s[i]);
    }
    unlock();
    return FPGA_OK;
}
fpga_status_t fpga_atari_read(uint32_t off, void *dst, size_t len) {
    if (off > FPGA_WIN_ATARI_BYTES || len > FPGA_WIN_ATARI_BYTES - off) return FPGA_ERR_RANGE;
    uint8_t *d = (uint8_t *)dst;
    lock();
    volatile uint16_t *base = FPGA_ATARI_ADDR;
    size_t i = 0;
    for (; i + 1 < len; i += 2) { uint16_t w = base[(off + i) >> 1]; d[i]=(uint8_t)w; d[i+1]=(uint8_t)(w>>8); }
    if (i < len) { uint16_t w = base[(off + i) >> 1]; d[i]=(uint8_t)w; }
    unlock();
    return FPGA_OK;
}

void     fpga_irq_enable(uint16_t mask) { fpga_reg_write(REG_IRQ_ENABLE, mask); }
uint16_t fpga_irq_enabled(void)         { return fpga_reg_read(REG_IRQ_ENABLE); }
uint16_t fpga_irq_pending(void)         { return fpga_reg_read(REG_IRQ_PENDING); }
void     fpga_irq_clear(uint16_t bits)  { fpga_reg_write(REG_IRQ_CLEAR, bits); } /* W1C */

int fpga_sio_rx_empty(void) { return (urd(SIO_RX_FIFO) & SIO_FIFO_EMPTY) != 0; }
int fpga_sio_tx_full(void)  { return (urd(SIO_TX_FIFO) & SIO_FIFO_FULL) != 0; }
uint16_t fpga_sio_tx_count(void) { return urd(SIO_TX_FIFO) & SIO_FIFO_COUNT_M; }
int fpga_sio_getc(uint8_t *out) {
    if (fpga_sio_rx_empty()) return 0;
    *out = (uint8_t)(urd(SIO_RX) & 0xffu);
    return 1;
}
int fpga_sio_putc(uint8_t c) {
    if (fpga_sio_tx_full()) return 0;
    uwr(SIO_TX, c);
    return 1;
}
void fpga_sio_set_divisor(uint8_t d) { uwr(SIO_DIVISOR, d); }
uint16_t fpga_sio_framing_errors(void) { return urd(SIO_FRAMING_ERR) & 0x3u; }

#endif /* FPGA_BUS_STM32 */

/* ---- RAM apertures ---- */
/* Extension registers are write-only on the FPGA, so shadow them here to answer
 * the get/phys_base queries without reading back bus floats. */
static uint8_t s_aperture_ext[2];   /* [0]=aperture 1, [1]=aperture 2 */
void fpga_aperture_set_ext(int aperture, uint8_t ext) {
    int i = (aperture == 2) ? 1 : 0;
    s_aperture_ext[i] = ext;
    fpga_reg_write(i ? REG_APERTURE2_EXT : REG_APERTURE1_EXT, ext);
}
uint8_t fpga_aperture_get_ext(int aperture) {
    return s_aperture_ext[(aperture == 2) ? 1 : 0];
}
uint32_t fpga_aperture_phys_base(int aperture) {
    return ((uint32_t)fpga_aperture_get_ext(aperture)) << 22;
}
