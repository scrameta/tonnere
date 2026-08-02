/*
 * fpga_bus_fake.c — in-memory model of the FPGA behind the FSMC (v0.2).
 *
 * Linux/host backend AND the oracle for unit tests. Models the v0.2 contract:
 * flat 16-bit registers, W1C interrupt controller, CONSOLE inject/phys split,
 * SIO FIFOs, and the 64 KB Atari window. No half-composition anywhere.
 */
#include "fpga_bus.h"
#include <string.h>

#if defined(FPGA_BUS_FAKE)

typedef struct {
    uint16_t reg[REG_COUNT];
    uint16_t sio[SIO_COUNT];
    uint8_t  atari[FPGA_WIN_ATARI_BYTES];

    /* keyboard shadow (64 bits) */
    uint16_t kbd_shadow[4];

    /* console physical switch state (test-injectable) */
    uint16_t console_phys;

    /* IRQ: pending is separate from the enable mask register */
    uint16_t irq_pending;

    /* SIO FIFOs */
    uint8_t  rx_fifo[256]; uint32_t rx_head, rx_tail;
    uint8_t  tx_fifo[256]; uint32_t tx_head, tx_tail;
    uint16_t framing_errors;
} fake_t;

static fake_t g;

/* ---- lifecycle ---- */
fpga_status_t fpga_bus_init(void) {
    memset(&g, 0, sizeof g);
    g.reg[REG_IFACE_MAGIC]   = FPGA_IFACE_MAGIC;
    g.reg[REG_IFACE_VERSION] = FPGA_IFACE_VERSION;
    if (fpga_iface_magic() != FPGA_IFACE_MAGIC) return FPGA_ERR_MAGIC;
    return FPGA_OK;
}
uint16_t fpga_iface_magic(void)   { return g.reg[REG_IFACE_MAGIC]; }
uint16_t fpga_iface_version(void) { return g.reg[REG_IFACE_VERSION]; }

/* ---- raw register access ---- */
uint16_t fpga_reg_read(enum fpga_reg_index idx) {
    if (idx == REG_CONSOLE_PHYS) return g.console_phys;
    if (idx == REG_IRQ_PENDING)  return (uint16_t)(g.irq_pending & g.reg[REG_IRQ_ENABLE]);
    return g.reg[idx];
}
void fpga_reg_write(enum fpga_reg_index idx, uint16_t value) {
    if (idx == REG_IRQ_CLEAR) { g.irq_pending &= (uint16_t)~value; return; } /* W1C */
    if (idx == REG_CONSOLE_PHYS) return;  /* read-only */
    g.reg[idx] = value;
}
void fpga_reg_rmw(enum fpga_reg_index idx, uint16_t mask, uint16_t value) {
    uint16_t cur = fpga_reg_read(idx);
    fpga_reg_write(idx, (uint16_t)((cur & ~mask) | (value & mask)));
}

/* ---- machine control ---- */
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

/* ---- keyboard ---- */
void fpga_kbd_matrix_write(uint16_t k0, uint16_t k1, uint16_t k2, uint16_t k3) {
    g.reg[REG_KBD0]=k0; g.reg[REG_KBD1]=k1; g.reg[REG_KBD2]=k2; g.reg[REG_KBD3]=k3;
    g.kbd_shadow[0]=k0; g.kbd_shadow[1]=k1; g.kbd_shadow[2]=k2; g.kbd_shadow[3]=k3;
}
void fpga_kbd_set(uint8_t kbcode, int pressed) {
    if (kbcode > 63) return;
    uint16_t *w = &g.kbd_shadow[kbcode >> 4];
    uint16_t m = (uint16_t)(1u << (kbcode & 15));
    if (pressed) *w |= m; else *w &= (uint16_t)~m;
}
void fpga_kbd_clear_all(void) { memset(g.kbd_shadow, 0, sizeof g.kbd_shadow); }
void fpga_kbd_flush(void) {
    fpga_kbd_matrix_write(g.kbd_shadow[0], g.kbd_shadow[1], g.kbd_shadow[2], g.kbd_shadow[3]);
}
void fpga_kbd_special(int shift, int ctrl, int brk) {
    uint16_t v = 0;
    if (shift) v |= (uint16_t)(1u<<KBD_SPECIAL_SHIFT_BIT);
    if (ctrl)  v |= (uint16_t)(1u<<KBD_SPECIAL_CTRL_BIT);
    if (brk)   v |= (uint16_t)(1u<<KBD_SPECIAL_BREAK_BIT);
    fpga_reg_write(REG_KBD_SPECIAL, v);
}

/* ---- console ---- */
void     fpga_console_inject(uint16_t bits) { fpga_reg_write(REG_CONSOLE_INJECT, bits); }
uint16_t fpga_console_phys_read(void)       { return g.console_phys; }

