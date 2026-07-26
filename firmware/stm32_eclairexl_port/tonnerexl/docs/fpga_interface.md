# TonnereXL — STM32 ↔ FPGA interface contract

**Status:** DRAFT v0.2 · **Owner:** firmware (STM32 side) · **Consumer:** FSMC adaptor RTL (FPGA side)

This document is the *authoritative contract* between the STM32F407 firmware and
the FPGA FSMC adaptor. The firmware `fpga_bus` HAL implements exactly what is
written here; the RTL Mark writes must match it. Where the RTL cannot match a
field, change *this document first*, bump the version, then change both sides.
Anything marked **TODO(mark)** is a value the RTL/board decides and the firmware
currently assumes.

## What changed from v0.1

v0.1 preserved the ZPU's 32-bit register numbering and split every register into
two 16-bit halves. v0.2 abandons that: the map is now a **native 16-bit,
purpose-named** register set designed for a fresh FSMC adaptor. Consequences:

- No cross-half read/modify/write, no read-latching, no commit-on-high — the
  entire half-split machinery is gone. Every register is a plain 16-bit access.
- No 32-bit register needs atomic access. Values that were >16 bits split into
  independent narrower registers (keyboard matrix = 4 regs, paddles = 2 axes per
  reg) or are read-only identity words.
- Dropped: the FPGA-side µs timers and LFSR (STM32 has `tx_time_get()`, DWT
  CYCCNT, and hardware RNG); the SPI/SD path (SDIO); the PLL words (I2C/si5351);
  keyboard-type and hotkey registers (USB→matrix mapping is STM32-side); the
  SD-status register (SD is entirely STM32-side; a fake-SD-on-FPGA may add its
  own status later).
- Added: a proper interrupt controller (enable / pending / clear).

---

## 1. Physical bus

| Property | Value | Notes |
| --- | --- | --- |
| Interface | FSMC/FMC Bank1, NE1 | Base `0x6000_0000`. Matches Mark's RAM test wiring. |
| Data width | 16-bit | Native; no register exceeds 16 bits. |
| Access type | Async SRAM, NWAIT-capable | Adaptor may assert wait for slow reads. |
| IRQ | one FPGA→STM32 line | EXTI, level-preferred. **TODO(mark): pin.** See §6. |

`FPGA_BUS_BASE` is a single compile-time constant in `Platform/fpga_bus_map.h`.

---

## 2. Address model

Two regions in the FSMC window:

| Region | Firmware constant | Purpose |
| --- | --- | --- |
| Register file | `FPGA_WIN_REGS` | The 16-bit registers of §3–§6. Register *n* at `base + FPGA_WIN_REGS + 2*n`. |
| Atari memory | `FPGA_WIN_ATARI` | 64 KB live Atari address space, indexed directly (ROM/cart/save-state). |
| SIO UART | `FPGA_WIN_UART` | SIO byte-stream FIFOs (§7). |

**TODO(mark):** confirm the FSMC offsets of these three regions. Firmware
defaults are in `fpga_bus_map.h`; matching the RTL is a localised edit there.

The Atari window is reached directly: firmware indexes it as 16-bit cells via
`fpga_atari_read/write`, which bound-check against 64 KB. No address/data port
indirection.

---

## 3. Register map (native 16-bit)

Register indices are logical; their FSMC offset is `FPGA_WIN_REGS + 2*index`.
Access column is from the **firmware's** point of view (W = firmware writes,
R = firmware reads). Most registers are firmware→FPGA outputs.

### Identity (read-only)

| Reg | Acc | Meaning |
| --- | --- | --- |
| `IFACE_MAGIC` | R | Interface magic. Firmware checks at boot. **TODO(mark): value** (strawman `0x584C`). |
| `IFACE_VERSION` | R | Interface version (strawman `0x0001`). Mismatch → firmware safe-mode. |

### Machine control (firmware writes)

