/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define SD_DETECT_Pin GPIO_PIN_13
#define SD_DETECT_GPIO_Port GPIOC
#define GPIO1_Pin GPIO_PIN_6
#define GPIO1_GPIO_Port GPIOF
#define GPIO2_Pin GPIO_PIN_7
#define GPIO2_GPIO_Port GPIOF
#define GPIO3_Pin GPIO_PIN_8
#define GPIO3_GPIO_Port GPIOF
#define GPIO4_Pin GPIO_PIN_9
#define GPIO4_GPIO_Port GPIOF
#define GPIO5_Pin GPIO_PIN_10
#define GPIO5_GPIO_Port GPIOF
#define STM_I2CRESET_Pin GPIO_PIN_5
#define STM_I2CRESET_GPIO_Port GPIOC
#define USART_CK_Pin GPIO_PIN_12
#define USART_CK_GPIO_Port GPIOB
#define USART_CTS_Pin GPIO_PIN_13
#define USART_CTS_GPIO_Port GPIOB
#define USART_RTS_Pin GPIO_PIN_14
#define USART_RTS_GPIO_Port GPIOB
#define ESP_BOOT_Pin GPIO_PIN_6
#define ESP_BOOT_GPIO_Port GPIOG
#define ESP_EN_Pin GPIO_PIN_7
#define ESP_EN_GPIO_Port GPIOG
#define SD_WP_Pin GPIO_PIN_3
#define SD_WP_GPIO_Port GPIOD
#define FPGA_PS_N_Pin GPIO_PIN_11
#define FPGA_PS_N_GPIO_Port GPIOG
#define FPGA_IRQ_Pin GPIO_PIN_12
#define FPGA_IRQ_GPIO_Port GPIOG
#define FPGA_CONFIG_N_Pin GPIO_PIN_13
#define FPGA_CONFIG_N_GPIO_Port GPIOG
#define FPGA_STATUS_N_Pin GPIO_PIN_14
#define FPGA_STATUS_N_GPIO_Port GPIOG
#define FPGA_CONF_DONE_Pin GPIO_PIN_15
#define FPGA_CONF_DONE_GPIO_Port GPIOG
#define SPI_CS1_Pin GPIO_PIN_6
#define SPI_CS1_GPIO_Port GPIOB
#define SPI_CS2_Pin GPIO_PIN_7
#define SPI_CS2_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
