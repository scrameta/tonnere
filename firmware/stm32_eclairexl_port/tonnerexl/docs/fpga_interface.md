# TonnereXL — STM32 ↔ FPGA interface contract

**Status:** DRAFT v0.3 · **Owner:** firmware (STM32 side) · **Consumer:** FSMC adaptor RTL (FPGA side)

Authoritative contract between the STM32F407 firmware and the FPGA FSMC adaptor.
The firmware `fpga_bus` HAL implements exactly what is written here; the RTL must
match it. To change a field, change *this document first*, bump the version, then
both sides. **TODO(mark)** marks values the RTL/board decides.

## Changes v0.2 → v0.3 (from RTL-implementation review)

- **Joysticks now inject/phys** like console: added `JOY01_PHYS`/`JOY23_PHYS`
  read registers. Physical joystick ports are wired to the FPGA (`inout`); the
  FPGA ORs injected + physical into PORTA/PORTB → GTIA, and firmware can read the
  raw physical state.
- **Resets collapsed.** The only real difference between warm and cold start is
  RAM contents, and CONSOLE-reset duplicated CONTROL-reset. Now a **single reset
  level bit** in CONTROL (firmware toggles low/high). RAM clear is done by
  FSMC→RAM DMA writes, not a reset variant. Dropped: warm/cold distinction,
  cold-reset strobe, CONSOLE reset key.
- **TURBO_DRIVE dropped.** The SIO handler lives *in* the FSMC adaptor RTL and
  negotiates turbo in-protocol; no register needed.
- **Shift/Ctrl/Break** are NOT in the 64-key matrix (they're the KR2 column,
  handled separately in the RTL keyboard-response logic). New `KBD_SPECIAL`
  register holds these 3 bits, kept separate from console keys (console keys and
  physical keyboard keys are distinct on the Atari).
- **SIO region** renamed from "UART" to "SIO handler" — it *is* the in-RTL SIO
  handler (`sio_handler.vhdl`), a 6-register byte-FIFO block. Interface unchanged.

## 1. Physical bus

| Property | Value | Notes |
| --- | --- | --- |
| Interface | FSMC/FMC Bank1, NE1 | Base `0x6000_0000`. |
| Data width | 16-bit | Native; no register exceeds 16 bits. |
| Access type | Async SRAM, NWAIT-capable | |
| IRQ | one FPGA→STM32 line | PG12, EXTI15_10, level-preferred. |

`FPGA_BUS_BASE` is a single compile-time constant in `Platform/fpga_bus_map.h`.

## 2. Address model

Three regions in the FSMC window:

| Region | Constant | Purpose |
| --- | --- | --- |
| Register file | `FPGA_WIN_REGS` (+0x0_0000) | The 16-bit registers of §3–§6. |
| Atari memory | `FPGA_WIN_ATARI` (+0x1_0000) | 64 KB live Atari space, direct-indexed. |
| SIO handler | `FPGA_WIN_SIO` (+0x2_0000) | SIO handler registers (§7). |

**TODO(mark):** confirm the region offsets; firmware defaults in `fpga_bus_map.h`.

### 2.1 Exact addresses (firmware defaults)

Base `0x6000_0000`. Register *n* at byte `0x6000_0000 + 2*n`.

**IMPORTANT — 16-bit FSMC addressing.** On a 16-bit FSMC bus the STM32 does NOT
drive external `A0`; the external address bus carries a 16-bit-word index. So the
byte offsets below map to **external word addresses = offset/2 = register index
n** on the FPGA's address pins. Decode in word units; do not expect A0. Region
select is byte-address bits [17:16]: REGS=`00`, ATARI=`01`, SIO=`10`.

Register file:

