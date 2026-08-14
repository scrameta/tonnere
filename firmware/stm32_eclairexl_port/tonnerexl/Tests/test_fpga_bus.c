/*
 * test_fpga_bus.c — exercises the v0.2 fpga_bus fake backend (contract model):
 * flat 16-bit registers, typed machine-control, keyboard matrix + shadow,
 * console inject/phys split, joysticks, paddles, freezer, W1C IRQ controller,
 * bounded Atari copies, and SIO FIFOs.
 */
#include "fpga_bus.h"
#include "test_harness.h"
#include <stdint.h>

/* test-only hooks from the fake */
void   fake_fpga_inject_rx(const uint8_t *data, size_t n);
size_t fake_fpga_drain_tx(uint8_t *out, size_t max);
void   fake_fpga_raise_irq(uint16_t bits);
void   fake_fpga_set_console_phys(uint16_t bits);
uint16_t fake_fpga_get_reg(enum fpga_reg_index idx);
void   fake_fpga_set_reg(enum fpga_reg_index idx, uint16_t v);

static void test_identity(void) {
    CHECK(fpga_bus_init() == FPGA_OK);
    CHECK_EQ_U32(fpga_iface_magic(), FPGA_IFACE_MAGIC);
    CHECK_EQ_U32(fpga_iface_version(), FPGA_IFACE_VERSION);
}

static void test_reg_roundtrip(void) {
    fpga_bus_init();
    fpga_reg_write(REG_DEBUG0, 0x1234);
    CHECK_EQ_U32(fpga_reg_read(REG_DEBUG0), 0x1234);
    fpga_reg_write(REG_DEBUG0, 0xFFFF);
    fpga_reg_rmw(REG_DEBUG0, 0xFF00, 0xAB00);
    CHECK_EQ_U32(fpga_reg_read(REG_DEBUG0), 0xABFF);
}

static void test_machine_control(void) {
    fpga_bus_init();
    fpga_core_set_pause(1);
    CHECK(fake_fpga_get_reg(REG_CONTROL) & (1u<<CTRL_PAUSE_BIT));
    fpga_core_set_atari800(1);
    /* pause survives setting another control bit (independent RMW) */
    CHECK(fake_fpga_get_reg(REG_CONTROL) & (1u<<CTRL_PAUSE_BIT));
    CHECK(fake_fpga_get_reg(REG_CONTROL) & (1u<<CTRL_ATARI800_BIT));
    fpga_core_set_pause(0);
    CHECK(!(fake_fpga_get_reg(REG_CONTROL) & (1u<<CTRL_PAUSE_BIT)));
    CHECK(fake_fpga_get_reg(REG_CONTROL) & (1u<<CTRL_ATARI800_BIT));
    /* cold reset strobe returns to 0 */
    fpga_core_set_reset(1);
    CHECK(fake_fpga_get_reg(REG_CONTROL) & (1u<<CTRL_RESET_BIT));
    fpga_core_set_reset(0);
    CHECK(!(fake_fpga_get_reg(REG_CONTROL) & (1u<<CTRL_RESET_BIT)));
    /* performance packs speed + vbl-restrict */
    fpga_set_performance(0x2A, 1);
    uint16_t p = fake_fpga_get_reg(REG_PERFORMANCE);
    CHECK_EQ_U32(p & PERF_SPEED_MASK, 0x2A);
    CHECK(p & (1u<<PERF_VBL_RESTRICT_BIT));
    /* video packs its fields */
    fpga_set_video(0x5, 1, 0, 1);
    uint16_t v = fake_fpga_get_reg(REG_VIDEO);
    CHECK_EQ_U32(v & VIDEO_MODE_MASK, 0x5);
    CHECK(v & (1u<<VIDEO_PAL_BIT));
    CHECK(!(v & (1u<<VIDEO_SCANLINES_BIT)));
    CHECK(v & (1u<<VIDEO_CSYNC_BIT));
}

