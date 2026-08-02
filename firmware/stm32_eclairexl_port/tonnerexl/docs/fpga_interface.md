# TonnereXL — STM32 ↔ FPGA interface contract

**Status:** DRAFT v0.4 · **Owner:** firmware (STM32 side) · **Consumer:** FSMC adaptor RTL (FPGA side)

Authoritative contract between the STM32F407 firmware and the FPGA FSMC adaptor.
The firmware `fpga_bus` HAL implements exactly what is written here; the RTL must
match it. To change a field, change *this document first*, bump the version, then
both sides. **TODO(mark)** marks values the RTL/board decides.

## Changes v0.3 → v0.4 (addressing)

Replaced the flat three-region byte-offset map with an **aperture-banked**
scheme (§2), because the physical space (2 GiB) far exceeds the STM32's 16 MB
FSMC reach. FSMC A22..A19 select between two banked RAM apertures (8 MB + 7 MB,
each with an 8-bit extension register giving physical A29..A22) and a fixed
1 MB region (`A22..A19 = 1111`) that is never banked and holds the registers,
SIO handler, and Atari window — so the aperture extension registers can never be
banked out of reach. Added `APERTURE1_EXT`/`APERTURE2_EXT`. Addresses are now
quoted in word units; memory sizes in bytes.

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

Addresses in this document are **word addresses** (what the FPGA decodes on its
23 FSMC address lines). Memory *sizes* are given in bytes (what people expect);
the bus is 16-bit, so 1 word = 2 bytes.

### 2.0 Two views of the bus

- **STM32 side:** FSMC Bank1 NE1 at CPU base `0x6000_0000`. Firmware pointers are
  `0x6000_0000 + byte offset`.
- **FPGA side:** when NE1 is asserted the FPGA sees only the offset within the
  bank, on 23 word-address lines **FSMC A22..A0** (8M words = 16 MB byte reach).
  The `0x6000_0000` base never reaches the FPGA. On a 16-bit bus the STM32 does
  not drive external A0; FSMC A0 already means "word 1", so decode in word units.

### 2.1 The aperture-banked physical map

The FPGA has a flat physical space of **word addresses A0–A29** (1 Giword =
2 GiB byte) holding all RAM and the registers, split into **64 blocks** of
2^24 words (32 MB byte) each, selected by physical `A29..A24`. The top block
(block 63, `A29..A24 = 111111`) is the **fixed region**, further split 8 ways by
`A21..A19`.

The STM→FPGA conversion is: `phys(21:0) = FSMC(21:0)`; `phys(29:22) =
APERTURE1_EXT` if FSMC A22=0, else `APERTURE2_EXT` — **except** when FSMC A22=1
and FSMC(21:19)=`111`, where `phys(29:24)` is forced all-ones and `phys(23:19)`
to `00000`, deterministically landing the access at the base of block 63
regardless of `APERTURE2_EXT`. That determinism is what makes the fixed region
unbankable: the override ignores the extension register, so the registers can
never be banked out of reach. Note the override also clears `phys(21:19)` to 0,
freeing `phys(18:16)` (a straight passthrough of FSMC A18:A16) to select the
slot within the fixed region — the trigger bits and the slot-select bits are
deliberately different, so they don't collide.

| Physical word | Size (byte) | Block(s) `A29:A24` | Contents |
| --- | --- | --- | --- |
| `0x0000_0000 … 0x07FF_FFFF` | 256 MiB | `000000`–`000111` (8) | SDRAM |
| `0x0800_0000 …` | 32 MiB | `001000` | SRAM1 |
| `0x0900_0000 …` | 32 MiB | `001001` | SRAM2 |
| `0x0A00_0000 …` | 32 MiB | `001010` | ROM |
| … | | | (room to grow) |
| `0x3F00_0000 … 0x3FFF_FFFF` | 32 MiB | `111111` (block 63) | **Fixed region**: Atari / SIO / registers (§2.3) |

