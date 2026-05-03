#include <stdint.h>
#include "tx_api.h"
#include "main.h"

#include "thread_toggle.h"
#include "logger.h"

static TX_THREAD thread_toggle;
static ULONG toggle_stack[128];

static void thread_toggle_entry(ULONG arg)
{
    log_printf("Hello world");
    while (1)
    {
        HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_6);
        tx_thread_sleep(50);
    }
}

void thread_toggle_init(void)
{
    tx_thread_create(&thread_toggle,
                     "Toggle",
                     thread_toggle_entry,
                     0,
                     toggle_stack,
                     sizeof(toggle_stack),
                     5,
                     5,
                     TX_NO_TIME_SLICE,
                     TX_AUTO_START);
}
