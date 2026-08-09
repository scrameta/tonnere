/* test_usb_keyboard.c — portable HID->Atari keyboard mapping tests.
 * Runs on host against the fake FPGA backend: proves USB key events land in
 * the right matrix bits without any board/USBX. */
#include "test_harness.h"
#include "usb_keyboard.h"
#include "fpga_bus.h"
#include "fpga_bus_map.h"

uint16_t fake_fpga_get_reg(enum fpga_reg_index idx);

/* matrix bit N -> which KBD register (0..3) and bit within it */
static int matrix_bit_set(int kbcode) {
    int reg = REG_KBD0 + (kbcode / 16);
    uint16_t v = fake_fpga_get_reg((enum fpga_reg_index)reg);
    return (v >> (kbcode % 16)) & 1;
}

static void test_kbd_b_and_m(void) {
    fpga_bus_init();
    usb_keyboard_reset();

    /* B = HID 0x05 -> KBCODE 0x15 = bit 21 (kbd1 bit 5) */
    CHECK_EQ_U32(usb_keyboard_hid_to_kbcode(0x05), 0x15);
    usb_keyboard_key_event(0x05, 1);
    CHECK(matrix_bit_set(0x15));
    usb_keyboard_key_event(0x05, 0);
    CHECK(!matrix_bit_set(0x15));

    /* M = HID 0x10 -> KBCODE 0x25 = bit 37 (kbd2 bit 5) */
    CHECK_EQ_U32(usb_keyboard_hid_to_kbcode(0x10), 0x25);
    usb_keyboard_key_event(0x10, 1);
    CHECK(matrix_bit_set(0x25));

    /* Space = HID 0x2c -> KBCODE 0x21 = bit 33 (was mis-mapped before) */
    CHECK_EQ_U32(usb_keyboard_hid_to_kbcode(0x2c), 0x21);
    /* '[' = HID 0x2f -> KBCODE 0x0E (guess-table had this wrong) */
    CHECK_EQ_U32(usb_keyboard_hid_to_kbcode(0x2f), 0x0E);
    /* '-' = HID 0x2d -> KBCODE 0x36 (guess-table had this wrong) */
    CHECK_EQ_U32(usb_keyboard_hid_to_kbcode(0x2d), 0x36);

    /* numeric keypad maps to the same KBCODEs as the main keys */
    CHECK_EQ_U32(usb_keyboard_hid_to_kbcode(0x5F), 0x33); /* KP7 -> '7' */
    CHECK_EQ_U32(usb_keyboard_hid_to_kbcode(0x62), 0x32); /* KP0 -> '0' */
    CHECK_EQ_U32(usb_keyboard_hid_to_kbcode(0x58), 0x0C); /* KP Enter -> Return */
}

static void test_kbd_l_gap_disambiguation(void) {
    /* L = HID 0x0F -> KBCODE 0x00 (the one legit 0x00 mapping) */
    CHECK_EQ_U32(usb_keyboard_hid_to_kbcode(0x0F), 0x00);
    /* an unmapped usage must NOT read back as 0x00/L */
    CHECK_EQ_U32(usb_keyboard_hid_to_kbcode(0x65), 0xFF);
}

static void test_kbd_modifiers_and_break(void) {
    fpga_bus_init();
    usb_keyboard_reset();
    /* modifiers are not matrix bits; they go to KBD_SPECIAL */
    usb_keyboard_key_event(0xE1, 1);   /* LShift */
    uint16_t sp = fake_fpga_get_reg(REG_KBD_SPECIAL);
    CHECK(sp & (1u << KBD_SPECIAL_SHIFT_BIT));
    usb_keyboard_key_event(0x48, 1);   /* Break */
    sp = fake_fpga_get_reg(REG_KBD_SPECIAL);
    CHECK(sp & (1u << KBD_SPECIAL_BREAK_BIT));
    /* neither set a matrix bit */
    usb_keyboard_key_event(0xE1, 0);
    usb_keyboard_key_event(0x48, 0);
    sp = fake_fpga_get_reg(REG_KBD_SPECIAL);
    CHECK(!(sp & (1u << KBD_SPECIAL_SHIFT_BIT)));
    CHECK(!(sp & (1u << KBD_SPECIAL_BREAK_BIT)));
}

static void test_kbd_report_mode(void) {
    fpga_bus_init();
    usb_keyboard_reset();
    /* full report: B + M held, shift down */
    uint8_t keys[6] = { 0x05, 0x10, 0, 0, 0, 0 };
    usb_keyboard_report(0x02 /* LShift */, keys);
    CHECK(matrix_bit_set(0x15));   /* B */
    CHECK(matrix_bit_set(0x25));   /* M */
    uint16_t sp = fake_fpga_get_reg(REG_KBD_SPECIAL);
    CHECK(sp & (1u << KBD_SPECIAL_SHIFT_BIT));
    /* empty report releases everything */
    uint8_t none[6] = { 0, 0, 0, 0, 0, 0 };
    usb_keyboard_report(0, none);
    CHECK(!matrix_bit_set(0x15));
    CHECK(!matrix_bit_set(0x25));
}

void run_usb_keyboard_tests(void) {
    RUN(test_kbd_b_and_m);
    RUN(test_kbd_l_gap_disambiguation);
    RUN(test_kbd_modifiers_and_break);
    RUN(test_kbd_report_mode);
}
