#include "logger.h"

#include "main.h"
#include "tx_api.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LOG_QUEUE_DEPTH 16
#define LOG_MSG_LEN     128
#define LOG_STACK_WORDS  256

extern UART_HandleTypeDef huart3;   // or whichever CubeMX generated

typedef struct {
    char text[LOG_MSG_LEN];
} log_msg_t;

static TX_THREAD logger_thread;
static ULONG logger_stack[LOG_STACK_WORDS];

/* Queue stores pointers, not whole 128-byte structs. */
static TX_QUEUE log_queue;
static ULONG log_queue_storage[LOG_QUEUE_DEPTH];

static log_msg_t log_slots[LOG_QUEUE_DEPTH];
static ULONG log_slot_free_mask = (1UL << LOG_QUEUE_DEPTH) - 1UL;

static TX_MUTEX log_mutex;

static void logger_thread_entry(ULONG arg)
{
    (void)arg;

    log_msg_t *msg = NULL;

    while (1) {
        if (tx_queue_receive(&log_queue, &msg, TX_WAIT_FOREVER) == TX_SUCCESS) {
            size_t len = strnlen(msg->text, LOG_MSG_LEN);

            if (len > 0) {
                HAL_UART_Transmit(&huart3,
                                  (uint8_t *)msg->text,
                                  (uint16_t)len,
                                  100);
            }

            tx_mutex_get(&log_mutex, TX_WAIT_FOREVER);
            for (unsigned i = 0; i < LOG_QUEUE_DEPTH; i++) {
                if (&log_slots[i] == msg) {
                    log_slot_free_mask |= (1UL << i);
                    break;
                }
            }
            tx_mutex_put(&log_mutex);
        }
    }
}

void logger_init(void)
{
    UINT s;

    s = tx_mutex_create(&log_mutex, "log_mutex", TX_NO_INHERIT);
    if (s != TX_SUCCESS) {
        Error_Handler();
    }

    s = tx_queue_create(&log_queue,
                        "log_queue",
                        TX_1_ULONG,
                        log_queue_storage,
                        sizeof(log_queue_storage));
    if (s != TX_SUCCESS) {
        Error_Handler();
    }

    s = tx_thread_create(&logger_thread,
                         "logger",
                         logger_thread_entry,
                         0,
                         logger_stack,
                         sizeof(logger_stack),
                         10,          /* lower priority than bring-up threads */
                         10,
                         TX_NO_TIME_SLICE,
                         TX_AUTO_START);
    if (s != TX_SUCCESS) {
        Error_Handler();
    }
}

void log_printf(const char *fmt, ...)
{
    log_msg_t *slot = NULL;

    if (tx_mutex_get(&log_mutex, TX_NO_WAIT) != TX_SUCCESS) {
        return;
    }

    for (unsigned i = 0; i < LOG_QUEUE_DEPTH; i++) {
        if (log_slot_free_mask & (1UL << i)) {
            log_slot_free_mask &= ~(1UL << i);
            slot = &log_slots[i];
            break;
        }
    }

    tx_mutex_put(&log_mutex);

    if (slot == NULL) {
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(slot->text, LOG_MSG_LEN, fmt, ap);
    va_end(ap);

    slot->text[LOG_MSG_LEN - 1] = '\0';

    if (tx_queue_send(&log_queue, &slot, TX_NO_WAIT) != TX_SUCCESS) {
        tx_mutex_get(&log_mutex, TX_WAIT_FOREVER);
        for (unsigned i = 0; i < LOG_QUEUE_DEPTH; i++) {
            if (&log_slots[i] == slot) {
                log_slot_free_mask |= (1UL << i);
                break;
            }
        }
        tx_mutex_put(&log_mutex);
    }
}