The STM32's 23 FSMC lines (16 MB reach) can't see 2 GiB directly, so the big RAM
is reached through **banked apertures**. FSMC A22..A19 select the aperture; each
aperture supplies the high physical bits (A29..A22) from an 8-bit **extension
register**, and FSMC A21..A0 give the offset within it:

| FSMC A22..A19 | Aperture | Intra-window (FSMC) | Window size | High phys bits |
| --- | --- | --- | --- | --- |
| `0xxx` | **Aperture 1** | A21..A0 | 4M word / 8 MB | `EXT1[7:0]` → phys A29..A22 |
| `1000`–`1110` | **Aperture 2** | A21..A0 (top 1/8 removed) | 3.5M word / 7 MB | `EXT2[7:0]` → phys A29..A22 |
| `1111` | **Fixed region** | A18..A0 | 512K word / 1 MB | fixed at top of map — NOT banked |

Physical word address formed for a RAM access:

```
Aperture 1 (A22=0):        phys = EXT1[7:0] & FSMC_A21..A0
Aperture 2 (A22=1,≠1111):  phys = EXT2[7:0] & FSMC_A21..A0
Fixed     (A22..A19=1111): phys = (top-of-map) & FSMC_A18..A0
```

**Two apertures** let the STM32 keep two windows open at once (e.g. source and
destination of a copy) without re-banking between accesses. The STM32 working
set is ≤ 2 MB (usually < 256 KB), so a single aperture position covers any one
chunk and re-banking is infrequent.

**Why a fixed region (the lockout fix).** If the registers lived only inside a
banked aperture, banking that aperture away would make the extension registers
themselves unreachable — with no way back except reset. So the top 1/8 of the
FSMC map (`A22..A19 = 1111`) is a **fixed, unbanked** window onto the top 1 MB of
the physical map, where the registers, SIO handler, and Atari window live. That
window is always reachable, so the aperture extension registers can never lock
themselves out. This is why the registers + Atari window sit together at the top
of the physical map — one fixed 1 MB window must cover them all.

### 2.2 Aperture extension registers

Each aperture's 8 high physical bits come from an extension register in the
fixed region:

| Register | Meaning |
| --- | --- |
| `APERTURE1_EXT` | phys A29..A22 for Aperture 1 (8 bits) |
| `APERTURE2_EXT` | phys A29..A22 for Aperture 2 (8 bits) |

Set the extension, then stream the aperture window. The fixed region needs no
extension register (its target is hardwired).

### 2.3 Fixed-region layout (block 63, always reachable)

Block 63 (`A29..A24 = 111111`) is the fixed region. The conversion clears
`A23..A19` to 0, so the slot is selected by `A18..A16` (a passthrough of FSMC
A18:A16 — free bits, distinct from the `(21:19)` trigger). Each slot is 64K
words wide (`A15..A0`). Physical addresses in the linear FPGA space:

| A18:A16 | Slot | Phys word | Within-slot | Size |
| --- | --- | --- | --- | --- |
| `000` | Atari 64 KB window (§8) | `0x3F00_0000` | A14:A0 | 32K words / 64 KB |
| `110` | SIO handler (§7) | `0x3F06_0000` | A2:A0 | 6 words |
| `111` | Register file (§3–§6) | `0x3F07_0000` | A5:A0 | 31 words |
| others | reserved (NO_SELECT) | | — | — |

The device selects (`DMA_SELECT` / `SIO_SELECT` / `REG_SELECT` / `NO_SELECT`)
are driven from this decode: the Atari-window slot goes to the RAM/DMA path,
SIO and Registers to their handlers, and any other `A18:A16` in block 63 is
`NO_SELECT`.

Slots are generously sized (64K words each) — the Atari window fills half of
its slot, the SIO handler and register file use a handful of words at the base
of theirs. Five slots are spare for future growth.

### 2.4 Register indices

Registers are addressed by their index within the register file (word offset
inside the fixed region). Index *n* below:

