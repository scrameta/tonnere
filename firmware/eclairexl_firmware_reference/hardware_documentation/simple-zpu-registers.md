# Simple ZPU register map

This document describes the memory-mapped peripherals implemented by
`zpu_config_regs.vhdl`, and the names used by `firmware_eclairexl/regs.h`.
It documents the register interface rather than the ZPU instruction set.

## Addressing

The ZPU uses byte addresses and 32-bit words.  The configuration window starts
at `0x0004_0000`; consequently register *n* is at `0x0004_0000 + 4*n`.
All registers in the first block are 32 bits unless a narrower field is shown.
Unlisted read bits are zero.  The generic input and output registers are ports
between this block and the board top level, so their meanings are platform
specific (see [Platform-level bit assignments](#platform-level-bit-assignments)).

The top-level map is:

| ZPU byte range | Contents |
| --- | --- |
| `0x0000_0000`-`0x0000_ffff` | ZPU local ROM/RAM |
| `0x0001_0000`-`0x0001_ffff` | live Atari address space |
| `0x0002_0000`-`0x0002_ffff` | Atari/savestate mirror |
| `0x0004_0000`-`0x0004_03ff` | simple registers described below |
| `0x0004_0400`-`0x0004_07ff` | SIO UART |
| `0x0004_0800`-`0x0004_0bff` | USB host/slave instance 0 |
| `0x0004_0c00`-`0x0004_0fff` | USB host/slave instance 1 |
| `0x0004_1000`-`0x0004_13ff` | USB host/slave instance 2 |
| `0x0004_1400`-`0x0004_17ff` | USB host/slave instance 3 |
| `0x0004_1800`-`0x0004_1bff` | USB host/slave instance 4 |
| `0x0004_1c00`-`0x0004_1fff` | board PLL/reconfiguration interface |

Only the number of USB instances selected by the HDL `usb` generic exists. The
USB host/slave registers themselves are intentionally outside the scope of this
document.

## Simple registers

| Address | Firmware name | Access | Meaning |
| --- | --- | --- | --- |
| `0x40000` | `zpu_in1` | R | Generic input 1. |
| `0x40004` | `zpu_in2` | R | Generic input 2. |
| `0x40008` | `zpu_in3` | R | Generic input 3. |
| `0x4000c` | `zpu_in4` | R | Generic input 4. |
| `0x40010` | `zpu_out1` | R/W | Generic output 1; resets to zero. |
| `0x40014` | `zpu_out2` | R/W | Generic output 2; resets to zero. |
| `0x40018` | `zpu_out3` | R/W | Generic output 3; resets to zero. |
| `0x4001c` | `zpu_out4` | R/W | Generic output 4; resets to zero. |
| `0x40020` | `zpu_pause` / `zpu_timer` | W/R | Write a delay in microseconds; the ZPU stalls while it counts down. Read the free-running 32-bit microsecond counter. |
| `0x40024` | `zpu_spi_data` | W/R | Write bits 7:0 to start one SPI byte transfer; read bits 7:0 for the last received byte. Poll SPI state before reading. |
| `0x40028` | `zpu_spi_state` | W/R | SPI control on write; SPI busy status on read (below). |
| `0x4002c` | `zpu_sio` | R | Bit 0 is the raw SIO command input; other bits are zero. |
| `0x40030` | `zpu_board` | R | 32-bit HDL `platform` generic identifying the board. |
| `0x40034` | `zpu_spi_dma` | W | Start an SPI-to-local-memory transfer (below). |
| `0x40038` | `zpu_out5` | R/W | Generic output 5; resets to zero. |
| `0x4003c` | `zpu_out6` | R/W | Generic output 6; resets to zero. |
| `0x40040` | `zpu_i2c_0` | W/R | I2C controller 0 command/status (below). |
| `0x40044` | `zpu_i2c_1` | W/R | I2C controller 1 command/status (same format). |
| `0x40048` | `zpu_timer2_threshold` / `zpu_timer2` | W/R | Write wrap threshold; read current microsecond counter (below). |
| `0x4004c` | `zpu_rand` | R | Bits 7:0 contain a continuously running 17-bit polynomial LFSR result. |
| `0x40050` | `zpu_out7` | R/W | Generic output 7; resets to zero. |
| `0x40054` | `zpu_out8` | R/W | Generic output 8; resets to zero. |

### SPI control (`0x40028`)

On a write, bit 0 selects SPI slave 0 or 1, bits 2:1 drive the two active-low
chip-select outputs, and bit 3 selects the clock rate.  When bit 3 is one the
divider is fixed at `0x80` for slow initialization; when it is zero the HDL
`spi_clock_div` generic is used.  Bits 31:4 are ignored.  On a read, bit 0 is
one while the SPI engine is busy and bits 31:1 are zero.

### SPI DMA (`0x40034`)

| Bits | Write meaning |
| --- | --- |
| 15:0 | First 16-bit local-memory byte address. |
| 31:16 | Exclusive end address. |

Writing starts repeated `0xff` SPI transfers. Each received byte is written to
local memory, beginning at the low-half address, and the ZPU remains stalled
until the incremented address equals the end address.  In normal use the end
must therefore be greater than the start; an empty/reversed range will wrap the
16-bit address counter.

### I2C command and status (`0x40040`, `0x40044`)

On write, bits 15:9 are the 7-bit slave address, bit 8 is `1` for read or `0`
for write, and bits 7:0 are write data. Bits 31:16 are ignored. A write launches
one transaction and briefly stalls the ZPU while the controller accepts it.

On read, bits 7:0 are received data, bit 8 is busy, and bit 9 is the acknowledge
error indication. Bits 31:10 are zero. Firmware should wait for busy to clear
before consuming received data or launching the next command.

### Timer 2 (`0x40048`)

Timer 2 increments once per microsecond. A write sets its 32-bit threshold. The
counter resets to zero whenever its current value is greater than or equal to
that threshold. Since both counter and threshold reset to zero, software should
program a nonzero threshold before relying on this timer. The threshold itself
is not readable at this address; reads return the counter.

## SIO UART

The UART is at `0x40400`; its registers are also word-spaced. Only the low 16
read bits and low 8 write bits are implemented.

| Address | Firmware name | Access | Bits and side effects |
| --- | --- | --- | --- |
| `0x40400` | `zpu_uart_tx` | W | Bits 7:0 enqueue a transmit byte. |
| `0x40404` | `zpu_uart_tx_fifo` | R | Bit 9 full, bit 8 empty, bits 7:0 queued-byte count. |
| `0x40408` | `zpu_uart_rx` | R | If nonempty, bits 7:0 data, bits 14:8 captured divisor; reading advances the receive FIFO. Firmware notes that the first read requests the next value and may be stale. |
| `0x4040c` | `zpu_uart_rx_fifo` | R | Bit 9 full, bit 8 empty, bits 7:0 queued-byte count. |
| `0x40410` | `zpu_uart_divisor` | W/R | Write bits 7:0 as the pending transmit divisor (applied once transmission is idle); read bits 7:0 as the measured receive divisor. Bit rate is `pokey_enable_frequency / (divisor + 6) / 2`. |
| `0x40414` | `zpu_uart_framing_error` | R | Bit 0 serial framing error, bit 1 SIO-command framing error. Reading clears both indications. |

The firmware header names offsets 7-9 as debug registers, but the current UART
HDL does not implement them; reads return zero and writes have no effect.

## PLL interface

The board-specific PLL/reconfiguration target occupies `0x41c00`-`0x41fff`.
A write passes all 32 data bits to the external block and uses word-address bits
7:2 as its register address. Reads are not implemented by this register block.
`firmware_eclairexl/regs.h` currently names the `go`, `m`, `c`, fractional-M,
bandwidth and charge-pump words; their interpretation belongs to the selected
board's PLL block rather than the simple ZPU register set.

## Platform-level bit assignments

The generic registers expose wires, not a platform-independent protocol. The
firmware gives the following common Atari assignments, and most full Atari 800
top levels connect them this way. A board may tie inputs low, leave outputs open,
or repurpose them (notably MiST storage control), so verify its top-level HDL.

### Generic input 1 (`0x40000`)

| Bits | Common firmware meaning |
| --- | --- |
| 8 | Soft-boot hotkey. |
| 9 | Cold-boot hotkey. |
| 10 | Settings-menu hotkey. |
| 11 | File-selector hotkey. |
| 17:12 | Controls, conventionally Escape/Fire/Left/Right/Down/Up. |
| 18 | SD-card detect. |
| 19 | SD-card write protect. |
| 28:26 | Turbo-drive feedback. |
| 29 | MiST SD acknowledge. |
| 31:30 | MiST SD mounted/event state. |

Inputs 3 and 4 commonly carry the lower and upper halves of the 64-bit keyboard
matrix. Input 2 is normally zero, but MiST uses it for SD image size or sector
buffer data.

### Generic output 1 (`0x40010`)

| Bits | Common firmware meaning |
| --- | --- |
| 0 | Pause the Atari core. |
| 1 | Reset the Atari core. |
| 7:2 | 6502 speed/turbo selection. |
| 10:8 | RAM configuration selection. |
| 11 | Atari 800 mode. |
| 22:17 | Emulated cartridge type/selection. |
| 25 | Freezer enable. |
| 27:26 | Keyboard type (`ANSI`, `ISO`, or custom mapping). Some HDL consumes only bit 26. |
| 30:28 | Turbo-drive selection. |
| 31 | Restrict 6502 turbo operation to vertical blank. |

### Other common generic outputs

* Output 2 and output 3 conventionally inject USB joystick 0 and 1 state.
  Bits 3:0 are directions and bits 5:4 are trigger inputs; the exact ordering
  is determined by the board connection. MiST instead repurposes much of output
  3 for SD transfers.
* Output 4 carries keyboard injection data from the USB firmware.
* Output 5 commonly packs four unsigned analog axes: joystick 1 X in bits 7:0,
  joystick 1 Y in 15:8, joystick 2 X in 23:16, and joystick 2 Y in 31:24.
* Output 6 uses bits 2:0 for video mode, bit 4 for PAL/TV standard, bit 5 for
  scanlines, and bit 6 for composite sync.
* Output 7 is an optional freezer debug port: address in bits 15:0, data in
  bits 23:16, read/write strobes in bits 24/25, and data-match mode in bit 26.
* Output 8 currently has no common assignment.

## Firmware use

Firmware should access these locations through the `volatile` pointers in
`firmware_eclairexl/regs.h`. For read/write registers, use a read-modify-write
when changing one shared bit field. The HDL initializes all generic outputs and
state registers to zero except SPI control, which resets to slave 1, both
chip-selects inactive, and slow speed.