| idx | byte off | FSMC byte addr | ext word | acc | register |
| ---:| ---: | --- | ---: | --- | --- |
| 0 | 0x00 | 0x6000_0000 | 0x00 | R   | IFACE_MAGIC |
| 1 | 0x02 | 0x6000_0002 | 0x01 | R   | IFACE_VERSION |
| 2 | 0x04 | 0x6000_0004 | 0x02 | W   | CONTROL |
| 3 | 0x06 | 0x6000_0006 | 0x03 | W   | RAMCONFIG |
| 4 | 0x08 | 0x6000_0008 | 0x04 | W   | PERFORMANCE |
| 5 | 0x0A | 0x6000_000A | 0x05 | W   | CART |
| 6 | 0x0C | 0x6000_000C | 0x06 | W   | VIDEO |
| 7 | 0x0E | 0x6000_000E | 0x07 | W   | KBD0 |
| 8 | 0x10 | 0x6000_0010 | 0x08 | W   | KBD1 |
| 9 | 0x12 | 0x6000_0012 | 0x09 | W   | KBD2 |
| 10 | 0x14 | 0x6000_0014 | 0x0A | W   | KBD3 |
| 11 | 0x16 | 0x6000_0016 | 0x0B | W   | KBD_SPECIAL |
| 12 | 0x18 | 0x6000_0018 | 0x0C | W   | CONSOLE_INJECT |
| 13 | 0x1A | 0x6000_001A | 0x0D | R   | CONSOLE_PHYS |
| 14 | 0x1C | 0x6000_001C | 0x0E | W   | JOY01 |
| 15 | 0x1E | 0x6000_001E | 0x0F | W   | JOY23 |
| 16 | 0x20 | 0x6000_0020 | 0x10 | R   | JOY01_PHYS |
| 17 | 0x22 | 0x6000_0022 | 0x11 | R   | JOY23_PHYS |
| 18 | 0x24 | 0x6000_0024 | 0x12 | W   | PADDLE01 |
| 19 | 0x26 | 0x6000_0026 | 0x13 | W   | PADDLE23 |
| 20 | 0x28 | 0x6000_0028 | 0x14 | W   | FREEZE_ADDR |
| 21 | 0x2A | 0x6000_002A | 0x15 | W   | FREEZE_DATA_CTRL |
| 22 | 0x2C | 0x6000_002C | 0x16 | RW  | IRQ_ENABLE |
| 23 | 0x2E | 0x6000_002E | 0x17 | R   | IRQ_PENDING |
| 24 | 0x30 | 0x6000_0030 | 0x18 | W1C | IRQ_CLEAR |
| 25 | 0x32 | 0x6000_0032 | 0x19 | RW  | DEBUG0 |
| 26 | 0x34 | 0x6000_0034 | 0x1A | RW  | DEBUG1 |
| 27 | 0x36 | 0x6000_0036 | 0x1B | RW  | DEBUG2 |
| 28 | 0x38 | 0x6000_0038 | 0x1C | RW  | DEBUG3 |

SIO handler (byte address = `0x6002_0000 + 2*index`) — matches `sio_handler.vhdl`:

| idx | byte off | FSMC byte addr | ext word | acc | register |
| ---:| ---: | --- | ---: | --- | --- |
| 0 | 0x00 | 0x6002_0000 | 0x00 | W | SIO_TX (transmit byte) |
| 1 | 0x02 | 0x6002_0002 | 0x01 | R | SIO_TX_FIFO (full@9 empty@8 count7:0) |
| 2 | 0x04 | 0x6002_0004 | 0x02 | R | SIO_RX (data14:0; read advances) |
| 3 | 0x06 | 0x6002_0006 | 0x03 | R | SIO_RX_FIFO (full@9 empty@8 count7:0) |
| 4 | 0x08 | 0x6002_0008 | 0x04 | W | SIO_DIVISOR (applied after tx done) |
| 5 | 0x0A | 0x6002_000A | 0x05 | R | SIO_FRAMING_ERR (serial@0 command@1, auto-clear) |

Atari window: `0x6001_0000 .. 0x6001_FFFF` (64 KB), 16-bit cells.

## 3. Register map

### Identity (R)
`IFACE_MAGIC` (strawman `0x584C`, **TODO(mark)**), `IFACE_VERSION` (`0x0001`).

### Machine control (W)

| Reg | Bits |
| --- | --- |
| `CONTROL` | reset@0 (**level** — SW toggles low/high), pause@1, freezer-enable@2, atari800-mode@3 |
| `RAMCONFIG` | RAM config (bits 2:0) |
| `PERFORMANCE` | 6502 speed/turbo (bits 5:0), VBL-restrict-turbo@8 |
| `CART` | cartridge type (bits 5:0) |
| `VIDEO` | mode (2:0), PAL@4, scanlines@5, composite-sync@6 |

`CONTROL.reset` drives the 6502 reset line directly (this is the RESET key /
system reset). There is no separate cold/warm bit — a cold start is
"clear RAM via DMA, then pulse reset"; a warm start is "pulse reset".

### Keyboard (W)

| Reg | Meaning |
| --- | --- |
| `KBD0`–`KBD3` | 64-key matrix, bit *n* = KBCODE *n* (**TODO(mark): confirm scan order**) |
| `KBD_SPECIAL` | shift@0, ctrl@1, break@2 — the KR2 non-matrix keys |

Shift/Ctrl/Break drive the RTL's `KEYBOARD_SHIFT/CONTROL/BREAK` signals, which
the keyboard-response logic asserts on scan columns `00`(break)/`10`(shift)/
`11`(ctrl). They are NOT matrix positions.

### Console keys — inject/phys (Start/Select/Option)

| Reg | Acc | Meaning |
| --- | --- | --- |
| `CONSOLE_INJECT` | W | inject Start@0/Select@1/Option@2 |
| `CONSOLE_PHYS` | R | physical console switch state (same layout) |

