# Board integration — wiring the port into your STM32Cube project

The port has a single shared entry point, `app_main()`, that both the host tests
and the board call. On the board you make **three small edits** to files you
already have — no files are replaced. Hardware bring-up (byte pools, USBX,
FileX, si5351) stays exactly where CubeMX puts it.

## Architecture recap

```
                 app_main()   <-- shared, identical on board + host, TESTED
                    |
   +----------------+-----------------+
   | fpga_bus_init  simplefile_lock   |   <-- shared port code
   | app_threads_create  logger       |
   +----------------+-----------------+
                    |
              platform.h            <-- the hardware seam
             /            \
   platform_stm32.c     platform_host.c
   (board hooks)        (stubs/stdout)
```

Only `platform_stm32.c`'s hooks and the FSMC backend differ per target.
Everything above `platform.h` is one shared codebase.

## Edit 1 — call app_main() from tx_application_define

In `AZURE_RTOS/App/app_azure_rtos.c`, at the **end** of the
`MX_USBX_Host_Init_Success` USER CODE block (right after your si5351 init, where
all pools + USBX + FileX + clocks are ready):

```c
    /* USER CODE BEGIN MX_USBX_Host_Init_Success */
    thread_toggle_init();
    log_printf("Init SI5351\r\n");
    /* ... your existing si5351 init ... */
    log_printf("Init SI5351 DONE\r\n");

    /* --- TonnereXL port entry --- */
    {
        extern UINT app_main(const void *cfg);   /* app_config_t* */
        static struct { TX_BYTE_POOL *thread_pool; } port_cfg;
        port_cfg.thread_pool = &tx_app_byte_pool;
        if (app_main(&port_cfg) != TX_SUCCESS) {
            log_printf("app_main failed\r\n");
        }
    }
    /* USER CODE END MX_USBX_Host_Init_Success */
```

(Or `#include "app_main.h"` and use the real `app_config_t` type instead of the
local struct — cleaner if you add the port's App/ dir to your includes, which
edit 3 does anyway.)

## Edit 2 — provide the board hooks

The port calls board-provided functions (declared `weak` so it links without
them). Add real ones in your board code:

```c
/* Emit one log byte to your console transport (UART — you've wired this). */
void board_log_putc(char c)
{
    /* HAL_UART_Transmit(&huartX,(uint8_t*)&c,1,HAL_MAX_DELAY); */
}

/* Apply an si5351 mode. The port never sees SI5351_MODE_*; platform_stm32.c
 * maps the abstract (standard, refresh) pair to one of these mode ids and
 * calls this. Map the int to your real si5351_apply_mode on the right pca9546
 * channels. The int values are the BSI_* enum in platform_stm32.c — keep them
 * in sync with your SI5351_MODE_* order, or edit resolve_mode() to match. */
void board_si5351_apply_mode(int si5351_mode)
{
    /* e.g. pca9546_select(2); si5351_apply_mode((si5351_mode_t)si5351_mode);
     *      pca9546_select(3); si5351_apply_mode((si5351_mode_t)si5351_mode); */
}
```

The port requests video via `platform_set_video(standard, refresh)` and checks
`platform_video_supported(...)`; the (standard, refresh) → si5351 mapping and the
valid-pair table live in `platform_stm32.c`'s `resolve_mode()`. Today that table
is hardcoded; later it can read monitor EDID/HDMI caps over I2C behind the same
interface, without the port changing.

## Edit 3 — add the port sources to your CMake / project

Add these to your STM32Cube build (CMakeLists or the IDE project), with
`FPGA_BUS_STM32` defined and the three port dirs on the include path:

Sources:
```
Platform/fpga_bus_stm32.c      # NOT fpga_bus_fake.c
Platform/platform_stm32.c      # NOT platform_host.c
App/app_main.c
App/logger.c                   # replaces your logger.c, or keep yours — see note
App/app_threads.c
App/app_service_steps.c
Filesystem/simplefile_filex.c
```

Include dirs: `Platform/ App/ Filesystem/`
Compile define: `FPGA_BUS_STM32`

**Logger note:** the port ships a shared `logger.c`/`logger.h` whose only
board-specific part is `board_log_putc` (edit 2). If you keep your existing
`logger.c` instead, just ensure it provides `log_printf`/`log_puts` with the
same signatures and you can drop the port's `App/logger.c` from the build. Using
the port's logger means host and board log through identical code.

## Byte pool sizing

`app_threads_create` allocates thread stacks from `tx_app_byte_pool`:

| Thread | Stack |
| --- | --- |
| drive/SIO | 2 KB |
| usb input | 2 KB |
| sd lifecycle | 4 KB |
| menu | 3 KB |
| input queue | 128 B |

Plus ThreadX control-block and byte-pool allocation overhead: budget **~12 KB**
of `TX_APP_MEM_POOL_SIZE` for the port, on top of whatever your existing app
uses. If `TX_APP_MEM_POOL_SIZE` is currently sized only for the CubeMX template,
bump it in the `.ioc` (ThreadX → memory pool size) by ~12 KB, regenerate, and
your USER CODE edits survive (they're in preserved blocks).

If `app_threads_create` returns non-`TX_SUCCESS`, the pool is too small — that's
the first thing to check.

## SDIO init (Phase 3, when you get there)

`MX_SDIO_SD_Init()` is still commented out in `main.c`. The SD lifecycle thread
will call it on first card-detect (so a later-inserted card works). No action
needed now; noted so it's not forgotten.
