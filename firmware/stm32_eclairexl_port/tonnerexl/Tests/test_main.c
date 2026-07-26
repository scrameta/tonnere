/*
 * test_main.c — host test runner.
 *
 * Boots the REAL ThreadX scheduler (upstream linux port) and runs all suites
 * from within a thread, so the filesystem/mutex code executes in the same
 * environment it will on-target. FileX runs on its RAM driver; fpga_bus uses
 * the fake backend.
 */
#include "tx_api.h"
#include "test_harness.h"
#include <stdlib.h>

int g_tests_run = 0;
int g_tests_failed = 0;

void run_fpga_bus_tests(void);
void run_simplefile_tests(void);

static TX_THREAD test_thread;
static UCHAR     test_stack[32*1024];

static void test_thread_entry(ULONG arg) {
    (void)arg;
    printf("=== TonnereXL host test suite ===\n\n");
    printf("--- fpga_bus (fake backend / contract model) ---\n");
    run_fpga_bus_tests();
    printf("\n--- simplefile on FileX (real RAM disk) ---\n");
    run_simplefile_tests();

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
