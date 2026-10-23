#include <unity.h>

#include "buzzer.h"
#include "tests.h"

#define TONE_PRESCALER          256UL
#define HALF_PERIODS_PER_CYCLE  2UL
#define MAX_ERROR_PERCENT       5U          /* testplan, system test UC.5 step 7 */
#define PERCENT                 100U

static uint32_t produced_frequency(uint8_t compareValue)
{
    return F_CPU / (HALF_PERIODS_PER_CYCLE * TONE_PRESCALER * (compareValue + 1UL));
}

static void test_buzzer_compare_value_for_lowest_tone(void)
{
    TEST_ASSERT_EQUAL_UINT8(135, buzzer_compare_value(FREQUENCY_MIN_HZ));
}

static void test_buzzer_compare_value_for_highest_tone(void)
{
    TEST_ASSERT_EQUAL_UINT8(21, buzzer_compare_value(FREQUENCY_MAX_HZ));
}

static void test_buzzer_every_tone_within_5_percent(void)
{
    for (uint16_t frequency = FREQUENCY_MIN_HZ; frequency <= FREQUENCY_MAX_HZ; frequency++) {
        uint32_t produced = produced_frequency(buzzer_compare_value(frequency));

        TEST_ASSERT_UINT32_WITHIN(frequency * MAX_ERROR_PERCENT / PERCENT, frequency, produced);
    }
}

void run_buzzer_tests(void)
{
    RUN_TEST(test_buzzer_compare_value_for_lowest_tone);
    RUN_TEST(test_buzzer_compare_value_for_highest_tone);
    RUN_TEST(test_buzzer_every_tone_within_5_percent);
}
