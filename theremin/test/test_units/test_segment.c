#include <unity.h>

#include "fakes.h"
#include "segment.h"
#include "tests.h"

#define HEX_DIGITS          16U
#define SEGMENT_ADDRESS     0x39
#define DECIMAL_POINT       0x80
#define SHOWN_DIGIT         0x5

/* Standard 7-segment codes, bit 0 = segment A ... bit 6 = segment G. */
static const uint8_t EXPECTED_PATTERNS[HEX_DIGITS] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
    0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71,
};

static void test_segment_pattern_matches_standard_hex_digits(void)
{
    for (uint8_t digit = 0; digit < HEX_DIGITS; digit++) {
        TEST_ASSERT_EQUAL_HEX8(EXPECTED_PATTERNS[digit], segment_pattern(digit));
    }
}

static void test_segment_pattern_uses_only_the_low_nibble(void)
{
    TEST_ASSERT_EQUAL_HEX8(segment_pattern(0xB), segment_pattern(0x1B));
}

static void test_segment_show_sends_active_low_pattern(void)
{
    segment_show(SHOWN_DIGIT);

    TEST_ASSERT_EQUAL_HEX8(SEGMENT_ADDRESS, sentAddress);
    TEST_ASSERT_EQUAL_UINT8(1, sentLength);
    TEST_ASSERT_EQUAL_HEX8((uint8_t)~segment_pattern(SHOWN_DIGIT), sentData);
    TEST_ASSERT_BITS_HIGH(DECIMAL_POINT, sentData);             /* active low, so the point is off */
}

void run_segment_tests(void)
{
    RUN_TEST(test_segment_pattern_matches_standard_hex_digits);
    RUN_TEST(test_segment_pattern_uses_only_the_low_nibble);
    RUN_TEST(test_segment_show_sends_active_low_pattern);
}
