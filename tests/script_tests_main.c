#include "test_harness.h"

void test_script_suite(void);

int main(void)
{
    test_script_suite();
    return test_report();
}
