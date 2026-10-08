#pragma once
// Minimal assertion helpers for the host tests. One test executable is one translation unit, so the
// failure counter lives here. End main() with `return check_report();`.
#include <stdio.h>

static int check_failures;
static int check_total;

static inline void check_true(int ok, const char *file, int line, const char *expr)
{
    check_total++;
    if (!ok)
    {
        check_failures++;
        printf("%s:%d: CHECK failed: %s\n", file, line, expr);
    }
}

static inline void check_equal(long long a, long long b, const char *file, int line, const char *expr_a, const char *expr_b)
{
    check_total++;
    if (a != b)
    {
        check_failures++;
        printf("%s:%d: CHECK_EQ failed: %s (= %lld) != %s (= %lld)\n", file, line, expr_a, a, expr_b, b);
    }
}

static inline int check_report(void)
{
    printf("%d checks, %d failed\n", check_total, check_failures);
    return check_failures ? 1 : 0;
}

#define CHECK(cond) check_true(!!(cond), __FILE__, __LINE__, #cond)
#define CHECK_EQ(a, b) check_equal((long long)(a), (long long)(b), __FILE__, __LINE__, #a, #b)
