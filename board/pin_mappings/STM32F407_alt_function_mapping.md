# STM32F407ZG (U19) pin usage and alternate functions (from tonnere.net)

## FSMC (47 pins)
AF used: **AF12** (FSMC/FMC). Total FSMC-related pins in netlist: 47.
See the CSV/XLSX for the full per-pin list.

## SPI (4 pins)
| STM32 pin | Package pin | Signal | AF | Net | Note |
|---|---:|---|---|---|---|
| PB4 | 134 | SPI1_MISO | AF5 | /FPGA/SPI.MISO |  |
| PB5 | 135 | SPI1_MOSI | AF5 | /FPGA/SPI.MOSI |  |
| PB6 | 136 | SPI1_NSS1 (GPIO or NSS) | AF5 | /FPGA/SPI.CS1 | Chip-select likely GPIO (or SPI NSS depending config) |
| PB7 | 137 | SPI1_NSS2 (GPIO) | AF5 | /FPGA/SPI.CS2 | Chip-select likely GPIO (or SPI NSS depending config) |

## USART/UART (7 pins)
| STM32 pin | Package pin | Signal | AF | Net | Note |
|---|---:|---|---|---|---|
| PB10 | 69 | USART3_TX | AF7 (USART3) | /Microcontroller/USART1.TX |  |
| PB11 | 70 | USART3_RX | AF7 (USART3) | /Microcontroller/USART1.RX |  |
| PB12 | 73 | USART3_CK | AF7 (USART3) | /Microcontroller/USART1.CK |  |
| PB13 | 74 | USART3_CTS | AF7 (USART3) | /Microcontroller/USART1.CTS |  |
| PB14 | 75 | USART3_RTS | AF7 (USART3) | /Microcontroller/USART1.RTS |  |
| PC6 | 96 | USART6_TX | AF8 (USART6) | /ESP32/UART1.TX |  |
| PC7 | 97 | USART6_RX | AF8 (USART6) | /ESP32/UART1.RX |  |

## I2C (3 pins)
| STM32 pin | Package pin | Signal | AF | Net | Note |
|---|---:|---|---|---|---|
| PB8 | 139 | I2C1_SCL | AF4 | /Microcontroller/STM.SCL |  |
| PB9 | 140 | I2C1_SDA | AF4 | /Microcontroller/STM.SDA |  |
| PC5 | 45 | I2C | AF4 | /Microcontroller/STM.I2CRESET | Reset line for I2C devices? (GPIO) |