| idx | acc | register |
| ---:| --- | --- |
| 0 | R | IFACE_MAGIC |
| 1 | R | IFACE_VERSION |
| 2 | W | CONTROL |
| 3 | W | RAMCONFIG |
| 4 | W | PERFORMANCE |
| 5 | W | CART |
| 6 | W | VIDEO |
| 7 | W | KBD0 |
| 8 | W | KBD1 |
| 9 | W | KBD2 |
| 10 | W | KBD3 |
| 11 | W | KBD_SPECIAL |
| 12 | W | CONSOLE_INJECT |
| 13 | R | CONSOLE_PHYS |
| 14 | W | JOY01 |
| 15 | W | JOY23 |
| 16 | R | JOY01_PHYS |
| 17 | R | JOY23_PHYS |
| 18 | W | PADDLE01 |
| 19 | W | PADDLE23 |
| 20 | W | FREEZE_ADDR |
| 21 | W | FREEZE_DATA_CTRL |
| 22 | RW | IRQ_ENABLE |
| 23 | R | IRQ_PENDING |
| 24 | W1C | IRQ_CLEAR |
| 25 | RW | DEBUG0 |
| 26 | RW | DEBUG1 |
| 27 | RW | DEBUG2 |
| 28 | RW | DEBUG3 |
| 29 | W | APERTURE1_EXT (phys A29..A22 for aperture 1) |
| 30 | W | APERTURE2_EXT (phys A29..A22 for aperture 2) |

The extension registers are write-only; firmware shadows their values.
Firmware: `fpga_aperture_set_ext(aperture, ext)` / `fpga_aperture_get_ext`.

SIO handler (word offsets 0–5 within its part of the fixed region) — matches
`sio_handler.vhdl`:

| off | acc | register |
| ---:| --- | --- |
| 0 | W | SIO_TX (transmit byte) |
| 1 | R | SIO_TX_FIFO (full@9 empty@8 count7:0) |
| 2 | R | SIO_RX (data14:0; read advances) |
| 3 | R | SIO_RX_FIFO (full@9 empty@8 count7:0) |
| 4 | W | SIO_DIVISOR (applied after tx done) |
| 5 | R | SIO_FRAMING_ERR (serial@0 command@1, auto-clear) |

Atari window: 32K words / 64 KB, 16-bit cells, within the fixed region.

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

**Edge-triggered.** Every source latches into `IRQ_PENDING` on the **rising edge
of one signal** — a single edge, not both edges, not a level. After a reset,
pending is clear and each source needs a fresh rising edge to fire.

Sources (each = rising edge of the named signal):

| Bit | Rising edge of | Meaning |
| --- | --- | --- |
| 0 | SIO command line | new SIO command frame beginning |
| 1 | SIO RX not-empty (empty→non-empty) | a byte arrived in the RX FIFO |
| 2 | SIO TX empty (drain→empty) | transmit finished (FIFO drained) |
| 3 | POTGO | Atari started a POT cycle (paddle pacing) |
| 4 | DMA-done | (only if adaptor-assisted DMA is ever used) |
| 15:5 | — | reserved (read 0) |

**RX read-until-empty rule (load-bearing).** Because bit 1 is a *single* rising
edge on empty→non-empty, multiple bytes arriving close together may produce only
one edge (the FIFO went non-empty once and stayed non-empty). So on each RX wake
the firmware MUST drain the FIFO until `SIO_RX_FIFO` reports empty — it cannot
assume one IRQ equals one byte. (`drive_service_step` does `while
(fpga_sio_getc())`, satisfying this.)

The IRQ says *when* to look; the FIFO status registers (`SIO_TX_FIFO`,
`SIO_RX_FIFO`) say *what state* — poll them for counts/full/empty detail. If the
opposite transition is ever needed (e.g. TX empty→non-empty, or RX
non-empty→empty), add it as a separate IRQ bit (5–15 are free).

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
