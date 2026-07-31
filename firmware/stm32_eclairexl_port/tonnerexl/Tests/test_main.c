/*
 * test_main.c — host test runner.
 *
 * Boots the real ThreadX scheduler (linux port) and, from within a thread:
 *   1. runs the component unit suites (fpga_bus, simplefile), then
 *   2. calls the SHARED app_main() to prove the port's real startup path works
 *      on the host exactly as it will on the board.
 * FileX runs on its RAM driver; fpga_bus uses the fake backend; platform is the
 * host stub.
 */
#include "tx_api.h"
#include "app_main.h"
#include "test_harness.h"
#include <stdlib.h>
#include <stdio.h>

int g_tests_run = 0;
int g_tests_failed = 0;

void run_fpga_bus_tests(void);
void run_simplefile_tests(void);

static TX_THREAD test_thread;
static UCHAR     test_stack[32*1024];

/* a byte pool for app_main's threads, mirroring the board's tx_app_byte_pool */
static TX_BYTE_POOL app_pool;
static UCHAR        app_pool_mem[64*1024];

static void test_thread_entry(ULONG arg) {
    (void)arg;
    printf("=== TonnereXL host test suite ===\n\n");
    printf("--- fpga_bus (fake backend / contract model) ---\n");
    run_fpga_bus_tests();
    printf("\n--- simplefile on FileX (real RAM disk) ---\n");
    run_simplefile_tests();

    printf("\n--- app_main() shared startup path ---\n");
    tx_byte_pool_create(&app_pool, "app", app_pool_mem, sizeof app_pool_mem);
    app_config_t cfg = { .thread_pool = &app_pool };
    UINT st = app_main(&cfg);
    CHECK(st == TX_SUCCESS);
    /* give the started threads a couple of ticks to run their first iteration */
    tx_thread_sleep(4);

    printf("\n=== %d checks, %d failed ===\n", g_tests_run, g_tests_failed);
    exit(g_tests_failed ? 1 : 0);
}

void tx_application_define(void *first_unused) {
    (void)first_unused;
    tx_thread_create(&test_thread, "tests", test_thread_entry, 0,
                     test_stack, sizeof test_stack,
                     15, 15, TX_NO_TIME_SLICE, TX_AUTO_START);
}

int main(void) { tx_kernel_enter(); return 0; }
