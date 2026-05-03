cmake_minimum_required(VERSION 3.22)
# Enable CMake support for ASM and C languages
enable_language(C ASM)
# STM32CubeMX generated symbols (macros)
set(MX_Defines_Syms 
	TX_INCLUDE_USER_DEFINE_FILE 
	UX_INCLUDE_USER_DEFINE_FILE 
	USE_HAL_DRIVER 
	STM32F407xx
    $<$<CONFIG:Debug>:DEBUG>
)

# STM32CubeMX generated include paths
set(MX_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../AZURE_RTOS/App
    ${CMAKE_CURRENT_SOURCE_DIR}/../../USBX/App
    ${CMAKE_CURRENT_SOURCE_DIR}/../../USBX/Target
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/CMSIS/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/inc/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/inc/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/ports/generic/inc/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/inc/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/ports/cortex_m4/gnu/inc/
)

# STM32CubeMX generated application sources
set(MX_Application_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../AZURE_RTOS/App/app_azure_rtos.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../USBX/App/ux_host_keyboard.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../USBX/App/ux_host_mouse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../USBX/App/app_usbx_host.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/tx_initialize_low_level.s
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/app_threadx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/stm32f4xx_it.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/stm32f4xx_hal_msp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/stm32f4xx_hal_timebase_tim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/sysmem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/syscalls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../startup_stm32f407xx.s
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/tx_initialize_low_level.s
)

# STM32 HAL/LL Drivers
set(STM32_Drivers_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/system_stm32f4xx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_fsmc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_nor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_sram.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_nand.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pccard.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rtc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rtc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_sdmmc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_sd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_mmc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_hcd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_usb.c
)

# Drivers Midllewares


