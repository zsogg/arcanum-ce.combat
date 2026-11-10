#include "test_harness.h"

void test_script_suite(void);
void test_out_of_range_lines_suite(void);

int main(void)
{
    test_script_suite();
    test_out_of_range_lines_suite();
    return test_report();
}