static void test_keyboard_matrix(void) {
    fpga_bus_init();
    fpga_kbd_matrix_write(0x0001, 0x0002, 0x0004, 0x8000);
    CHECK_EQ_U32(fake_fpga_get_reg(REG_KBD0), 0x0001);
    CHECK_EQ_U32(fake_fpga_get_reg(REG_KBD3), 0x8000);
    /* shadow set/flush: press kbcode 0, 17 (KBD1 bit1), 63 (KBD3 bit15) */
    fpga_kbd_clear_all();
    fpga_kbd_set(0, 1);
    fpga_kbd_set(17, 1);
    fpga_kbd_set(63, 1);
    fpga_kbd_flush();
    CHECK_EQ_U32(fake_fpga_get_reg(REG_KBD0), 0x0001);
    CHECK_EQ_U32(fake_fpga_get_reg(REG_KBD1), 0x0002);
    CHECK_EQ_U32(fake_fpga_get_reg(REG_KBD3), 0x8000);
    /* release 17 */
    fpga_kbd_set(17, 0);
    fpga_kbd_flush();
    CHECK_EQ_U32(fake_fpga_get_reg(REG_KBD1), 0x0000);
    /* out-of-range kbcode ignored */
    fpga_kbd_set(64, 1);
    fpga_kbd_flush();  /* no crash, no effect beyond existing */
}

static void test_console_inject_phys(void) {
    fpga_bus_init();
    /* inject writes CONSOLE_INJECT */
    fpga_console_inject((1u<<CONSOLE_START_BIT) | (1u<<CONSOLE_OPTION_BIT));
    CHECK_EQ_U32(fake_fpga_get_reg(REG_CONSOLE_INJECT),
                 (1u<<CONSOLE_START_BIT)|(1u<<CONSOLE_OPTION_BIT));
    /* phys is a separate read source */
    fake_fpga_set_console_phys(1u<<CONSOLE_SELECT_BIT);
    CHECK(fpga_console_phys_read() & (1u<<CONSOLE_SELECT_BIT));
    /* inject and phys don't alias */
    CHECK(!(fpga_console_phys_read() & (1u<<CONSOLE_START_BIT)));
}

static void test_joy_paddle(void) {
    fpga_bus_init();
    /* joystick 0 = up+fire, joystick 1 = right */
    fpga_joy_write(0, 0x1 | (1u<<JOY_TRIGGER_BIT), 0x8);
    uint16_t j = fake_fpga_get_reg(REG_JOY01);
    CHECK_EQ_U32((j >> JOY_A_SHIFT) & JOY_FIELD_MASK, 0x1 | (1u<<JOY_TRIGGER_BIT));
    CHECK_EQ_U32((j >> JOY_B_SHIFT) & JOY_FIELD_MASK, 0x8);
    /* joysticks 2/3 in the second register */
    fpga_joy_write(1, 0x2, 0x4);
    uint16_t j2 = fake_fpga_get_reg(REG_JOY23);
    CHECK_EQ_U32((j2 >> JOY_A_SHIFT) & JOY_FIELD_MASK, 0x2);
    CHECK_EQ_U32((j2 >> JOY_B_SHIFT) & JOY_FIELD_MASK, 0x4);
    /* paddles pack two 8-bit axes */
    fpga_paddle_write(0, 0x12, 0x34);
    uint16_t pd = fake_fpga_get_reg(REG_PADDLE01);
    CHECK_EQ_U32(pd & PADDLE_AXIS_MASK, 0x12);
    CHECK_EQ_U32((pd >> PADDLE_B_SHIFT) & PADDLE_AXIS_MASK, 0x34);
    /* joystick phys read is a separate register from inject */
    fake_fpga_set_reg(REG_JOY01_PHYS, 0x0005);
    CHECK_EQ_U32(fpga_joy_phys_read(0), 0x0005);
    CHECK_EQ_U32(fpga_joy_phys_read(1), 0x0000);
    /* inject didn't disturb phys */
    CHECK_EQ_U32(fake_fpga_get_reg(REG_JOY01) & 0xff, j & 0xff);
}

static void test_kbd_special(void) {
    fpga_bus_init();
    fpga_kbd_special(1, 0, 1);   /* shift + break, no ctrl */
    uint16_t v = fake_fpga_get_reg(REG_KBD_SPECIAL);
    CHECK(v & (1u<<KBD_SPECIAL_SHIFT_BIT));
    CHECK(!(v & (1u<<KBD_SPECIAL_CTRL_BIT)));
    CHECK(v & (1u<<KBD_SPECIAL_BREAK_BIT));
}