/* ---- joysticks / paddles ---- */
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

/* ---- freezer debug ---- */
void fpga_freeze_addr(uint16_t addr) { fpga_reg_write(REG_FREEZE_ADDR, addr); }
void fpga_freeze_data_ctrl(uint8_t data, int rd, int wr, int match) {
    uint16_t v = data;
    if (rd)    v |= (uint16_t)(1u<<FREEZE_READ_BIT);
    if (wr)    v |= (uint16_t)(1u<<FREEZE_WRITE_BIT);
    if (match) v |= (uint16_t)(1u<<FREEZE_MATCH_BIT);
    fpga_reg_write(REG_FREEZE_DATA_CTRL, v);
}

/* ---- Atari window ---- */
fpga_status_t fpga_atari_write(uint32_t off, const void *src, size_t len) {
    if (off > FPGA_WIN_ATARI_BYTES || len > FPGA_WIN_ATARI_BYTES - off) return FPGA_ERR_RANGE;
    memcpy(&g.atari[off], src, len);
    return FPGA_OK;
}
fpga_status_t fpga_atari_read(uint32_t off, void *dst, size_t len) {
    if (off > FPGA_WIN_ATARI_BYTES || len > FPGA_WIN_ATARI_BYTES - off) return FPGA_ERR_RANGE;
    memcpy(dst, &g.atari[off], len);
    return FPGA_OK;
}

/* ---- interrupt controller ---- */
void     fpga_irq_enable(uint16_t mask) { g.reg[REG_IRQ_ENABLE] = mask; }
uint16_t fpga_irq_enabled(void)         { return g.reg[REG_IRQ_ENABLE]; }
uint16_t fpga_irq_pending(void)         { return (uint16_t)(g.irq_pending & g.reg[REG_IRQ_ENABLE]); }
void     fpga_irq_clear(uint16_t bits)  { g.irq_pending &= (uint16_t)~bits; }

/* ---- SIO UART ---- */
static uint32_t rx_count(void){ return (g.rx_head - g.rx_tail) & 0xffu; }
static uint32_t tx_count(void){ return (g.tx_head - g.tx_tail) & 0xffu; }
int fpga_sio_rx_empty(void){ return rx_count() == 0; }
int fpga_sio_tx_full(void){ return tx_count() >= 255u; }
uint16_t fpga_sio_tx_count(void){ return (uint16_t)tx_count(); }
int fpga_sio_getc(uint8_t *out){
    if (fpga_sio_rx_empty()) return 0;
    *out = g.rx_fifo[g.rx_tail & 0xffu]; g.rx_tail++;
    return 1;
}
int fpga_sio_putc(uint8_t c){
    if (fpga_sio_tx_full()) return 0;
    g.tx_fifo[g.tx_head & 0xffu] = c; g.tx_head++;
    return 1;
}
void fpga_sio_set_divisor(uint8_t d){ g.sio[SIO_DIVISOR] = d; }
uint16_t fpga_sio_framing_errors(void){ uint16_t e=g.framing_errors; g.framing_errors=0; return e; }

/* ---- test-only hooks ---- */
void fake_fpga_inject_rx(const uint8_t *data, size_t n){
    for (size_t i=0;i<n;i++){ g.rx_fifo[g.rx_head & 0xffu]=data[i]; g.rx_head++; }
    g.irq_pending |= (uint16_t)(1u << IRQ_SIO_RX_BIT);
}
size_t fake_fpga_drain_tx(uint8_t *out, size_t max){
    size_t n=0; while(n<max && tx_count()){ out[n++]=g.tx_fifo[g.tx_tail & 0xffu]; g.tx_tail++; }
    return n;
}
void fake_fpga_raise_irq(uint16_t bits){ g.irq_pending |= bits; }
void fake_fpga_set_console_phys(uint16_t bits){ g.console_phys = bits; }
uint16_t fake_fpga_get_reg(enum fpga_reg_index idx){ return g.reg[idx]; }
void fake_fpga_set_reg(enum fpga_reg_index idx, uint16_t v){ g.reg[idx]=v; }
const uint8_t* fake_fpga_atari_ptr(void){ return g.atari; }

#endif /* FPGA_BUS_FAKE */

/* ---- RAM apertures ---- */
void fpga_aperture_set_ext(int aperture, uint8_t ext) {
    fpga_reg_write(aperture == 2 ? REG_APERTURE2_EXT : REG_APERTURE1_EXT, ext);
}
uint8_t fpga_aperture_get_ext(int aperture) {
    return (uint8_t)(fpga_reg_read(aperture == 2 ? REG_APERTURE2_EXT : REG_APERTURE1_EXT) & 0xffu);
}
uint32_t fpga_aperture_phys_base(int aperture) {
    return ((uint32_t)fpga_aperture_get_ext(aperture)) << 22;
}
