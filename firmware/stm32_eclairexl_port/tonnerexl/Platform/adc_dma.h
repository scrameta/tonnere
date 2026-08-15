/*
 * adc_dma.h — paddle + audio ADC → DMA2 → FSMC/FPGA streams (STM32F407).
 *
 * Two independent circular ADC→DMA streams write raw 12-bit samples straight
 * into the FPGA register file over FSMC. No CPU touches the samples in the
 * normal path; the FPGA does the paddle threshold compare and latches audio.
 * See docs/stm32f407_paddle_adc_dma_fsmc.md and docs/fpga_interface.md §3.
 *
 *   Paddle:  ADC2, 8-rank continuous scan, DMA very-high, dest PADDLE_ADC0..7.
 *   Audio:   ADC1, 4-rank timer-triggered scan (~44.1 kHz), DMA high,
 *            dest AUDIO_ADC0..3, CONT=0.
 *
 * This module owns the ADC-start / DMA-start sequencing and the POTGO/
 * POT_RESET GPIO clamp. CubeMX still generates the peripheral init (handles,
 * MSP, NVIC); see docs for the exact CubeMX settings. The functions below are
 * called from USER CODE blocks after MX_*_Init().
 *
 * On-target only: the whole module is compiled out unless FPGA_BUS_STM32.
 */
#ifndef TONNEREXL_ADC_DMA_H
#define TONNEREXL_ADC_DMA_H

#include <stdint.h>
#include "fpga_bus.h"

/* The prototypes below reference ADC_HandleTypeDef; pull in the HAL on target.
 * (fpga_bus_map.h already assumes HAL GPIO symbols under FPGA_BUS_STM32.) */
#if defined(FPGA_BUS_STM32)
#include "stm32f4xx_hal.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Start the paddle stream: ADC2 8-rank continuous scan, circular DMA to
 * PADDLE_ADC0..7. Idempotent-ish: call once after MX_ADC2_Init and the DMA
 * MSP link. Returns FPGA_OK or an error if HAL start fails. */
fpga_status_t adc_dma_paddle_start(void);

/* Start the audio stream: ADC1 4-rank timer-triggered scan, circular DMA to
 * AUDIO_ADC0..3. The timer (TIM2 TRGO by default) must already be configured
 * and started to produce the 44.1 kHz trigger. */
fpga_status_t adc_dma_audio_start(void);

/* Stop each stream (DMA + ADC). Mainly for bring-up / fault recovery. */
void adc_dma_paddle_stop(void);
void adc_dma_audio_stop(void);

/* ---- POTGO / POT_RESET paddle-pin clamp (called from the FPGA IRQ ISR) ----
 * The FPGA IRQ handler demuxes IRQ_PENDING and calls these directly — NOT via
 * a ThreadX thread — to keep the GPIO response deterministic (design doc §ThreadX).
 *   POTGO rising (IRQ bit 3):  clamp — preload the 8 paddle GPIO latches low,
 *                              then switch analogue->push-pull output-low.
 *   POT_RESET falling (bit 5): release — switch the pins back to analogue mode.
 * The ADC/DMA keep running throughout; sampling resumes as soon as the pins
 * return to analogue. Preloading low BEFORE enabling output avoids a transient
 * high. The paddle pins span GPIOA and GPIOC on this board (errata), so each
 * op touches two ports. */
void adc_dma_paddle_pins_clamp(void);
void adc_dma_paddle_pins_release(void);

/* Fault hook. Called from the DMA/ADC error ISR path when an ADC overrun (OVR)
 * or DMA transfer/FIFO error is seen. In DMA mode an ADC overrun latches the
 * ADC and it stops issuing DMA requests, so this is a hard fault, not a dropped
 * sample (design doc §"ADC overrun is a real fault"). Default impl asserts;
 * override for your bring-up policy. */
void adc_dma_on_fault(ADC_HandleTypeDef *hadc, uint32_t err);

#ifdef __cplusplus
}
#endif
#endif /* TONNEREXL_ADC_DMA_H */
