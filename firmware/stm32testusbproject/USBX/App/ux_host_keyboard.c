/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ux_host_keyboard.c
  * @author  MCD Application Team
  * @brief   USBX host applicative file
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
#include "app_usbx_host.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "logger.h"
#include "ux_host_class_hub.h"
#include "ux_host_class_hid.h"
#include "ux_host_class_hid_keyboard.h"
#include "ux_host_class_hid_mouse.h"
//#include "ux_dcd_stm32.h"            /* not needed for host, ignore if missing */
#include "ux_hcd_stm32.h"
#include "stm32f4xx_hal.h"
extern TX_SEMAPHORE              hid_keyboard_semaphore;
extern UX_HOST_CLASS_HID_KEYBOARD *current_keyboard;
extern volatile UCHAR             keyboard_attached;
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
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
void  hid_keyboard_thread_entry(ULONG arg);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* USER CODE BEGIN 1 */

/**
  * @brief  hid_keyboard_thread_entry .
  * @param  ULONG arg
  * @retval Void
  */
void  hid_keyboard_thread_entry(ULONG arg)
{
    (void)arg;
    ULONG keycode, state;

    while (1) {
        /* Wait for a keyboard to attach. */
	log_printf("KB thread:Waiting for semaphore\r\n");
        tx_semaphore_get(&hid_keyboard_semaphore, TX_WAIT_FOREVER);
	log_printf("KB thread:Got semaphore\r\n");

        /* Poll while the keyboard is present. */
        while (keyboard_attached && current_keyboard != UX_NULL) {
            UINT s = ux_host_class_hid_keyboard_key_get(current_keyboard,
                                                       &keycode, &state);
            if (s == UX_SUCCESS) {
                log_printf("KEY 0x%02lX state=%lu\r\n", keycode, state);
            } else {
                /* No key yet; sleep briefly so we don't busy-loop. */
                tx_thread_sleep(1);
            }
        }
	log_printf("KB thread:Keyboard removed\r\n");
        /* Keyboard removed — loop back and wait for the next one. */
    }
}

/* USER CODE END 1 */
