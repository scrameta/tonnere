/* Tiny assert-based test harness (no external deps). */
#ifndef TONNEREXL_TEST_HARNESS_H
#define TONNEREXL_TEST_HARNESS_H
#include <stdio.h>
#include <string.h>

extern int g_tests_run, g_tests_failed;

#define CHECK(cond) do { \
    g_tests_run++; \
    if (!(cond)) { g_tests_failed++; \
        printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

#define CHECK_EQ_U32(a,b) do { \
    g_tests_run++; \
    uint32_t _a=(uint32_t)(a), _b=(uint32_t)(b); \
    if (_a!=_b) { g_tests_failed++; \
        printf("  FAIL %s:%d  %s(0x%08x) != %s(0x%08x)\n", \
               __FILE__, __LINE__, #a,_a,#b,_b); } \
} while (0)

#define RUN(fn) do { printf("[ %s ]\n", #fn); fn(); } while (0)

#endif
