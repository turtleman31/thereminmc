/* Unity test runner for all hardware-independent units (testplan, chapter Unittesten). */

#include <unity.h>

#include "tests.h"

void setUp(void)
{
}

void tearDown(void)
{
}

int main(void)
{
    UNITY_BEGIN();
    run_filter_tests();
    run_ldr_tests();
    run_buzzer_tests();
    run_buttons_tests();
    run_segment_tests();
    return UNITY_END();
}