## GPIO/Other (46 pins)
| STM32 pin | Package pin | Signal | AF | Net | Note |
|---|---:|---|---|---|---|
| PA0 | 34 | JOY2.POT4 | None | /AtariInterfaces/JOY2.POT4 | Analog input (ADC) for paddle/pot |
| PA1 | 35 | JOY2.POT5 | None | /AtariInterfaces/JOY2.POT5 | Analog input (ADC) for paddle/pot |
| PA13 | 105 | Net-(J9-Pin_2) | None | Net-(J9-Pin_2) |  |
| PA14 | 109 | Net-(J9-Pin_3) | None | Net-(J9-Pin_3) |  |
| PA2 | 36 | STMAUD.ADC.L | None | /Audio/STMAUD.ADC.L |  |
| PA3 | 37 | STMAUD.ADC.R | None | /Audio/STMAUD.ADC.R |  |
| PA4 | 40 | JOY2.POT6 | None | /AtariInterfaces/JOY2.POT6 | Analog input (ADC) for paddle/pot |
| PA5 | 41 | JOY2.POT7 | None | /AtariInterfaces/JOY2.POT7 | Analog input (ADC) for paddle/pot |
| PA6 | 42 | STMAUD.ADC.PBI | None | /Audio/STMAUD.ADC.PBI |  |
| PA7 | 43 | STMAUD.ADC.SIO | None | /Audio/STMAUD.ADC.SIO |  |
| PB2 | 48 | BOOT1 | None | /Microcontroller/BOOT1 |  |
| PB3 | 133 | Net-(U19-PB3) | None | Net-(U19-PB3) |  |
| PC0 | 26 | JOY.POT0 | None | /AtariInterfaces/JOY.POT0 | Analog input (ADC) for paddle/pot |
| PC1 | 27 | JOY.POT1 | None | /AtariInterfaces/JOY.POT1 | Analog input (ADC) for paddle/pot |
| PC10 | 111 | Net-(RN10-R2.2) | None | Net-(RN10-R2.2) |  |
| PC11 | 112 | Net-(RN10-R1.2) | None | Net-(RN10-R1.2) |  |
| PC12 | 113 | Net-(RN7-R4.2) | None | Net-(RN7-R4.2) |  |
| PC13 | 7 | Net-(RN7-R3.2) | None | Net-(RN7-R3.2) |  |
| PC14 | 8 | Net-(U19-PC14) | None | Net-(U19-PC14) |  |
| PC15 | 9 | Net-(U19-PC15) | None | Net-(U19-PC15) |  |
| PC2 | 28 | JOY.POT2 | None | /AtariInterfaces/JOY.POT2 | Analog input (ADC) for paddle/pot |
| PC3 | 29 | JOY.POT3 | None | /AtariInterfaces/JOY.POT3 | Analog input (ADC) for paddle/pot |
| PC8 | 98 | Net-(RN10-R4.2) | None | Net-(RN10-R4.2) |  |
| PC9 | 99 | Net-(RN10-R3.2) | None | Net-(RN10-R3.2) |  |
| PD2 | 116 | Net-(RN7-R1.2) | None | Net-(RN7-R1.2) |  |
| PD3 | 117 | Net-(RN7-R2.2) | None | Net-(RN7-R2.2) |  |
| PF10 | 22 | Net-(J10-Pin_3) | None | Net-(J10-Pin_3) |  |
| PF6 | 18 | Net-(J10-Pin_7) | None | Net-(J10-Pin_7) |  |
| PF7 | 19 | Net-(J10-Pin_6) | None | Net-(J10-Pin_6) |  |
| PF8 | 20 | Net-(J10-Pin_5) | None | Net-(J10-Pin_5) |  |
| PF9 | 21 | Net-(J10-Pin_4) | None | Net-(J10-Pin_4) |  |
| PG11 | 126 | FPGA.PS_N | None | /FPGA/FPGA.PS_N |  |
| PG12 | 127 | FPGA.IRQ | None | /FPGA/FPGA.IRQ |  |
| PG13 | 128 | FPGA.CONFIG_N | None | /FPGA/FPGA.CONFIG_N |  |
| PG14 | 129 | FPGA.STATUS_N | None | /FPGA/FPGA.STATUS_N |  |
| PG15 | 132 | FPGA.CONF_DONE | None | /FPGA/FPGA.CONF_DONE |  |
| PG6 | 91 | ESP.BOOT | None | /ESP32/ESP.BOOT |  |
| PG7 | 92 | ESP.EN | None | /ESP32/ESP.EN |  |
| PG8 | 93 | ESP.SGPI0 | None | /ESP32/ESP.SGPI0 |  |
| PG9 | 124 | ESP.SGPI1 | None | /ESP32/ESP.SGPI1 |  |
| PG10 | 125 | ESP.SGPI2 | None | /ESP32/ESP.SGPI2 |  |
| PA9 | 101 | ESP.SGPIO | None | /ESP32/ESP.SGPIO |  |
| PH0 | 23 | Net-(U19-PH0) | None | Net-(U19-PH0) |  |
| PH1 | 24 | Net-(U19-PH1) | None | Net-(U19-PH1) |  |
| nan | 25 | NRST | None | /Microcontroller/NRST |  |
| nan | 138 | Net-(JP1-C) | None | Net-(JP1-C) |  |