static void test_ram_aperture(void) {
    fpga_bus_init();
    /* two independent apertures, each an 8-bit extension = phys A29..A22 */
    fpga_aperture_set_ext(1, 0x05);
    fpga_aperture_set_ext(2, 0x42);
    CHECK_EQ_U32(fpga_aperture_get_ext(1), 0x05);
    CHECK_EQ_U32(fpga_aperture_get_ext(2), 0x42);
    CHECK_EQ_U32(fake_fpga_get_reg(REG_APERTURE1_EXT), 0x05);
    CHECK_EQ_U32(fake_fpga_get_reg(REG_APERTURE2_EXT), 0x42);
    /* physical base = ext << 22 (window offset 0) */
    CHECK_EQ_U32(fpga_aperture_phys_base(1), 0x05u << 22);
    CHECK_EQ_U32(fpga_aperture_phys_base(2), 0x42u << 22);
}

static void test_freezer(void) {
    fpga_bus_init();
    fpga_freeze_addr(0xBEEF);
    CHECK_EQ_U32(fake_fpga_get_reg(REG_FREEZE_ADDR), 0xBEEF);
    fpga_freeze_data_ctrl(0x5A, 1, 0, 1);
    uint16_t d = fake_fpga_get_reg(REG_FREEZE_DATA_CTRL);
    CHECK_EQ_U32(d & FREEZE_DATA_MASK, 0x5A);
    CHECK(d & (1u<<FREEZE_READ_BIT));
    CHECK(!(d & (1u<<FREEZE_WRITE_BIT)));
    CHECK(d & (1u<<FREEZE_MATCH_BIT));
}

static void test_irq_controller(void) {
    fpga_bus_init();
    /* enable only SIO cmd + POTGO */
    fpga_irq_enable((1u<<IRQ_SIO_CMD_BIT) | (1u<<IRQ_POTGO_BIT));
    CHECK_EQ_U32(fpga_irq_enabled(), (1u<<IRQ_SIO_CMD_BIT)|(1u<<IRQ_POTGO_BIT));
    /* raise POTGO + UART_RX; only enabled bits are visible in pending */
    fake_fpga_raise_irq((1u<<IRQ_POTGO_BIT) | (1u<<IRQ_SIO_RX_BIT));
    uint16_t p = fpga_irq_pending();
    CHECK(p & (1u<<IRQ_POTGO_BIT));
    CHECK(!(p & (1u<<IRQ_SIO_RX_BIT)));   /* not enabled -> masked out */
    /* W1C: clearing POTGO removes it, leaves others */
    fpga_irq_clear(1u<<IRQ_POTGO_BIT);
    CHECK(!(fpga_irq_pending() & (1u<<IRQ_POTGO_BIT)));
    /* enable UART_RX now; the earlier-raised bit becomes visible */
    fpga_irq_enable((1u<<IRQ_SIO_RX_BIT));
    CHECK(fpga_irq_pending() & (1u<<IRQ_SIO_RX_BIT));
    fpga_irq_clear(1u<<IRQ_SIO_RX_BIT);
    CHECK_EQ_U32(fpga_irq_pending(), 0);
}

static void test_atari_copy_even_odd(void) {
    fpga_bus_init();
    uint8_t src[7] = {1,2,3,4,5,6,7};
    CHECK(fpga_atari_write(0x100, src, 6) == FPGA_OK);
    uint8_t back[7] = {0};
    CHECK(fpga_atari_read(0x100, back, 6) == FPGA_OK);
    CHECK(memcmp(src, back, 6) == 0);
    CHECK(fpga_atari_write(0x201, src, 7) == FPGA_OK);   /* odd offset+len */
    uint8_t back2[7] = {0};
    CHECK(fpga_atari_read(0x201, back2, 7) == FPGA_OK);
    CHECK(memcmp(src, back2, 7) == 0);
    CHECK(fpga_atari_write(FPGA_WIN_ATARI_BYTES - 2, src, 6) == FPGA_ERR_RANGE);
}

static void test_sio_fifo(void) {
    fpga_bus_init();
    CHECK(fpga_sio_rx_empty());
    uint8_t frame[] = {0x31, 0x52, 0x40, 0x40, 0x00};
    fake_fpga_inject_rx(frame, sizeof frame);
    CHECK(!fpga_sio_rx_empty());
    uint8_t got[8]; int n=0; uint8_t tmp;
    while (fpga_sio_getc(&tmp)) got[n++]=tmp;
    CHECK_EQ_U32((uint32_t)n, sizeof frame);
    CHECK(memcmp(got, frame, sizeof frame) == 0);
    for (int b=0;b<4;b++) CHECK(fpga_sio_putc((uint8_t)(0xC0+b)));
    uint8_t txout[8]; size_t m = fake_fpga_drain_tx(txout, sizeof txout);
    CHECK_EQ_U32((uint32_t)m, 4);
    CHECK(txout[0]==0xC0 && txout[3]==0xC3);
}

