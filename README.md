## Project Tonnere

The EclaireXL was a partially open Atari XL FPGA implementation designed almost a decade ago. This project is intended to be a fully open community follow up version.

## Status

For now there is only a hardware design from which a physical board has been produced. The design goals of this are described below.

## Memory

3 independent banks of RAM from FPGA

- 512KB 16-bit wide 10ns SRAM (for VBXE)
- 2048KB 16-bit wide 10ns SRAM (primary Atari core)
- 32MB 16-bit wide SDRAM (secondary Atari core etc - for large memory expansions, cartridge emulation etc)

## Video output

- HDMI driven directly by FPGA
- Analog video driven by FPGA as sigma delta and/or pattern based DAC, filtered/buffered by THS7316.
- NTSC in 480i or 480p, PAL in 576i or 576p
- Polyphasic and area-based scaler allowing 720p, 1080i etc

## Audio output

- PCM5102A DAC

## Audio input

- 12-bit ADC for PBI, SIO, 3.5mm left/right on STM microcontroller

## Joystick ports 1-4

- Directly connected to FPGA (via level shifters)
- Paddle support using STM ADC inputs

## Six USB ports

- 1 USB A internal for keyboard
- 3 USB A external for joystick, keyboard etc
- 1 USB C for power only (next to power switch)
- 1 USB C for STM32 DFU (to program it)

## Atari peripheral emulator (ESP32 microcontroller/almost Fujinet)

- Wifi support
- Serial connection to STM32
- SPI master to FPGA (for DMA to RAM, faster than serial via STM)
- SIO pins (all)
- Audio out DAC goes to SIO audio in
- Slave to STM32
- Slave to Atari via SIO
- JTAG for debugging

## Firmware/control (STM32)

- SPI master to FPGA SPI flash chip (to program)
- Can restart FPGA
- SRAM port connected to FPGA (for DMA to Atari core RAM + RAM banks)
- IRQ from FPGA
- 4-bit external SD card interface
- SPI to internal micro SD card (will appear as IDE hard drive to Atari core)
- ADC for sampling (see audio in)
- Serial connection for communication for Atari peripheral emulator
- USB port + stack
- DFU mode for programming, can then program FPGA flash and ESP32 via serial (+BOOT etc).
- HDMI i2c link (to get mode support)
- PLL i2c link (4 programmable clocks for FPGA)
- Samples joysticks 3 and 4, tells FPGA.
- SWD port for debugging
- USART port for expansion
- Will run menus etc, via DMA to Atari core (+ potentially framebuffer)

## Atari core (Cyclone 10 LP FPGA)

- Core Atari XL and Atari 800 support
- 2x/4x colour clock Antic/GTIA support
- VBXE
- PokeyMAX
- HDD via IDE emulation on microsd
- Turbo freezer
- Common cartridge banking logic (on RAM)

## Atari ports

- Left cartridge
- PBI with standard extensions (enabled/selected via jumper), 50 pin edge and 50 pin header option.
- SIO (with motor control, without 12V)
- Start/select/option switch headers
- 4 joystick ports