| Reg | Acc | Bits |
| --- | --- | --- |
| `CONTROL` | W | pause core@0, warm-reset@1, cold-reset-strobe@2, freezer-enable@3, atari800-mode@4 |
| `RAMCONFIG` | W | RAM configuration selection (bits 2:0, room to grow) |
| `PERFORMANCE` | W | 6502 speed/turbo select (bits 5:0), VBL-restrict-turbo@8 |
| `TURBO_DRIVE` | W | turbo-drive selection — owned by the SIO handler, kept separate |
| `CART` | W | cartridge type/selection (bits 5:0, room to grow) |
| `VIDEO` | W | video mode (bits 2:0), PAL/NTSC@4, scanlines@5, composite-sync@6 |

### Keyboard (firmware writes, no readback)

| Reg | Acc | Meaning |
| --- | --- | --- |
| `KBD0` | W | Atari 800XL key matrix bits 15:0 |
| `KBD1` | W | key matrix bits 31:16 |
| `KBD2` | W | key matrix bits 47:32 |
| `KBD3` | W | key matrix bits 63:48 |
| `CONSOLE_INJECT` | W | inject console keys: Start@0, Select@1, Option@2, Reset@3 |
| `CONSOLE_PHYS` | R | physical console/Reset switch state (same bit layout) — see §5 |

The 64 matrix bits are laid out in KBCODE scan order (bit *n* = KBCODE *n*),
**including Break and both Shift keys** at their real matrix positions.
**TODO(mark): confirm the KBCODE→bit assignment against the RTL's matrix scan.**
Firmware uses the standard 800XL matrix table (see `fpga_bus_map.h` comment).

### Controllers

| Reg | Acc | Meaning |
| --- | --- | --- |
| `JOY01` | W | digital joystick 0 (bits 4:0) + joystick 1 (bits 12:8); each = 4 dir + 1 trigger |
| `JOY23` | W | digital joystick 2 + joystick 3 (same layout) |
| `PADDLE01` | W | analog axis 0 (bits 7:0) + axis 1 (bits 15:8), from STM32 ADCs |
| `PADDLE23` | W | analog axis 2 + axis 3 |

Digital joystick inputs are physically wired to the FPGA and injected by
firmware here; analog paddle values are read by the STM32's ADCs and written
here. Paddle recharge/charge-cycle timing is handled STM32-side in hardware; the
FPGA needs no POTGO-reset plumbing. See §6 for POTGO IRQ pacing.

### Freezer debug (firmware writes)

| Reg | Acc | Meaning |
| --- | --- | --- |
| `FREEZE_ADDR` | W | 16-bit freezer debug address |
| `FREEZE_DATA_CTRL` | W | data bits 7:0, read-strobe@8, write-strobe@9, data-match-mode@10 |

### Debug scratch (read/write, no hardware meaning)

| Reg | Acc | Meaning |
| --- | --- | --- |
| `DEBUG0`–`DEBUG3` | RW | general-purpose scratch, readable/writable both sides, for bring-up |

---

## 4. (reserved)

*The half-word access rules that occupied this section in v0.1 are deleted — the
map is native 16-bit and needs none of them.*

---

## 5. CONSOLE inject vs physical

The console keys (Start/Select/Option) and the Reset line have **two sources**:
the FPGA's physical switch inputs, and firmware injection (e.g. a menu action or
a USB-key-mapped console press). These are kept as two registers:

- `CONSOLE_INJECT` (W): firmware asserts a console key / Reset by setting its bit.
- `CONSOLE_PHYS` (R): firmware reads the **real physical switch state**, so it can
  react to a hardware Reset or console press (e.g. a physical Reset returning to
  the menu).

The FPGA **ORs** the injected bits with the physical switches before driving GTIA
(console keys) and the system reset line (Reset). This matches the OR-combining
the EclaireXL top level already does for console/trigger sources.

**RTL must:** OR `CONSOLE_INJECT` with the debounced physical switches into GTIA
and the reset line; expose the raw physical switch state at `CONSOLE_PHYS`.

---

## 6. Interrupt controller

