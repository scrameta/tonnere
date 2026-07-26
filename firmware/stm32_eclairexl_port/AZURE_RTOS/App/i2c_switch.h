#pragma once

#define PCA9546_ADDR  0xE0u   // A2=A1=A0=0

/* Implement this for your platform (STM32 HAL, ESP-IDF i2c_master, etc.) */
extern void pca9546_write(uint8_t addr, uint8_t val);

static void pca9546_select(uint8_t channel)
{
    uint8_t val = (1u << channel);
    pca9546_write(PCA9546_ADDR,val);
}

static void pca9546_select_none(void)
{
    uint8_t val = 0x00u;
    pca9546_write(PCA9546_ADDR,val);
}
