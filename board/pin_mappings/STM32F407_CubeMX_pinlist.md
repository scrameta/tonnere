# STM32F407ZG (U19) CubeMX-style summary and pinlist

Derived from the KiCad netlist (`tonnere.net`). Reflects which STM32 pins are
wired to which functions, with alternate-function assignments where applicable.

## Peripheral summary

- **FMC/FSMC** (AF12): 47 pins — external bus to FPGA U30 (address/data/control).
  Direct wiring, no bus switch.
- **SPI** (AF5): 4 pins (PB3-PB7) — shared active-serial/SD/flash SPI bus.
  PB3 (SCK) via series R52.  PB6/PB7 are chip-selects (CS1=SD, CS2=FPGA flash).
- **USART3** (AF7): 5 pins (PB10-PB14) — general-purpose USART.
  Note: net labels say USART1 but the STM32 AF7 on PB10-14 is USART3.
- **USART6** (AF8): 2 pins (PC6/PC7) — UART to ESP32 (RXD0/TXD0).
  Also broken out on J18 (ESP PGM header).
- **I2C1** (AF4): 2 pins (PB8/PB9) + 1 GPIO reset (PC5).
- **SDIO** (AF12): 6 pins (PC8-PC12, PD2) — SD card interface via series
  resistor networks RN7 (CMD/CK) and RN10 (D0-D3). PC13 is SD card detect (GPIO).
- **Debug**: PA13/PA14 (SWDIO/SWCLK) on J9 SWD header.
  PC14/PC15 are OSC32_IN/OUT (32.768 kHz RTC crystal Y2).
- **ADC analog inputs**: joystick pots (PA0-1/PA4-5/PC0-3) and audio ADC (PA2-3/PA6-7).
- **ESP32 GPIO**: PG6/PG7 -> ESP32 IO0/EN (boot/reset control, also on J18).
  PG8-10 -> ESP32 input-only SGPI0-2.  PA9 <- ESP32 SGPIO (IO2).
- **FPGA control**: PG11 -> MSEL1, PG12 <- IRQ, PG13 -> nCONFIG,
  PG14 <- nSTATUS, PG15 <- CONF_DONE.