/* Manual writes to the paddle/audio ADC DMA-target registers (test path).
 * In normal operation DMA owns these; the manual accessors let us verify the
 * FPGA-visible register layout without the ADC running. Also checks the
 * contiguity + base addresses the DMA contract depends on. */
static void test_adc_stream_regs(void) {
    fpga_bus_init();

    /* paddle: 8 channels, each masked to 12 bits, landing in consecutive regs */
    for (unsigned ch = 0; ch < FPGA_PADDLE_ADC_COUNT; ++ch)
        fpga_paddle_adc_write(ch, (uint16_t)(0x0A00 + ch));
    for (unsigned ch = 0; ch < FPGA_PADDLE_ADC_COUNT; ++ch)
        CHECK_EQ_U32(fake_fpga_get_reg((enum fpga_reg_index)(REG_PADDLE_ADC0 + ch)),
                     (uint16_t)(0x0A00 + ch));

    /* audio: 4 channels */
    for (unsigned ch = 0; ch < FPGA_AUDIO_ADC_COUNT; ++ch)
        fpga_audio_adc_write(ch, (uint16_t)(0x0B00 + ch));
    for (unsigned ch = 0; ch < FPGA_AUDIO_ADC_COUNT; ++ch)
        CHECK_EQ_U32(fake_fpga_get_reg((enum fpga_reg_index)(REG_AUDIO_ADC0 + ch)),
                     (uint16_t)(0x0B00 + ch));

    /* bits 15:12 are dropped (FPGA ignores them; accessor masks) */
    fpga_paddle_adc_write(0, 0xF123);
    CHECK_EQ_U32(fake_fpga_get_reg(REG_PADDLE_ADC0), 0x0123);

    /* out-of-range channel is a no-op, not a clobber of an adjacent reg */
    fpga_paddle_adc_write(FPGA_PADDLE_ADC_COUNT, 0x0FFF);   /* ignored */
    CHECK_EQ_U32(fake_fpga_get_reg(REG_AUDIO_ADC0), 0x0B00); /* untouched */

    /* layout invariants the DMA stream relies on */
    CHECK_EQ_U32(REG_PADDLE_ADC0, 31);
    CHECK_EQ_U32(REG_AUDIO_ADC0, 39);
    CHECK_EQ_U32(REG_PADDLE_ADC7 - REG_PADDLE_ADC0, 7);
    CHECK_EQ_U32(REG_AUDIO_ADC3 - REG_AUDIO_ADC0, 3);
}

void run_fpga_bus_tests(void) {
    RUN(test_identity);
    RUN(test_reg_roundtrip);
    RUN(test_machine_control);
    RUN(test_keyboard_matrix);
    RUN(test_console_inject_phys);
    RUN(test_joy_paddle);
    RUN(test_kbd_special);
    RUN(test_ram_aperture);
    RUN(test_freezer);
    RUN(test_irq_controller);
    RUN(test_atari_copy_even_odd);
    RUN(test_sio_fifo);
    RUN(test_adc_stream_regs);
}

#include "platform.h"
static void test_video_modes(void) {
    /* ORIGINAL has no 59.94 variant */
    CHECK(platform_video_supported(STANDARD_ORIGINAL, REFRESH_PAL_50) == 1);
    CHECK(platform_video_supported(STANDARD_ORIGINAL, REFRESH_NTSC_60) == 1);
    CHECK(platform_video_supported(STANDARD_ORIGINAL, REFRESH_NTSC_5994) == 0);
    /* ED and HD720 support all three refreshes */
    CHECK(platform_video_supported(STANDARD_ED, REFRESH_NTSC_5994) == 1);
    CHECK(platform_video_supported(STANDARD_HD720, REFRESH_NTSC_5994) == 1);
    /* setting a valid mode succeeds; invalid is rejected */
    CHECK(platform_set_video(STANDARD_HD720, REFRESH_PAL_50) == 0);
    CHECK(platform_set_video(STANDARD_ORIGINAL, REFRESH_NTSC_5994) == -1);
}

void run_platform_video_tests(void) {
    RUN(test_video_modes);
}