A single FPGA→STM32 line, EXTI, level-preferred. **TODO(mark): GPIO port/pin +
EXTI line.** Three registers:

| Reg | Acc | Meaning |
| --- | --- | --- |
| `IRQ_ENABLE` | RW | mask: a source may assert the line only if its bit is set here |
| `IRQ_PENDING` | R | which enabled sources have fired (ISR reads to demux) |
| `IRQ_CLEAR` | W1C | write 1 to a bit to clear that pending source |

Clear semantics are **write-1-to-clear** (robust against read races on a shared
line): the ISR reads `IRQ_PENDING`, handles the set bits, then writes those bits
back to `IRQ_CLEAR`. The line deasserts once no enabled+pending bit remains.

### Sources

| Bit | Source | Consumer |
| --- | --- | --- |
| 0 | SIO command-line asserted (new SIO frame) | Drive/SIO thread |
| 1 | SIO UART RX FIFO non-empty / watermark | Drive/SIO thread |
| 2 | SIO UART TX FIFO empty (frame sent) | Drive/SIO thread |
| 3 | POTGO strobe (Atari started a new POT cycle) | paddle poll |
| 4 | DMA complete (only if adaptor-assisted DMA is ever used) | whoever waits |
| 15:5 | reserved (read 0) | — |

**POTGO** is the paddle pacing source: the FPGA raises this IRQ when the Atari
issues POTGO, and the STM32's paddle poll runs in response — reads its ADCs and
writes `PADDLE01`/`PADDLE23` — so paddle values are refreshed in sync with the
Atari's POT read cycle rather than free-running.

**RTL must:** raise the line while any enabled+pending bit is set (level), and
deassert once firmware has cleared them via `IRQ_CLEAR`.

---

## 7. SIO UART

Unchanged from the ZPU map (already 16-bit native). Byte-stream FIFOs for SIO.

| Reg | Acc | Bits and side effects |
| --- | --- | --- |
| `UART_TX` | W | bits 7:0 enqueue a transmit byte |
| `UART_TX_FIFO` | R | full@9, empty@8, queued-count 7:0 |
| `UART_RX` | R | data 7:0 (+ captured divisor 14:8); reading advances the RX FIFO |
| `UART_RX_FIFO` | R | full@9, empty@8, queued-count 7:0 |
| `UART_DIVISOR` | RW | write pending TX divisor (bits 7:0); read measured RX divisor |
| `UART_FRAMING_ERR` | R | serial framing@0, SIO-command framing@1; reading clears both |

---

## 8. Atari memory window

The 64 KB live Atari address space is a direct FSMC region (`FPGA_WIN_ATARI`),
used for ROM/cartridge load and save-state. Firmware indexes it directly as
16-bit cells via `fpga_atari_read/write`, which bound-check against 64 KB. There
is no DMA descriptor engine and no >16-bit address register.

If bulk moves ever need adaptor assistance (Mode B), that would add a descriptor
and use IRQ bit 4 — not required for bring-up and not currently specified.

---

## 9. Identity / version

`IFACE_MAGIC` / `IFACE_VERSION` are read at startup. Firmware checks the magic
matches and the version is compatible; mismatch → safe-mode (diagnostic menu
only, no drive emulation). Both are read-only, so no atomicity concern.

```
FPGA_IFACE_MAGIC   = 0x584C   /* 'XL' — TODO(mark): finalise */
FPGA_IFACE_VERSION = 0x0001
```

---

## 10. Summary of TODO(mark) decisions the RTL pins down

1. FSMC offsets of the register file, Atari window, and SIO UART region (§2).
2. IRQ GPIO pin + EXTI line (§6).
3. Final `IFACE_MAGIC` value (§9).
4. KBCODE→bit assignment for the 64-key matrix (§3, keyboard).
5. CD/WP GPIO pins are **firmware-side** now (SD is STM32-side); not an RTL item.

All resolved defaults live as named constants in `fpga_bus_map.h`, so matching
the RTL later is a localised edit.
