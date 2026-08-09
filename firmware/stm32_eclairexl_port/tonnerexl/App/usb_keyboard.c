/*
 * usb_keyboard.c — portable USB-HID -> Atari keyboard mapping.
 * See usb_keyboard.h. Depends only on the fpga_bus HAL.
 */
#include "usb_keyboard.h"
#include "fpga_bus.h"

#include "logger.h"

#define NOKEY 0xFF
#define HID_BREAK 0x48   /* Pause/Break -> Atari Break (spatial binding) */

/* HID usage (Usage Page 0x07) -> Atari KBCODE (== matrix bit). NOKEY = unmapped.
 * Alphanumerics are the standard KBCODE values (B/M verified against the walk).
 * Punctuation entries reflect the chosen spatial binding — confirm/adjust
 * against the bit-walk; whatever bit lights a key IS its KBCODE. */
static const uint8_t hid_to_kbcode[256] = {
    /* Composed from the reference firmware's USB->PS/2 (usb2ps2[]) and the
     * PS/2->Atari mapping in ps2_to_atari800.vhdl (US-layout branch), so the
     * behaviour matches the old board exactly. matrix bit == KBCODE. */
    [0x04]=0x3F, /* A */
    [0x05]=0x15, /* B */
    [0x06]=0x12, /* C */
    [0x07]=0x3A, /* D */
    [0x08]=0x2A, /* E */
    [0x09]=0x38, /* F */
    [0x0A]=0x3D, /* G */
    [0x0B]=0x39, /* H */
    [0x0C]=0x0D, /* I */
    [0x0D]=0x01, /* J */
    [0x0E]=0x05, /* K */
    [0x0F]=0x00, /* L */
    [0x10]=0x25, /* M */
    [0x11]=0x23, /* N */
    [0x12]=0x08, /* O */
    [0x13]=0x0A, /* P */
    [0x14]=0x2F, /* Q */
    [0x15]=0x28, /* R */
    [0x16]=0x3E, /* S */
    [0x17]=0x2D, /* T */
    [0x18]=0x0B, /* U */
    [0x19]=0x10, /* V */
    [0x1A]=0x2E, /* W */
    [0x1B]=0x16, /* X */
    [0x1C]=0x2B, /* Y */
    [0x1D]=0x17, /* Z */
    [0x1E]=0x1F, /* 1 */
    [0x1F]=0x1E, /* 2 */
    [0x20]=0x1A, /* 3 */
    [0x21]=0x18, /* 4 */
    [0x22]=0x1D, /* 5 */
    [0x23]=0x1B, /* 6 */
    [0x24]=0x33, /* 7 */
    [0x25]=0x35, /* 8 */
    [0x26]=0x30, /* 9 */
    [0x27]=0x32, /* 0 */
    [0x28]=0x0C, /* Return */
    [0x29]=0x1C, /* Esc */
    [0x2A]=0x34, /* Backspace */
    [0x2B]=0x2C, /* Tab */
    [0x2C]=0x21, /* Space */
    [0x2D]=0x36, /* - */
    [0x2E]=0x37, /* = */
    [0x2F]=0x0E, /* [ */
    [0x30]=0x0F, /* ] */
    [0x31]=0x02, /* backslash */
    [0x32]=0x02, /* Europe1 */
    [0x33]=0x06, /* ; */
    [0x34]=0x07, /* ' */
    [0x36]=0x20, /* , */
    [0x37]=0x22, /* . */
    [0x38]=0x26, /* / */
    [0x39]=0x3C, /* CapsLock */
    [0x3A]=0x03, /* F1 */
    [0x3B]=0x04, /* F2 */
    [0x3C]=0x13, /* F3 */
    [0x3D]=0x14, /* F4 */
    [0x3E]=0x11, /* F5 = Help */
    /* Numeric keypad (Atari has none) -> same Atari KBCODE as the main-key
     * equivalent, so the keypad types the same digits/symbols. */
    [0x54]=0x26, /* KP / */
    [0x55]=0x07, /* KP * */
    [0x56]=0x36, /* KP - */
    [0x57]=0x06, /* KP + */
    [0x58]=0x0C, /* KP Enter */
    [0x59]=0x1F, /* KP 1 */
    [0x5A]=0x1E, /* KP 2 */
    [0x5B]=0x1A, /* KP 3 */
    [0x5C]=0x18, /* KP 4 */
    [0x5D]=0x1D, /* KP 5 */
    [0x5E]=0x1B, /* KP 6 */
    [0x5F]=0x33, /* KP 7 */
    [0x60]=0x35, /* KP 8 */
    [0x61]=0x30, /* KP 9 */
    [0x62]=0x32, /* KP 0 */
    [0x63]=0x22, /* KP . */
};