## Pinlist
| STM32 pin | LQFP pin | Mode (CubeMX) | AF | User label | Net | Notes |
|---|---:|---|---|---|---|---|
| PA13 | 105 | Debug |  | SWDIO | Net-(J9-Pin_2) | J9 SWD header pin 2 |
| PA14 | 109 | Debug |  | SWCLK | Net-(J9-Pin_3) | J9 SWD header pin 3 |
| PA11 | 103 | FSMC (AF12) | AF12 | FSMC | Net-(U19-PA11) |  |
| PA12 | 104 | FSMC (AF12) | AF12 | FSMC | Net-(U19-PA12) |  |
| PD0 | 114 | FSMC (AF12) | AF12 | FSMC_D2 | /FPGA/FSMC.D2 |  |
| PD1 | 115 | FSMC (AF12) | AF12 | FSMC_D3 | /FPGA/FSMC.D3 |  |
| PD10 | 79 | FSMC (AF12) | AF12 | FSMC_D15 | /FPGA/FSMC.D15 |  |
| PD11 | 80 | FSMC (AF12) | AF12 | FSMC_A16 | /FPGA/FSMC.A16 |  |
| PD12 | 81 | FSMC (AF12) | AF12 | FSMC_A17 | /FPGA/FSMC.A17 |  |
| PD13 | 82 | FSMC (AF12) | AF12 | FSMC_A18 | /FPGA/FSMC.A18 |  |
| PD14 | 85 | FSMC (AF12) | AF12 | FSMC_D0 | /FPGA/FSMC.D0 |  |
| PD15 | 86 | FSMC (AF12) | AF12 | FSMC_D1 | /FPGA/FSMC.D1 |  |
| PD4 | 118 | FSMC (AF12) | AF12 | FSMC_NOE | /FPGA/FSMC.NOE |  |
| PD5 | 119 | FSMC (AF12) | AF12 | FSMC_NWE | /FPGA/FSMC.NWE |  |
| PD6 | 122 | FSMC (AF12) | AF12 | FSMC_NWAIT | /FPGA/FSMC.NWAIT |  |
| PD7 | 123 | FSMC (AF12) | AF12 | FSMC_NE1 | /FPGA/FSMC.NE1 |  |
| PD8 | 77 | FSMC (AF12) | AF12 | FSMC_D13 | /FPGA/FSMC.D13 |  |
| PD9 | 78 | FSMC (AF12) | AF12 | FSMC_D14 | /FPGA/FSMC.D14 |  |
| PE0 | 141 | FSMC (AF12) | AF12 | FSMC_NBL0 | /FPGA/FSMC.NBL0 |  |
| PE1 | 142 | FSMC (AF12) | AF12 | FSMC_NBL1 | /FPGA/FSMC.NBL1 |  |
| PE10 | 63 | FSMC (AF12) | AF12 | FSMC_D7 | /FPGA/FSMC.D7 |  |
| PE11 | 64 | FSMC (AF12) | AF12 | FSMC_D8 | /FPGA/FSMC.D8 |  |
| PE12 | 65 | FSMC (AF12) | AF12 | FSMC_D9 | /FPGA/FSMC.D9 |  |
| PE13 | 66 | FSMC (AF12) | AF12 | FSMC_D10 | /FPGA/FSMC.D10 |  |
| PE14 | 67 | FSMC (AF12) | AF12 | FSMC_D11 | /FPGA/FSMC.D11 |  |
| PE15 | 68 | FSMC (AF12) | AF12 | FSMC_D12 | /FPGA/FSMC.D12 |  |
| PE3 | 2 | FSMC (AF12) | AF12 | FSMC_A19 | /FPGA/FSMC.A19 |  |
| PE4 | 3 | FSMC (AF12) | AF12 | FSMC_A20 | /FPGA/FSMC.A20 |  |
| PE5 | 4 | FSMC (AF12) | AF12 | FSMC_A21 | /FPGA/FSMC.A21 |  |
| PE6 | 5 | FSMC (AF12) | AF12 | FSMC_A22 | /FPGA/FSMC.A22 |  |
| PE7 | 58 | FSMC (AF12) | AF12 | FSMC_D4 | /FPGA/FSMC.D4 |  |
| PE8 | 59 | FSMC (AF12) | AF12 | FSMC_D5 | /FPGA/FSMC.D5 |  |
| PE9 | 60 | FSMC (AF12) | AF12 | FSMC_D6 | /FPGA/FSMC.D6 |  |
| PF0 | 10 | FSMC (AF12) | AF12 | FSMC_A0 | /FPGA/FSMC.A0 |  |
| PF1 | 11 | FSMC (AF12) | AF12 | FSMC_A1 | /FPGA/FSMC.A1 |  |
| PF12 | 50 | FSMC (AF12) | AF12 | FSMC_A6 | /FPGA/FSMC.A6 |  |
| PF13 | 53 | FSMC (AF12) | AF12 | FSMC_A7 | /FPGA/FSMC.A7 |  |
| PF14 | 54 | FSMC (AF12) | AF12 | FSMC_A8 | /FPGA/FSMC.A8 |  |
| PF15 | 55 | FSMC (AF12) | AF12 | FSMC_A9 | /FPGA/FSMC.A9 |  |
| PF2 | 12 | FSMC (AF12) | AF12 | FSMC_A2 | /FPGA/FSMC.A2 |  |
| PF3 | 13 | FSMC (AF12) | AF12 | FSMC_A3 | /FPGA/FSMC.A3 |  |
| PF4 | 14 | FSMC (AF12) | AF12 | FSMC_A4 | /FPGA/FSMC.A4 |  |
| PF5 | 15 | FSMC (AF12) | AF12 | FSMC_A5 | /FPGA/FSMC.A5 |  |
| PG0 | 56 | FSMC (AF12) | AF12 | FSMC_A10 | /FPGA/FSMC.A10 |  |
| PG1 | 57 | FSMC (AF12) | AF12 | FSMC_A11 | /FPGA/FSMC.A11 |  |
| PG2 | 87 | FSMC (AF12) | AF12 | FSMC_A12 | /FPGA/FSMC.A12 |  |
| PG3 | 88 | FSMC (AF12) | AF12 | FSMC_A13 | /FPGA/FSMC.A13 |  |
| PG4 | 89 | FSMC (AF12) | AF12 | FSMC_A14 | /FPGA/FSMC.A14 |  |
| PG5 | 90 | FSMC (AF12) | AF12 | FSMC_A15 | /FPGA/FSMC.A15 |  |
| NRST | 25 | GPIO (TBD) |  | NRST | /Microcontroller/NRST |  |
| Net-(JP1-C) | 138 | GPIO (TBD) |  | Net-(JP1-C) | Net-(JP1-C) |  |
| PA0 | 34 | GPIO (TBD) |  | JOY2.POT4 | /AtariInterfaces/JOY2.POT4 | Analog input (ADC) for paddle/pot |
| PA1 | 35 | GPIO (TBD) |  | JOY2.POT5 | /AtariInterfaces/JOY2.POT5 | Analog input (ADC) for paddle/pot |
| PA2 | 36 | GPIO (TBD) |  | STMAUD.ADC.L | /Audio/STMAUD.ADC.L |  |
| PA3 | 37 | GPIO (TBD) |  | STMAUD.ADC.R | /Audio/STMAUD.ADC.R |  |
| PA4 | 40 | GPIO (TBD) |  | JOY2.POT6 | /AtariInterfaces/JOY2.POT6 | Analog input (ADC) for paddle/pot |
| PA5 | 41 | GPIO (TBD) |  | JOY2.POT7 | /AtariInterfaces/JOY2.POT7 | Analog input (ADC) for paddle/pot |
| PA6 | 42 | GPIO (TBD) |  | STMAUD.ADC.PBI | /Audio/STMAUD.ADC.PBI |  |
| PA7 | 43 | GPIO (TBD) |  | STMAUD.ADC.SIO | /Audio/STMAUD.ADC.SIO |  |
| PA9 | 101 | GPIO (TBD) |  | ESP.SGPIO | /ESP32/ESP.SGPIO | ESP32 IO2 (SGPIO) via R92 |
| PB2 | 48 | GPIO (TBD) |  | BOOT1 | /Microcontroller/BOOT1 |  |
| PC0 | 26 | GPIO (TBD) |  | JOY.POT0 | /AtariInterfaces/JOY.POT0 | Analog input (ADC) for paddle/pot |
| PC1 | 27 | GPIO (TBD) |  | JOY.POT1 | /AtariInterfaces/JOY.POT1 | Analog input (ADC) for paddle/pot |
| PC2 | 28 | GPIO (TBD) |  | JOY.POT2 | /AtariInterfaces/JOY.POT2 | Analog input (ADC) for paddle/pot |
| PC3 | 29 | GPIO (TBD) |  | JOY.POT3 | /AtariInterfaces/JOY.POT3 | Analog input (ADC) for paddle/pot |
| PD3 | 117 | GPIO (TBD) |  | Net-(RN7-R2.2) | Net-(RN7-R2.2) |  |
| PF10 | 22 | GPIO (TBD) |  | Net-(J10-Pin_3) | Net-(J10-Pin_3) |  |
| PF6 | 18 | GPIO (TBD) |  | Net-(J10-Pin_7) | Net-(J10-Pin_7) |  |
| PF7 | 19 | GPIO (TBD) |  | Net-(J10-Pin_6) | Net-(J10-Pin_6) |  |
| PF8 | 20 | GPIO (TBD) |  | Net-(J10-Pin_5) | Net-(J10-Pin_5) |  |
| PF9 | 21 | GPIO (TBD) |  | Net-(J10-Pin_4) | Net-(J10-Pin_4) |  |
| PG10 | 125 | GPIO (TBD) |  | ESP.SGPI2 | /ESP32/ESP.SGPI2 | ESP32 input-only IO34 (SGPI2) |
| PG11 | 126 | GPIO (TBD) |  | FPGA.PS_N | /FPGA/FPGA.PS_N |  |
| PG12 | 127 | GPIO (TBD) |  | FPGA.IRQ | /FPGA/FPGA.IRQ |  |
| PG13 | 128 | GPIO (TBD) |  | FPGA.CONFIG_N | /FPGA/FPGA.CONFIG_N |  |
| PG14 | 129 | GPIO (TBD) |  | FPGA.STATUS_N | /FPGA/FPGA.STATUS_N |  |
| PG15 | 132 | GPIO (TBD) |  | FPGA.CONF_DONE | /FPGA/FPGA.CONF_DONE |  |
| PG6 | 91 | GPIO (TBD) |  | ESP.BOOT | /ESP32/ESP.BOOT | ESP32 IO0 boot strap; also on J18 header |
| PG7 | 92 | GPIO (TBD) |  | ESP.EN | /ESP32/ESP.EN | ESP32 EN; also on J18 header |
| PG8 | 93 | GPIO (TBD) |  | ESP.SGPI0 | /ESP32/ESP.SGPI0 | ESP32 input-only IO36 (SGPI0) |
| PG9 | 124 | GPIO (TBD) |  | ESP.SGPI1 | /ESP32/ESP.SGPI1 | ESP32 input-only IO39 (SGPI1) |
| PH0 | 23 | GPIO (TBD) |  | Net-(U19-PH0) | Net-(U19-PH0) |  |
| PH1 | 24 | GPIO (TBD) |  | Net-(U19-PH1) | Net-(U19-PH1) |  |
| PB8 | 139 | I2C (AF4) | AF4 | I2C1_SCL | /Microcontroller/STM.SCL |  |
| PB9 | 140 | I2C (AF4) | AF4 | I2C1_SDA | /Microcontroller/STM.SDA |  |
| PC5 | 45 | I2C (AF4) | AF4 | I2C | /Microcontroller/STM.I2CRESET | Reset line for I2C devices? (GPIO) |
| PC14 | 8 | RTC |  | OSC32_IN | Net-(U19-PC14) | 32.768 kHz crystal Y2 |
| PC15 | 9 | RTC |  | OSC32_OUT | Net-(U19-PC15) | 32.768 kHz crystal Y2 |
| PC10 | 111 | SDIO (AF12) | AF12 | SDIO_D2 | Net-(RN10-R2.2) | Via RN10 to SD card D2 |
| PC11 | 112 | SDIO (AF12) | AF12 | SDIO_D3 | Net-(RN10-R1.2) | Via RN10 to SD card D3 |
| PC12 | 113 | SDIO (AF12) | AF12 | SDIO_CK | Net-(RN7-R4.2) | Via RN7 to SD card CLK |
| PC13 | 7 | SDIO |  | SD_DETECT | Net-(RN7-R3.2) | Via RN7 to SD card detect |
| PC8 | 98 | SDIO (AF12) | AF12 | SDIO_D0 | Net-(RN10-R4.2) | Via RN10 to SD card D0 |
| PC9 | 99 | SDIO (AF12) | AF12 | SDIO_D1 | Net-(RN10-R3.2) | Via RN10 to SD card D1 |
| PD2 | 116 | SDIO (AF12) | AF12 | SDIO_CMD | Net-(RN7-R1.2) | Via RN7 to SD card CMD |
| PB3 | 133 | SPI (AF5) | AF5 | SPI1_SCK | Net-(U19-PB3) | Via series R52 to SPI.SCK bus |
| PB4 | 134 | SPI (AF5) | AF5 | SPI1_MISO | /FPGA/SPI.MISO |  |
| PB5 | 135 | SPI (AF5) | AF5 | SPI1_MOSI | /FPGA/SPI.MOSI |  |
| PB6 | 136 | SPI (AF5) | AF5 | SPI1_NSS1 (GPIO or NSS) | /FPGA/SPI.CS1 | Chip-select likely GPIO (or SPI NSS depending config) |
| PB7 | 137 | SPI (AF5) | AF5 | SPI1_NSS2 (GPIO) | /FPGA/SPI.CS2 | Chip-select likely GPIO (or SPI NSS depending config) |
| PB10 | 69 | USART/UART (AF7 (USART3)) | AF7 (USART3) | USART3_TX | /Microcontroller/USART1.TX |  |
| PB11 | 70 | USART/UART (AF7 (USART3)) | AF7 (USART3) | USART3_RX | /Microcontroller/USART1.RX |  |
| PB12 | 73 | USART/UART (AF7 (USART3)) | AF7 (USART3) | USART3_CK | /Microcontroller/USART1.CK |  |
| PB13 | 74 | USART/UART (AF7 (USART3)) | AF7 (USART3) | USART3_CTS | /Microcontroller/USART1.CTS |  |
| PB14 | 75 | USART/UART (AF7 (USART3)) | AF7 (USART3) | USART3_RTS | /Microcontroller/USART1.RTS |  |
| PC6 | 96 | USART/UART (AF8 (USART6)) | AF8 (USART6) | USART6_TX | /ESP32/UART1.TX | ESP32 RXD0; also on J18 header |
| PC7 | 97 | USART/UART (AF8 (USART6)) | AF8 (USART6) | USART6_RX | /ESP32/UART1.RX | ESP32 TXD0; also on J18 header |
