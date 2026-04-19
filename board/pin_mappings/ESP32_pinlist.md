# ESP32 module (U34: ESP32‑WROVER‑E) pin usage summary and pinlist

Derived from the KiCad netlist (`tonnere.net`). The ESP32 GPIO matrix allows
flexible peripheral routing, so this is primarily a *wiring* pinlist showing
which module pads are used and what they connect to.

## Connectivity overview

- **SPI to FPGA**: 4 pins (SCK/MOSI/MISO/CS) — direct to Cyclone 10 U30,
  no level-shifter. FPGA is bus master.
- **Atari SIO**: 8 pins — directly wired to the SIO signals (shared with FPGA
  U30 via bus switch U31). The ESP32 can participate in SIO traffic
  independently of the FPGA.
- **UART to STM32**: 2 pins (RXD0/TXD0) — direct to STM32 USART6 (PC6/PC7).
  Also broken out on J18 (ESP PGM header).
- **GPIO handshake to/from STM32**: 4 pins — 3 input-only (SGPI0-2 from
  STM32 PG8-10) + 1 output (SGPIO to STM32 PA9). None pass through the FPGA.
- **JTAG**: 4 pins (optional, directly on ESP GPIOs via GPIO matrix).
- **Audio**: 1 pin (ESPAUD.SIO via bus switch U31).
- **BOOT/EN**: IO0 (boot strap) and EN (chip enable), directly also connected to
  STM32 (PG6/PG7) and broken out on J18 header.

## Pinlist
| Module pad | ESP32 signal | GPIO | Function | Net | Notes |
|---:|---|---|---|---|---|
| 3 | EN |  | EN (chip enable/reset) | /ESP32/ESP.EN | Pull-up (R99); also on J18 ESP PGM header |
| 4 | SENSOR_VP/IO36 | IO36 | GPIO from STM32 | /ESP32/ESP.SGPI0 | Input-only; <- STM32 PG8 |
| 5 | SENSOR_VN/IO39 | IO39 | GPIO from STM32 | /ESP32/ESP.SGPI1 | Input-only; <- STM32 PG9 |
| 6 | IO34 | IO34 | GPIO from STM32 | /ESP32/ESP.SGPI2 | Input-only; <- STM32 PG10 |
| 7 | IO35 | IO35 | Atari SIO | /AtariInterfaces/SIO.MOTOR | Also to FPGA U30; ext pull-up +5V (R30) |
| 8 | IO32 | IO32 | Atari SIO | /AtariInterfaces/SIO.INTERRUPT | Also to FPGA U30; ext pull-up +5V (RN3) |
| 9 | IO33 | IO33 | Atari SIO | /AtariInterfaces/SIO.COMMAND | Also to FPGA U30; ext pull-up +5V (RN3) |
| 10 | IO25 | IO25 | Atari SIO | /AtariInterfaces/SIO.DATA_IN | Also to FPGA U30; ext pull-up +5V (RN4) |
| 11 | IO26 | IO26 | Audio SIO | /Audio/ESPAUD.SIO | Audio SIO; via bus switch U31 |
| 12 | IO27 | IO27 | Atari SIO | /AtariInterfaces/SIO.PROCEED | Also to FPGA U30; ext pull-up +5V (RN3) |
| 13 | IO14 | IO14 | JTAG (optional) | /ESP32/ESPJTAG.TMS | Via GPIO matrix |
| 14 | IO12 | IO12 | JTAG (optional) | /ESP32/ESPJTAG.TDI | Via GPIO matrix |
| 16 | IO13 | IO13 | JTAG (optional) | /ESP32/ESPJTAG.TCK | Via GPIO matrix |
| 23 | IO15 | IO15 | JTAG (optional) | /ESP32/ESPJTAG.TDO | Via GPIO matrix |
| 24 | IO2 | IO2 | GPIO to STM32 | /ESP32/ESP.SGPIO | -> STM32 PA9 |
| 25 | IO0 | IO0 | BOOT strap (GPIO0) | /ESP32/ESP.BOOT | Low=bootloader; also STM32 PG6 and J18 header |
| 26 | IO4 | IO4 | Atari SIO | /AtariInterfaces/SIO.CLOCK_OUT | Also to FPGA U30; ext pull-up +5V (RN4) |
| 29 | IO5 | IO5 | SPI to FPGA (U30) | /ESP32/ESP.CS | CS active low; pull-up to +3V3 (R94) |
| 30 | IO18 | IO18 | SPI to FPGA (U30) | /ESP32/ESP.SCK | Direct to FPGA U30 |
| 31 | IO19 | IO19 | SPI to FPGA (U30) | /ESP32/ESP.MISO | Direct to FPGA U30 |
| 33 | IO21 | IO21 | Atari SIO | /AtariInterfaces/SIO.CLOCK_IN | Also to FPGA U30; ext pull-up +5V (RN4) |
| 34 | RXD0 | RXD0 | UART to STM32 | /ESP32/UART1.TX | STM32 USART6 RX (PC7); also on J18 header |
| 35 | TXD0 | TXD0 | UART to STM32 | /ESP32/UART1.RX | STM32 USART6 TX (PC6); also on J18 header |
| 36 | IO22 | IO22 | Atari SIO | /AtariInterfaces/SIO.DATA_OUT | Also to FPGA U30; ext pull-up +5V (RN4) |
| 37 | IO23 | IO23 | SPI to FPGA (U30) | /ESP32/ESP.MOSI | Direct to FPGA U30 |
