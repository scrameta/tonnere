/*
 * platform_stm32.c — board implementation of the platform seam.
 *
 * Built only on-target (-DFPGA_BUS_STM32). Owns the mapping from the port's
 * abstract (standard, refresh) video pairs to the board's si5351 modes, and the
 * table of which pairs are valid. The port never sees SI5351_MODE_* enums.
 *
 * TODAY: the valid-pair table and the si5351 mode ids are hardcoded here.
 * LATER: platform_video_supported() can instead consult monitor EDID/HDMI caps
 * read over I2C — same signature, port unchanged.
 */
#include "platform.h"
#include "app_threads.h"

#if defined(FPGA_BUS_STM32)
#include "stm32f4xx_hal.h"
#include "main.h"   /* FPGA_IRQ_Pin */

int platform_init(void) {
    /* si5351 powered up + initial mode set in tx_application_define. */
    return 0;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin != FPGA_IRQ_Pin) return;  /* No real need to check pin, we only have one. */

    pot_reset_service_step();
}

#endif
