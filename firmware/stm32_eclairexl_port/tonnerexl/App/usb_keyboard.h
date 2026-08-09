/*
 * usb_keyboard.h — portable USB-HID -> Atari keyboard mapping.
 *
 * PORTABLE (host + STM32). Depends only on the fpga_bus HAL (fpga_kbd_set /
 * fpga_kbd_flush / fpga_kbd_special), never on USBX. The board-side USBX thread
 * and the host REPL both feed raw HID usage codes in here; the translation to
 * Atari KBCODE (== matrix bit) and the shift/ctrl/break handling live here so
 * they are exercised by the host tests.
 *
 * Mapping fact (from the RTL): matrix bit index == Atari KBCODE (low 6 bits).
 * Verified: B = KBCODE 0x15 = bit 21, M = 0x25 = bit 37.
 *
 * Spatial mapping: keys are bound by physical position (stickers) so the board
 * behaves like a real Atari keyboard; the table maps HID usage -> the KBCODE of
 * the Atari key at that position.
 */
#ifndef TONNEREXL_USB_KEYBOARD_H
#define TONNEREXL_USB_KEYBOARD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Translate a raw HID usage code to an Atari KBCODE (== matrix bit).
 * Returns 0xFF if the usage is unmapped or is a modifier (handled separately). */
uint8_t usb_keyboard_hid_to_kbcode(uint8_t hid_usage);

/* Single key change: raw HID usage code + pressed(1)/released(0). Updates the
 * matrix shadow (fpga_kbd_set) and flushes, or routes modifiers/break to
 * fpga_kbd_special. This is the seam both USBX and the host REPL call. */
void usb_keyboard_key_event(uint8_t hid_usage, int pressed);

/* Clear all held keys + modifiers (call on keyboard attach/detach). */
void usb_keyboard_reset(void);

/* Full HID boot report: modifier byte + up to 6 usage codes (0 = empty).
 * Rebuilds the whole matrix from the report. Provided for transports that hand
 * over the raw report rather than per-key changes. */
void usb_keyboard_report(uint8_t mod, const uint8_t keys[6]);

#ifdef __cplusplus
}
#endif
#endif /* TONNEREXL_USB_KEYBOARD_H */
