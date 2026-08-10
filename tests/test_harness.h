#ifndef ARCANUM_TESTS_TEST_HARNESS_H_
#define ARCANUM_TESTS_TEST_HARNESS_H_

// A deliberately tiny test harness.
//
// No external framework: the point of these tests is to run anywhere with no
// assets, no display and no network, and adding a dependency to achieve that
// would be self-defeating. Everything here is a macro over `printf` and a
// counter.
//
// Usage:
//
//     TEST(parses_a_plain_value)
//     {
//         char buf[] = "42";
//         char* str = buf;
//         int value = 0;
//         tig_str_parse_value(&str, &value);
//         CHECK_INT(value, 42);
//     }
//
//     // in the suite's runner
//     RUN(parses_a_plain_value);
//
// A failing CHECK reports and lets the rest of the test body run, so one broken
// assumption does not hide the others behind it.

#include <stdio.h>
#include <string.h>

extern int test_checks_run;
extern int test_checks_failed;
extern int test_cases_run;
extern int test_cases_failed;
extern int test_current_failed;

#define TEST(name) static void name(void)

#define RUN(name) test_run(#name, name)

#define CHECK(cond)                                                     \
    test_check((cond) ? 1 : 0, #cond, __FILE__, __LINE__)

#define CHECK_INT(actual, expected)                                     \
    test_check_int((long long)(actual), (long long)(expected),          \
        #actual, __FILE__, __LINE__)

#define CHECK_STR(actual, expected)                                     \
    test_check_str((actual), (expected), #actual, __FILE__, __LINE__)

// `tig_str_parse_*` advances the caller's pointer and sets it to NULL when the
// input is exhausted, so "where did it stop" is half of what these tests are
// checking. This spells that out rather than making every suite write it.
#define CHECK_AT_END(str)                                               \
    test_check((str) == NULL, #str " == NULL (input exhausted)",        \
        __FILE__, __LINE__)

#define CHECK_REMAINDER(str, expected)                                  \
    test_check_str((str), (expected), #str " (remainder)",              \
        __FILE__, __LINE__)

void test_run(const char* name, void (*func)(void));
void test_check(int ok, const char* expr, const char* file, int line);
void test_check_int(long long actual, long long expected, const char* expr,
    const char* file, int line);
void test_check_str(const char* actual, const char* expected, const char* expr,
    const char* file, int line);
int test_report(void);

#endif /* ARCANUM_TESTS_TEST_HARNESS_H_ */
