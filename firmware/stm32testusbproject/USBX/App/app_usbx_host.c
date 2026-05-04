/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_usbx_host.c
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
#include "ux_host_class_hub.h"
#include "ux_host_class_hid.h"
#include "ux_host_class_hid_keyboard.h"
#include "ux_host_class_hid_mouse.h"
//#include "ux_dcd_stm32.h"            /* not needed for host, ignore if missing */
#include "ux_hcd_stm32.h"
#include "stm32f4xx_hal.h"
#include "logger.h"
extern HCD_HandleTypeDef hhcd_USB_OTG_FS;   /* declared in usbh_conf or main */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define USBX_HOST_MEMORY_STACK_SIZE   (48 * 1024)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
TX_SEMAPHORE              hid_keyboard_semaphore;
UX_HOST_CLASS_HID_KEYBOARD *current_keyboard;
volatile UCHAR             keyboard_attached;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static UINT usb_host_change_cb(ULONG event, UX_HOST_CLASS *host_class, VOID *instance);
void  hid_keyboard_thread_entry(ULONG arg);

/* USER CODE END PFP */
/**
  * @brief  Application USBX Host Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT MX_USBX_Host_Init(VOID *memory_ptr)
{
  UINT ret = UX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;

  /* USER CODE BEGIN MX_USBX_Host_MEM_POOL */
    (void)byte_pool;
  /* USER CODE END MX_USBX_Host_MEM_POOL */

  /* USER CODE BEGIN MX_USBX_Host_Init */
  {

        VOID *stack_pool;

    /* Allocate the USBX memory pool out of the byte pool. */
    if (tx_byte_allocate(byte_pool, &stack_pool,
                         USBX_HOST_MEMORY_STACK_SIZE,
                         TX_NO_WAIT) != TX_SUCCESS) {
        return TX_POOL_ERROR;
    }

    /* Initialise USBX system memory. F407 has no D-cache so cache pool is 0. */
    if (ux_system_initialize(stack_pool, USBX_HOST_MEMORY_STACK_SIZE,
                             UX_NULL, 0) != UX_SUCCESS) {
        return UX_ERROR;
    }

    /* Initialise the USBX host stack with the change callback. */
    if (ux_host_stack_initialize(usb_host_change_cb) != UX_SUCCESS) {
        return UX_ERROR;
    }

    /* Register HID class. */
    if (ux_host_stack_class_register(_ux_system_host_class_hub_name,
                                     ux_host_class_hub_entry) != UX_SUCCESS) {
        return UX_ERROR;
    }

    /* Register HID class. */
    if (ux_host_stack_class_register(_ux_system_host_class_hid_name,
                                     ux_host_class_hid_entry) != UX_SUCCESS) {
        return UX_ERROR;
    }

    /* Register HID keyboard and mouse clients. */
    if (ux_host_class_hid_client_register(_ux_system_host_class_hid_client_keyboard_name,
                                          ux_host_class_hid_keyboard_entry) != UX_SUCCESS) {
        return UX_ERROR;
    }
    if (ux_host_class_hid_client_register(_ux_system_host_class_hid_client_mouse_name,
                                          ux_host_class_hid_mouse_entry) != UX_SUCCESS) {
        return UX_ERROR;
    }

    if (tx_semaphore_create(&hid_keyboard_semaphore, "hid_kb_sem", 0) != TX_SUCCESS) {
        return UX_ERROR;
    }

    /* Register the STM32 OTG host controller driver. */
    UINT s = ux_host_stack_hcd_register(_ux_system_host_hcd_stm32_name,
                                   _ux_hcd_stm32_initialize,
				   USB_OTG_FS_PERIPH_BASE,
                                   (ULONG)&hhcd_USB_OTG_FS);
    if (s != UX_SUCCESS) return UX_ERROR;

    {
        static TX_THREAD hid_keyboard_thread;
        VOID *kb_stack;
        if (tx_byte_allocate(byte_pool, &kb_stack, 1024, TX_NO_WAIT) != TX_SUCCESS)
            return UX_ERROR;
        if (tx_thread_create(&hid_keyboard_thread, "hid_kb",
                             hid_keyboard_thread_entry, 0,
                             kb_stack, 1024,
                             20, 20, TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
            return UX_ERROR;
    }

    /* Bring up the OTG FS hardware via HAL. */
    HAL_HCD_Start(&hhcd_USB_OTG_FS);
  }

  /* USER CODE END MX_USBX_Host_Init */

  return ret;
}

/* USER CODE BEGIN 1 */
static UINT usb_host_change_cb(ULONG event, UX_HOST_CLASS *host_class, VOID *instance)
{
    (void)host_class;
    (void)instance;

    // Log
    if (event == UX_DEVICE_INSERTION) {
        log_printf("USB: insertion\r\n");
    } else if (event == UX_DEVICE_REMOVAL) {
        log_printf("USB: removal\r\n");
    } else {
        log_printf("USB: event %lu\r\n", event);
    }

    log_printf("free=%lu \r\n",
           _ux_system->ux_system_regular_memory_pool_free);

    // Actual handling
    if (event == UX_HID_CLIENT_INSERTION) {
        UX_HOST_CLASS_HID_CLIENT *client = (UX_HOST_CLASS_HID_CLIENT *)instance;
      //  if (strcmp((char*)client->ux_host_class_hid_client_name,
      //             _ux_system_host_class_hid_client_keyboard_name) == 0) {
        {
            current_keyboard = (UX_HOST_CLASS_HID_KEYBOARD *)
                               client->ux_host_class_hid_client_local_instance;
            keyboard_attached = UX_TRUE;
            tx_semaphore_put(&hid_keyboard_semaphore);
        }
    }
    else if (event == UX_HID_CLIENT_REMOVAL) {
        UX_HOST_CLASS_HID_CLIENT *client = (UX_HOST_CLASS_HID_CLIENT *)instance;
       // if (strcmp((char*)client->ux_host_class_hid_client_name,
       //            _ux_system_host_class_hid_client_keyboard_name) == 0) {
	{
            keyboard_attached = UX_FALSE;
            current_keyboard = UX_NULL;
        }
    }

    return UX_SUCCESS;
}

/* USER CODE END 1 */
