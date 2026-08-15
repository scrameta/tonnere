# STM32F407 Paddle ADC → DMA → FSMC/FPGA Design

## 1. Why this design

Use the STM32 ADC only as a fast 8-channel sampler and stream every conversion directly to the FPGA:

```text
8 ADC channels
      ↓
ADC2 regular scan, continuous
      ↓
DMA2, circular
      ↓
8 consecutive 16-bit FSMC addresses
      ↓
FPGA threshold compare + sticky 8-bit hit register
      ↓
FPGA IRQ → STM32 GPIO action
```

This is preferable to the STM32 analog watchdog because the F407 watchdog only says that *some* watched channel crossed the threshold; it does not latch which channel caused it.

DMA already has to move every ADC result somewhere, so writing directly to FSMC avoids an intermediate SRAM buffer and removes ADC processing from the CPU. The FPGA gets the channel identity from the FSMC destination address and can compare each value with the common threshold immediately.

The bandwidth is modest: one 16-bit FSMC write per ADC conversion. Even at the F407 single-ADC maximum of 2.4 Msamples/s this is only 4.8 MB/s of payload.

ThreadX should stay out of the fast path. The FPGA IRQ handler should perform the GPIO change directly; a ThreadX event can be posted afterward if software needs to know about it.

---

## 2. Implementation steps

### FPGA

Map eight consecutive 16-bit write registers, for example:

```text
FPGA_ADC_BASE + 0x00   channel/rank 0
FPGA_ADC_BASE + 0x02   channel/rank 1
...
FPGA_ADC_BASE + 0x0E   channel/rank 7
```

On each write:

1. Decode the address to determine the channel.
2. Optionally store the latest 12-bit ADC value.
3. Compare it with the common threshold.
4. Set the corresponding bit in a sticky `adc_hit[7:0]` register.
5. Generate the required FPGA interrupt/event.

Provide a register or command to clear/re-arm the sticky bits.

### ADC2

Configure:

- Regular conversion group.
- Scan mode enabled.
- Exactly 8 ranks, always in the same order.
- Continuous conversion enabled.
- 12-bit resolution unless lower resolution is useful.
- Right-aligned data.
- Shortest sampling time that the external analogue source impedance can reliably drive.
- ADC DMA enabled.
- **DDS / continuous DMA requests enabled** so requests continue after the first 8-value sequence.

The ADC produces one DMA request after each regular-channel conversion.

### DMA2

Configure a valid ADC2 DMA2 stream/channel with:

```text
Direction          peripheral → memory
Peripheral address ADC2->DR
Memory address     FPGA_ADC_BASE
NDTR               8
Peripheral width   16 bit
Memory width       16 bit
Peripheral inc     disabled
Memory inc         enabled
Circular mode      enabled
Priority           very high
Peripheral burst   single
Memory burst       single
Half/full IRQs     disabled
```

After eight transfers, circular mode resets the destination address to `FPGA_ADC_BASE`, so the DMA continually writes ranks 0..7 to the same eight FPGA registers.

Enable/configure the DMA before starting ADC conversions, and clear old DMA status flags before enabling the stream.

### FPGA control IRQ

Keep this ISR short and independent of ThreadX scheduling.

To clamp the eight ADC pins low:

1. Preload their GPIO output latches low using `GPIOx->BSRR`.
2. Change the pins from analogue mode to push-pull output mode.

To release them:

1. Change the pins back to analogue mode.

Preloading zero **before** enabling output mode avoids a transient high output.

If possible, put all eight ADC pins on one GPIO port so the mode change can be one precomputed `MODER` write.

The ADC/DMA can remain running while the pins are clamped; this keeps the implementation simple and allows sampling to resume immediately when the pins return to analogue mode.

---

## 3. Settings and traps not to miss

### DMA FIFO: explicitly choose what you want

The STM32F4 DMA FIFO is 4 × 32 bits = 16 bytes.

At reset, DMA is in **Direct Mode** (`DMDIS=0`). The `FTH` field resets to `01`, which means **1/2 full**, but that threshold is ignored while Direct Mode is enabled.

For this application I would start with:

```text
FIFO mode       enabled
FIFO threshold  1/4 full
```

With 16-bit ADC values, a 1/4-full threshold is one 32-bit FIFO word = **two ADC samples**. This introduces at most roughly one extra sample interval before the first of a pair is written to the FPGA, but provides useful buffering if FSMC is temporarily unavailable.

Do **not** simply enable FIFO and leave its threshold at the reset value: that gives a 1/2-full threshold, i.e. four 16-bit samples before the memory-side transfer is triggered.

If absolute minimum ADC→FPGA latency matters more than stall tolerance, Direct Mode is also reasonable; its memory-side request is generated immediately after each ADC read.

### FSMC contention / NWAIT

DMA2's memory port can access FSMC through the bus matrix. CPU and DMA accesses to the same FSMC slave can therefore contend.

A CPU FSMC transaction already in progress cannot be pre-empted. If an FPGA read holds `NWAIT`, a pending DMA FSMC write waits until that transaction completes.

This should be benign here because:

- FPGA writes do not use `NWAIT`.
- Long waits are expected only on occasional reads.
- DMA FIFO mode can absorb temporary destination-side delays.

Instrument the FPGA's maximum read/NWAIT duration if useful.

### ADC overrun is a real fault

Enable/monitor ADC `OVR`.

In DMA mode, if ADC data overrun occurs, the F407 stops accepting further ADC DMA requests. Recovery requires clearing/reinitialising the ADC/DMA path.

Therefore treat **any ADC overrun as a fault/assertion**, not as a harmless dropped sample. It is a useful test for whether FSMC stalls or other DMA contention are ever too long.

Also consider enabling DMA transfer/FIFO error reporting while leaving normal half-transfer/transfer-complete interrupts disabled.

### ADC sample time

The shortest ADC sample time gives the highest rate, but only use it if the analogue source can charge the ADC sample capacitor quickly enough. A high source impedance may require a longer ADC sample time.

### ThreadX

Do not wake a thread to perform the time-critical GPIO operation.

Preferred sequence:

```text
FPGA IRQ
   ↓
read cause/status
   ↓
direct GPIO register write(s)
   ↓
optional ThreadX event notification
```

This keeps response latency deterministic and independent of thread scheduling.

---

## Recommended initial configuration

```text
ADC2: 8-channel continuous scan
ADC DMA: enabled + DDS enabled
DMA2: peripheral→memory, circular, NDTR=8
Widths: 16-bit / 16-bit
MINC: enabled
DMA priority: very high
FIFO: enabled, explicitly 1/4 threshold
Bursts: single
Normal DMA completion IRQs: disabled
ADC OVR / DMA error detection: enabled
Destination: eight consecutive FPGA FSMC halfword registers
FPGA: common-threshold comparator + sticky 8-bit result
IRQ GPIO handling: direct register access, not a ThreadX worker
```

## References

- ST RM0090 — STM32F405/415, STM32F407/417 reference manual  
  https://www.st.com/resource/en/reference_manual/dm00031020.pdf
- ST AN4031 — Using the STM32F2/F4/F7 DMA controller  
  https://www.st.com/resource/en/application_note/an4031-using-the-stm32f2-stm32f4-and-stm32f7-series-dma-controller-stmicroelectronics.pdf