FPGA ORs injected + physical into GTIA. (Reset is NOT a console key — see CONTROL.)

### Joysticks — inject/phys (digital)

| Reg | Acc | Meaning |
| --- | --- | --- |
| `JOY01` | W | inject joystick 0 (bits 4:0) + joystick 1 (bits 12:8); each 4 dir + 1 trigger |
| `JOY23` | W | inject joystick 2 + joystick 3 |
| `JOY01_PHYS` | R | physical joystick ports 0 + 1 (same layout) |
| `JOY23_PHYS` | R | physical joystick ports 2 + 3 |

Physical joystick ports are wired to the FPGA (`inout`). The FPGA ORs injected +
physical into PORTA/PORTB → GTIA (mirroring the console inject/phys model), and
firmware can read the raw physical port state — so USB joysticks (injected from
the STM32) and real Atari joysticks coexist.

### Paddles (W)
`PADDLE01`, `PADDLE23` — two 8-bit axes each, from STM32 ADCs. See §6 (POTGO).

### Freezer debug (W)
`FREEZE_ADDR` (16-bit), `FREEZE_DATA_CTRL` (data 7:0, read@8, write@9, match@10).

### Debug scratch (RW)
`DEBUG0`–`DEBUG3` — no hardware meaning, for bring-up.

## 5. Inject vs physical (console + joysticks)

Console keys and joysticks each have two sources: the FPGA's physical inputs and
firmware injection. Firmware writes the `*_INJECT`/`JOYxx` register to assert;
reads the `*_PHYS` register for real input state. The FPGA ORs them before GTIA.

**RTL must:** OR the injected registers with the debounced physical inputs into
GTIA/PORTA/PORTB; expose raw physical state at the `*_PHYS` registers.

## 6. Interrupt controller

Single FPGA→STM32 line (PG12, EXTI15_10, level-preferred). Three registers:

| Reg | Acc | Meaning |
| --- | --- | --- |
| `IRQ_ENABLE` | RW | mask: a source asserts the line only if its bit is set |
| `IRQ_PENDING` | R | which enabled sources fired (ISR reads to demux) |
| `IRQ_CLEAR` | W1C | write 1 to clear that pending source |

**Write-1-to-clear.** ISR reads `IRQ_PENDING`, handles, writes bits to
`IRQ_CLEAR`. Line deasserts when no enabled+pending bit remains.

Sources: SIO command@0, SIO RX FIFO@1, SIO TX empty@2, POTGO@3, DMA-done@4
(if used), reserved 15:5.

**POTGO** paces paddles: the FPGA raises it when the Atari starts a POT cycle;
the STM32 reads its ADCs and writes `PADDLE01`/`PADDLE23` in response.

## 7. SIO handler

The SIO handler lives in the FSMC adaptor RTL (`sio_handler.vhdl`) — a byte-FIFO
block, addressed at word offsets 0–5 in the SIO region (§2.1). It handles the
SIO serial line to POKEY directly; firmware drive-emulation exchanges SIO frames
through these FIFOs. Turbo/high-speed SIO is negotiated in-protocol by the
handler, so there is no turbo register.

| off | acc | function |
| --- | --- | --- |
| 0 | W | transmit byte (bits 7:0) |
| 1 | R | tx FIFO status: full@9, empty@8, count 7:0 |
| 2 | R | receive: data 14:0; reading advances the RX FIFO |
| 3 | R | rx FIFO status: full@9, empty@8, count 7:0 |
| 4 | W | divisor (applied after transmit completes) |
| 5 | R | framing error: serial@0, command@1 (auto-clears on read) |

The raw SIO command line is surfaced as IRQ source 0 (SIO command), so firmware
is notified of a new SIO command frame rather than polling.

## 8. Atari memory window

64 KB live Atari address space, direct FSMC region (`0x6001_0000`), indexed as
16-bit cells via `fpga_atari_read/write` (bounds-checked). Used for ROM/cart load,
save-state, and RAM clear (write zeroes) for cold start. No DMA descriptor engine.

## 9. Identity / version

`IFACE_MAGIC` / `IFACE_VERSION` read at startup; mismatch → firmware safe-mode.
```
FPGA_IFACE_MAGIC   = 0x584C   /* TODO(mark): finalise */
FPGA_IFACE_VERSION = 0x0001
```

## 10. TODO(mark) the RTL pins down

1. Region offsets of REGS / Atari / SIO (§2).
2. IRQ pin — PG12/EXTI15_10 (firmware default set).
3. `IFACE_MAGIC` value (§9).
4. KBCODE→bit assignment for the 64-key matrix (§3).
5. Reset polarity (does `CONTROL.reset=1` assert or deassert the 6502 reset line).