uint8_t usb_keyboard_hid_to_kbcode(uint8_t hid_usage)
{
    uint8_t code = hid_to_kbcode[hid_usage];
    /* table gaps default to 0x00, which is a valid KBCODE (L). Guard: only
     * usages we explicitly mapped are non-gap. Treat usage 0 and modifiers as
     * unmapped. */
    if (hid_usage == 0) return NOKEY;
    if (hid_usage >= 0xE0 && hid_usage <= 0xE7) return NOKEY;  /* modifiers */
    if (hid_usage == HID_BREAK) return NOKEY;                  /* special   */
    /* A real gap reads 0x00 but so does 'L' (usage 0x0F). Disambiguate: 0x0F is
     * the only usage that legitimately maps to 0x00. */
    if (code == 0x00 && hid_usage != 0x0F) return NOKEY;
    return code;
}

/* modifier/break state, tracked for fpga_kbd_special */
static int s_shift, s_ctrl, s_break, s_consol;

void usb_keyboard_reset(void)
{
    s_shift = s_ctrl = s_break = 0;
    s_consol = 0x7;
    fpga_kbd_matrix_write(0, 0, 0, 0);
    fpga_kbd_special(0, 0, 0);
}

void usb_keyboard_key_event(uint8_t hid_usage, int pressed)
{
    log_printf("USB: hid_usage=%d pressed=%d\r\n",hid_usage, pressed);

    uint8_t code = 255;

    /* modifiers + break are the KBD_SPECIAL side channel, not matrix bits */
    switch (hid_usage) {
        case 0xE1: case 0xE5:   /* L/R Shift */
            s_shift = pressed; fpga_kbd_special(s_shift, s_ctrl, s_break); return;
        case 0xE0: case 0xE4:   /* L/R Ctrl  */
            s_ctrl  = pressed; fpga_kbd_special(s_shift, s_ctrl, s_break); return;
        case 0xE6: 
            code=0x27; /* inverse video */
            break;
        case HID_BREAK:
            s_break = pressed; fpga_kbd_special(s_shift, s_ctrl, s_break); return;
        case 63:
            if (pressed)
                s_consol = s_consol&0x6;
            else
                s_consol = s_consol|0x1;
            fpga_console_inject(s_consol);
            return;
        case 64:
            if (pressed)
                s_consol = s_consol&0x5;
            else
                s_consol = s_consol|0x2;
            fpga_console_inject(s_consol);
            return;
        case 65:
            if (pressed)
                s_consol = s_consol&0x3;
            else
                s_consol = s_consol|0x4;
            fpga_console_inject(s_consol);
            return;
        case 66:
            //fpga_console_inject(s_consol);
            fpga_core_set_reset(pressed);
            return;
        default: 
            code = usb_keyboard_hid_to_kbcode(hid_usage);
            break;
    }

//USB: hid_usage=63 pressed=1 F6=START
//USB: hid_usage=64 pressed=1 F7=SELECT
//USB: hid_usage=65 pressed=1 F8=OPTION
//USB: hid_usage=66 pressed=1 F9=RESET
//CONSOL_IN <= '1'&CONSOL_OPTION&CONSOL_SELECT&CONSOL_START;

    log_printf("USB: code:%d shift:%d ctrl:%d break:%d\r\n",code,s_shift,s_ctrl,s_break);
    if (code == NOKEY) return;

    fpga_kbd_set(code, pressed);   /* update matrix shadow */
    fpga_kbd_flush();              /* push KBD0..3 */
}

void usb_keyboard_report(uint8_t mod, const uint8_t keys[6])
{
    /* Rebuild the matrix from the report: clear, set each held key. Uses the
     * matrix_write path directly since it's a full-state replace. */
    uint64_t matrix = 0;
    int brk = 0;
    for (int i = 0; i < 6; i++) {
        uint8_t usage = keys[i];
        log_printf("USB: key[%d]=%d\r\n",i,keys[i]);
        if (usage == 0) continue;
        if (usage == HID_BREAK) { brk = 1; continue; }
        uint8_t code = usb_keyboard_hid_to_kbcode(usage);
        if (code == NOKEY || code >= 64) continue;
        matrix |= ((uint64_t)1u << code);
    }
    log_printf("USB: mod=%d\r\n",mod);
    log_printf("USB: matrix=%x.%x.%x.%x\r\n",(uint16_t)(matrix        & 0xFFFF),(uint16_t)((matrix >> 16) & 0xFFFF),(uint16_t)((matrix >> 32) & 0xFFFF),(uint16_t)((matrix >> 48) & 0xFFFF));
    fpga_kbd_matrix_write((uint16_t)(matrix        & 0xFFFF),
                          (uint16_t)((matrix >> 16) & 0xFFFF),
                          (uint16_t)((matrix >> 32) & 0xFFFF),
                          (uint16_t)((matrix >> 48) & 0xFFFF));
    s_shift = (mod & 0x22) != 0;
    s_ctrl  = (mod & 0x11) != 0;
    s_break = brk;
    log_printf("USB: shift=%x ctrl=%x break=%x\r\n",s_shift, s_ctrl, s_break);
    fpga_kbd_special(s_shift, s_ctrl, s_break);
}