set(USBX_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_callback.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_controller_disable.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_ed_obtain.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_endpoint_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_endpoint_destroy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_endpoint_reset.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_entry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_frame_number_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_interrupt_handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_least_traffic_list_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_periodic_schedule.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_port_disable.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_port_enable.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_port_reset.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_port_resume.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_port_status_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_port_suspend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_power_down_port.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_power_on_port.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_request_bulk_transfer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_request_control_transfer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_request_transfer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_transfer_abort.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_stm32_host_controllers/ux_hcd_stm32_request_periodic_transfer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_keyboard_activate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_keyboard_callback.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_keyboard_deactivate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_keyboard_entry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_keyboard_ioctl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_keyboard_key_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_keyboard_thread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_debug_callback_register.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_debug_log.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_delay_ms.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_descriptor_pack.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_descriptor_parse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_error_callback_register.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_event_flags_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_event_flags_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_event_flags_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_event_flags_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_long_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_long_get_big_endian.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_long_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_long_put_big_endian.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_memory_allocate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_memory_allocate_add_safe.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_memory_allocate_mulc_safe.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_memory_allocate_mulv_safe.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_memory_compare.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_memory_copy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_memory_free.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_memory_free_block_best_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_memory_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_mutex_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_mutex_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_mutex_off.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_mutex_on.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_pci_class_scan.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_pci_read.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_pci_write.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_physical_address.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_semaphore_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_semaphore_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_semaphore_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_semaphore_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_set_interrupt_handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_short_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_short_get_big_endian.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_short_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_short_put_big_endian.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_string_length_check.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_string_length_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_string_to_unicode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_thread_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_thread_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_thread_identify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_thread_relinquish.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_thread_resume.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_thread_schedule_other.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_thread_sleep.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_thread_suspend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_timer_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_unicode_to_string.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_utility_virtual_address.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_system_error_handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_system_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_system_uninitialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_activate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_client_register.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_client_search.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_configure.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_deactivate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_descriptor_parse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_entry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_field_decompress.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_global_item_parse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_idle_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_idle_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_instance_clean.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_interrupt_endpoint_search.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_item_data_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_local_item_parse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_main_item_parse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_periodic_report_start.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_periodic_report_stop.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_report_add.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_report_callback_register.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_report_compress.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_report_decompress.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_report_descriptor_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_report_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_report_id_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_report_item_analyse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_report_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_resources_free.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_transfer_request_completed.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_bandwidth_check.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_bandwidth_claim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_bandwidth_release.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_class_call.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_class_device_scan.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_class_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_class_instance_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_class_instance_destroy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_class_instance_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_class_instance_verify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_class_interface_scan.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_class_register.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_class_unregister.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_configuration_descriptor_parse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_configuration_enumerate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_configuration_instance_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_configuration_instance_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_configuration_interface_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_configuration_interface_scan.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_configuration_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_delay_ms.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_device_address_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_device_configuration_activate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_device_configuration_deactivate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_device_configuration_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_device_configuration_reset.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_device_configuration_select.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_device_descriptor_read.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_device_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_device_remove.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_device_resources_free.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_device_string_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_endpoint_instance_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_endpoint_instance_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_endpoint_reset.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_endpoint_transfer_abort.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_enum_thread_entry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_hcd_register.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_hcd_thread_entry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_hcd_transfer_request.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_hcd_unregister.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_hnp_polling_thread_entry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_interface_endpoint_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_interface_instance_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_interface_instance_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_interface_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_interface_setting_select.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_interfaces_scan.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_new_configuration_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_new_device_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_new_device_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_new_endpoint_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_new_interface_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_rh_change_process.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_rh_device_extraction.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_rh_device_insertion.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_role_swap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_transfer_request.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_transfer_request_abort.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/core/src/ux_host_stack_uninitialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_mouse_activate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_mouse_buttons_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_mouse_callback.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_mouse_deactivate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_mouse_entry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_mouse_position_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hid_mouse_wheel_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hub_activate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hub_change_detect.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hub_change_process.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hub_configure.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hub_deactivate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hub_descriptor_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hub_entry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hub_feature.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hub_hub_change_process.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hub_interrupt_endpoint_start.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hub_port_change_connection_process.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hub_port_change_enable_process.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hub_port_change_over_current_process.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hub_port_change_process.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hub_port_change_reset_process.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hub_port_change_suspend_process.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hub_port_reset.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hub_ports_power.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hub_status_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hub_tasks_run.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/usbx/common/usbx_host_classes/src/ux_host_class_hub_transfer_request_completed.c
)
set(ThreadX_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/ports/cortex_m4/gnu/src/tx_thread_context_restore.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/ports/cortex_m4/gnu/src/tx_thread_context_save.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/ports/cortex_m4/gnu/src/tx_thread_interrupt_control.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/ports/cortex_m4/gnu/src/tx_thread_schedule.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/ports/cortex_m4/gnu/src/tx_thread_stack_build.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/ports/cortex_m4/gnu/src/tx_thread_system_return.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/ports/cortex_m4/gnu/src/tx_timer_interrupt.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_initialize_high_level.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_initialize_kernel_enter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_initialize_kernel_setup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_block_allocate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_block_pool_cleanup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_block_pool_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_block_pool_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_block_pool_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_block_pool_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_block_pool_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_block_release.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_byte_allocate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_byte_pool_cleanup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_byte_pool_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_byte_pool_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_byte_pool_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_byte_pool_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_byte_pool_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_byte_pool_search.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_byte_release.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_event_flags_cleanup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_event_flags_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_event_flags_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_event_flags_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_event_flags_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_event_flags_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_event_flags_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_event_flags_set_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_mutex_cleanup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_mutex_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_mutex_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_mutex_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_mutex_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_mutex_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_mutex_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_mutex_priority_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_mutex_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_queue_cleanup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_queue_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_queue_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_queue_flush.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_queue_front_send.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_queue_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_queue_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_queue_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_queue_receive.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_queue_send.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_queue_send_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_semaphore_ceiling_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_semaphore_cleanup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_semaphore_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_semaphore_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_semaphore_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_semaphore_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_semaphore_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_semaphore_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_semaphore_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_semaphore_put_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_entry_exit_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_identify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_preemption_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_priority_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_relinquish.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_reset.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_resume.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_shell_entry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_sleep.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_stack_analyze.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_stack_error_handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_stack_error_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_suspend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_system_preempt_check.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_system_resume.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_system_suspend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_terminate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_time_slice.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_time_slice_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_timeout.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_thread_wait_abort.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_time_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_time_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_block_allocate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_block_pool_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_block_pool_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_block_pool_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_block_pool_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_block_release.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_byte_allocate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_byte_pool_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_byte_pool_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_byte_pool_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_byte_pool_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_byte_release.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_event_flags_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_event_flags_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_event_flags_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_event_flags_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_event_flags_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_event_flags_set_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_mutex_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_mutex_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_mutex_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_mutex_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_mutex_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_mutex_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_queue_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_queue_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_queue_flush.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_queue_front_send.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_queue_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_queue_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_queue_receive.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_queue_send.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_queue_send_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_semaphore_ceiling_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_semaphore_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_semaphore_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_semaphore_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_semaphore_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_semaphore_prioritize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_semaphore_put.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_semaphore_put_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_thread_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_thread_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_thread_entry_exit_notify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_thread_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_thread_preemption_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_thread_priority_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_thread_relinquish.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_thread_reset.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_thread_resume.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_thread_suspend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_thread_terminate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_thread_time_slice_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_thread_wait_abort.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_timer_activate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_timer_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_timer_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_timer_deactivate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_timer_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_timer_expiration_process.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_timer_info_get.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_timer_initialize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_timer_system_activate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_timer_system_deactivate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/tx_timer_thread_entry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_timer_activate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_timer_change.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_timer_create.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_timer_deactivate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_timer_delete.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/threadx/common/src/txe_timer_info_get.c
)

# Link directories setup
set(MX_LINK_DIRS

)
# Project static libraries
set(MX_LINK_LIBS 
    STM32_Drivers
    ${TOOLCHAIN_LINK_LIBRARIES}
    USBX	ThreadX	
)
# Interface library for includes and symbols
add_library(stm32cubemx INTERFACE)
target_include_directories(stm32cubemx INTERFACE ${MX_Include_Dirs})
target_compile_definitions(stm32cubemx INTERFACE ${MX_Defines_Syms})

# Create STM32_Drivers static library
add_library(STM32_Drivers OBJECT)
target_sources(STM32_Drivers PRIVATE ${STM32_Drivers_Src})
target_link_libraries(STM32_Drivers PUBLIC stm32cubemx)


# Create USBX static library
add_library(USBX OBJECT)
target_sources(USBX PRIVATE ${USBX_Src})
target_link_libraries(USBX PUBLIC stm32cubemx)

# Create ThreadX static library
add_library(ThreadX OBJECT)
target_sources(ThreadX PRIVATE ${ThreadX_Src})
target_link_libraries(ThreadX PUBLIC stm32cubemx)

# Add STM32CubeMX generated application sources to the project
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MX_Application_Src})

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${MX_LINK_DIRS})

# Add libraries to the project
target_link_libraries(${CMAKE_PROJECT_NAME} ${MX_LINK_LIBS})

# Add the map file to the list of files to be removed with 'clean' target
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_PROJECT_NAME}.map)

# Validate that STM32CubeMX code is compatible with C standard
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()
