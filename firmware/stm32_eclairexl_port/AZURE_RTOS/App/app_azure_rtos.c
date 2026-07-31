
/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_azure_rtos.c
  * @author  MCD Application Team
  * @brief   azure_rtos application implementation file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/

#include "app_azure_rtos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "main.h"
#include "logger.h"

#include "si5351_modes.h"
#include "i2c_switch.h"

#include "app_main.h"
#include "platform.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN TX_Pool_Buffer */
/* USER CODE END TX_Pool_Buffer */
static UCHAR tx_byte_pool_buffer[TX_APP_MEM_POOL_SIZE];
static TX_BYTE_POOL tx_app_byte_pool;

/* USER CODE BEGIN FX_Pool_Buffer */
/* USER CODE END FX_Pool_Buffer */
static UCHAR  fx_byte_pool_buffer[FX_APP_MEM_POOL_SIZE];
static TX_BYTE_POOL fx_app_byte_pool;

/* USER CODE BEGIN UX_HOST_Pool_Buffer */
/* USER CODE END UX_HOST_Pool_Buffer */
static UCHAR  ux_host_byte_pool_buffer[UX_HOST_APP_MEM_POOL_SIZE];
static TX_BYTE_POOL ux_host_app_byte_pool;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/**
  * @brief  Define the initial system.
  * @param  first_unused_memory : Pointer to the first unused memory
  * @retval None
  */
VOID tx_application_define(VOID *first_unused_memory)
{
  /* USER CODE BEGIN  tx_application_define */

  /* USER CODE END  tx_application_define */

  VOID *memory_ptr;

  if (tx_byte_pool_create(&tx_app_byte_pool, "Tx App memory pool", tx_byte_pool_buffer, TX_APP_MEM_POOL_SIZE) != TX_SUCCESS)
  {
    /* USER CODE BEGIN TX_Byte_Pool_Error */

    /* USER CODE END TX_Byte_Pool_Error */
  }
  else
  {
    /* USER CODE BEGIN TX_Byte_Pool_Success */

    /* USER CODE END TX_Byte_Pool_Success */

    memory_ptr = (VOID *)&tx_app_byte_pool;

    if (App_ThreadX_Init(memory_ptr) != TX_SUCCESS)
    {
      /* USER CODE BEGIN  App_ThreadX_Init_Error */

      /* USER CODE END  App_ThreadX_Init_Error */
    }

    /* USER CODE BEGIN  App_ThreadX_Init_Success */

    /* USER CODE END  App_ThreadX_Init_Success */

  }

  if (tx_byte_pool_create(&fx_app_byte_pool, "Fx App memory pool", fx_byte_pool_buffer, FX_APP_MEM_POOL_SIZE) != TX_SUCCESS)
  {
    /* USER CODE BEGIN FX_Byte_Pool_Error */

    /* USER CODE END FX_Byte_Pool_Error */
  }
  else
  {
    /* USER CODE BEGIN FX_Byte_Pool_Success */

    /* USER CODE END FX_Byte_Pool_Success */

    memory_ptr = (VOID *)&fx_app_byte_pool;

    if (MX_FileX_Init(memory_ptr) != FX_SUCCESS)
    {
      /* USER CODE BEGIN MX_FileX_Init_Error */

      /* USER CODE END MX_FileX_Init_Error */
    }

    /* USER CODE BEGIN MX_FileX_Init_Success */

    /* USER CODE END MX_FileX_Init_Success */
  }

  if (tx_byte_pool_create(&ux_host_app_byte_pool, "Ux App memory pool", ux_host_byte_pool_buffer, UX_HOST_APP_MEM_POOL_SIZE) != TX_SUCCESS)
  {
    /* USER CODE BEGIN TX_Byte_Pool_Error */

    /* USER CODE END TX_Byte_Pool_Error */
  }
  else
  {
    /* USER CODE BEGIN UX_HOST_Byte_Pool_Success */

    logger_init();

    /* USER CODE END UX_HOST_Byte_Pool_Success */

    memory_ptr = (VOID *)&ux_host_app_byte_pool;

    if (MX_USBX_Host_Init(memory_ptr) != UX_SUCCESS)
    {
      /* USER CODE BEGIN MX_USBX_Host_Init_Error */

      /* USER CODE END MX_USBX_Host_Init_Error */
    }

    /* USER CODE BEGIN MX_USBX_Host_Init_Success */
    log_printf("Init SI5351\r\n");
    HAL_GPIO_WritePin(STM_I2CRESET_GPIO_Port, STM_I2CRESET_Pin, GPIO_PIN_SET);
    pca9546_select(2);
    si5351_init();                          // once after power-on
    si5351_apply_mode(SI5351_MODE_720_PAL); // call whenever mode changes
    pca9546_select(3);
    si5351_init();                          // once after power-on
    si5351_apply_mode(SI5351_MODE_720_PAL); // call whenever mode changes
    log_printf("Init SI5351 DONE\r\n");

    /* --- TonnereXL port entry --- */
    {
        app_config_t port_cfg;
        port_cfg.thread_pool = &tx_app_byte_pool;
        if (app_main(&port_cfg) != TX_SUCCESS) {
            log_printf("app_main failed\r\n");
        }
    }

    /* USER CODE END MX_USBX_Host_Init_Success */
  }

}

/* USER CODE BEGIN  0 */
extern I2C_HandleTypeDef hi2c1;
void si5351_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    HAL_I2C_Master_Transmit(&hi2c1, 0xc0, &buf[0], 2, HAL_MAX_DELAY);
}
extern void pca9546_write(uint8_t addr, uint8_t val)
{
    HAL_I2C_Master_Transmit(&hi2c1, addr, &val, 1, HAL_MAX_DELAY);
}

/* Map the port's abstract video-clock request to your si5351 code. */
void board_set_video_clock(int mode)   /* platform_clock_mode_t */
{
    switch(mode)
    {
    case PLAT_CLK_NTSC:
        si5351_apply_mode(SI5351_MODE_480_NTSC);
        break;
    case PLAT_CLK_PAL:
        si5351_apply_mode(SI5351_MODE_576_PAL);
        break;
    case PLAT_CLK_480P: // TODO, these are not match
        break;
    case PLAT_CLK_720P:
        break;
    default:	 
        break;
    }
//    SI5351_MODE_576_PAL           , /* 576p/i PAL@50 */
//    SI5351_MODE_480_NTSC          , /* 480p/i NTSC@59.94 */
//    SI5351_MODE_720_PAL           , /* 720p/1080i PAL@50 */
//    SI5351_MODE_720_PAL_HI        , /* 720p/1080i PAL@50 Hi */
//    SI5351_MODE_720_NTSC          , /* 720p/1080i NTSC@60 */
//    SI5351_MODE_720_NTSC_HI         /* 720p/1080i NTSC@60 Hi */
    /* e.g. switch(mode){ case PLAT_CLK_PAL: ... si5351_apply_mode(...) ... } */
}

/* Emit one log byte to your console transport (UART/ITM). */
extern UART_HandleTypeDef huart3;   // or whichever CubeMX generated
void board_log_putc(char c)
{
    HAL_UART_Transmit(&huart3,(uint8_t*)&c,1,1); 
}

/* USER CODE END  0 */
