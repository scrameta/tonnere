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
#include "usb_keyboard.h"             /* portable HID->Atari mapper (tonnerexl/App) */
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

        usb_keyboard_reset();   /* clear any stale held keys on attach */

        /* Tell the USBX keyboard class NOT to translate keys to ASCII — we want
         * RAW HID usage codes (the class otherwise applies its layout table and
         * key_get returns 'q'/113 instead of usage 0x14). With decode disabled,
         * key_get returns key_value (the raw HID usage), which is what the
         * spatial HID->KBCODE table expects. */
        ux_host_class_hid_keyboard_ioctl(current_keyboard,
                                         UX_HID_KEYBOARD_IOCTL_DISABLE_KEYS_DECODE,
                                         UX_NULL);

        /* Poll while the keyboard is present. Requires USBX built with
         * UX_HOST_CLASS_HID_KEYBOARD_EVENTS_KEY_CHANGES_MODE (+ _REPORT_MODIFIER_KEYS)
         * in ux_user.h, so key_get reports press/release changes (not a cooked
         * full report). Combined with decode-disable above, keycode is the raw
         * HID usage. */
        while (keyboard_attached && current_keyboard != UX_NULL) {
            UINT s = ux_host_class_hid_keyboard_key_get(current_keyboard,
                                                       &keycode, &state);
            if (s == UX_SUCCESS) {
                int pressed = (state & UX_HID_KEYBOARD_STATE_KEY_UP) ? 0 : 1;
                usb_keyboard_key_event((uint8_t)keycode, pressed);
            } else {
                /* No change yet; sleep briefly so we don't busy-loop. */
                tx_thread_sleep(1);
            }
        }
	usb_keyboard_reset();   /* keyboard gone — release everything */
	log_printf("KB thread:Keyboard removed\r\n");
        /* Keyboard removed — loop back and wait for the next one. */
    }
}

/* USER CODE END 1 */
