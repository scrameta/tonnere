# Board status
## 0.51

| Component         | Status | Comment |
| ---               | ---    | --- |
| Audio DAC         |  🟢    | Played a multi-frequency sine wave fine |
| ESP32             |  🟢    | Ran simple pin toggler, also checked it appears on JTAG | 
| ESP32 audio DAC   | ⚪     | |
| ESP32 WIFI        | ⚪     | |
| FPGA              |  🟢    | Working fine. Caution: Do not use clock input pins as outputs!  | 
| FPGA FLASH        |  🟢    | Working fine. Only tested one IC.  | 
| HDMI port         |  🟢    | Tested DVI video output  | 
| JOYSTICK DIR      |  🟢    | Tested toggling pins |
| JOYSTICK PADDLES  | ⚪     |  |
| PLL1              |  🟢    | Programmed via I2C and checked they produce clocks |
| PLL2              |  🟢    | Programmed via I2C and checked they produce clocks |
| SDCARD(EXT)       |  🟢    | Detect insertion/removal. Lists directory on insertion. 4-bit mode. |
| SDCARD(INT)       | ⚪     |  | 
| SDRAM             |  🟢    | Boots to ready | 
| SRAM1             |  🟡    | Used input pins on FPGA, had to correct with a few wires  | 
| SRAM2             |  🟡    | Used input pins on FPGA, had to correct with a few wires  | 
| STM32 ADC         | ⚪     |  | 
| STM32 CORE        |  🟢    | Code running fine. SWD works. USART debug works. | 
| STM32 DFU         |  🟡    | DFU slow to start due to PB5 connected to FPGA flash chip during boot  | 
| STM32 RTC         |  ⚪    |   | 
| STM32 FSMC        |  🟢    | Full bridge to control registers and DMA port implemented | 
| PBI               |  🟢    | Ran turbo freezer ok | 
| SIO               |  🟡    | Motor control did not go via level shifter. Modified that and wired up, now boots | 
| USB               |  🟢    | Tested keyboard in all ports. STM needed patches hub (class + reconnect fix) |
| Video DAC         |  🟡    | Single pin was too aggressive for sigma delta (very low oversampling). Too noisy. Added 2nd pin and used fixed patterns. Achieved 6-bit |

## 0.60

Not made yet...


## Key
| Symbol | Status |
| ---    | ---    |
| ⚪     | Not checked yet |
| 🔴     | Failed |
| 🟡     | Works with workaround |
| 🟢     | Passed |

Passed does not mean thoroughly checked...


<!--
| Feature A | ⚪ | Not checked yet |
| Feature C | 🟡 | Works with workaround |
| Feature D | 🟢 | Passed |
-->
