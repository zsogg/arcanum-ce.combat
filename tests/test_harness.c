#include "test_harness.h"

int test_checks_run;
int test_checks_failed;
int test_cases_run;
int test_cases_failed;
int test_current_failed;

void test_run(const char* name, void (*func)(void))
{
    test_current_failed = 0;
    test_cases_run++;

    func();

    if (test_current_failed != 0) {
        test_cases_failed++;
        printf("FAIL %s\n", name);
    }
}

// Failure output is flushed as it is produced. When stdout is redirected it is
// block-buffered, so a suite that hangs or is killed after a failing check
// loses every line it had written - the run then looks like it found nothing,
// which is this project's most-repeated failure mode wearing a new hat.
// Measured 2026-08-25: a mutation that stopped `tig_find_next_file` advancing
// its cursor was correctly caught here, and the evidence was invisible because
// `tig_file_list_create_native` then hung the binary before the buffer drained.
void test_check(int ok, const char* expr, const char* file, int line)
{
    test_checks_run++;
    if (ok) {
        return;
    }

    test_checks_failed++;
    test_current_failed++;
    printf("  %s:%d: %s\n", file, line, expr);
    fflush(stdout);
}

void test_check_int(long long actual, long long expected, const char* expr,
    const char* file, int line)
{
    test_checks_run++;
    if (actual == expected) {
        return;
    }

    test_checks_failed++;
    test_current_failed++;
    printf("  %s:%d: %s\n      expected: %lld\n      actual:   %lld\n",
        file, line, expr, expected, actual);
    fflush(stdout);
}

void test_check_str(const char* actual, const char* expected, const char* expr,
    const char* file, int line)
{
    test_checks_run++;

    if (actual == NULL || expected == NULL) {
        if (actual == expected) {
            return;
        }
    } else if (strcmp(actual, expected) == 0) {
        return;
    }

    test_checks_failed++;
    test_current_failed++;
    printf("  %s:%d: %s\n      expected: %s\n      actual:   %s\n",
        file, line, expr,
        expected != NULL ? expected : "(null)",
        actual != NULL ? actual : "(null)");
    fflush(stdout);
}

int test_report(void)
{
    printf("\n%d checks in %d cases", test_checks_run, test_cases_run);
    if (test_checks_failed == 0) {
        printf(" - all passed\n");
        return 0;
    }

    printf(" - %d checks FAILED in %d cases\n",
        test_checks_failed, test_cases_failed);
    return 1;
}
