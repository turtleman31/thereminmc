#include <unity.h>

#include "fakes.h"
#include "segment.h"
#include "tests.h"

#define HEX_DIGITS          16U
#define PCF8574A_ADDRESS    0x39        /* gameshield schematic: A0 = 1, A1 = A2 = 0 */
#define PCF8574_ADDRESS     0x21
#define NO_DEVICE           0x00
#define DECIMAL_POINT       0x80
#define SHOWN_DIGIT         0x5

/* Standard 7-segment codes, bit 0 = segment A ... bit 6 = segment G. */
static const uint8_t EXPECTED_PATTERNS[HEX_DIGITS] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
    0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71,
};

/* Runs segment_init() with only deviceAddress on the bus; returns the address segment_show() uses. */
static uint8_t address_after_init(uint8_t deviceAddress)
{
    fakeDeviceAddress = deviceAddress;
    segment_init();
    segment_show(SHOWN_DIGIT);
    return sentAddress;
}

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
    TEST_ASSERT_EQUAL_HEX8(PCF8574A_ADDRESS, address_after_init(PCF8574A_ADDRESS));
    TEST_ASSERT_EQUAL_UINT8(1, sentLength);
    TEST_ASSERT_EQUAL_HEX8((uint8_t)~segment_pattern(SHOWN_DIGIT), sentData);
    TEST_ASSERT_BITS_HIGH(DECIMAL_POINT, sentData);             /* active low, so the point is off */
}

static void test_segment_init_finds_pcf8574_without_a(void)
{
    TEST_ASSERT_EQUAL_HEX8(PCF8574_ADDRESS, address_after_init(PCF8574_ADDRESS));
}

static void test_segment_init_without_answer_uses_schematic_address(void)
{
    address_after_init(PCF8574_ADDRESS);
    TEST_ASSERT_EQUAL_HEX8(PCF8574A_ADDRESS, address_after_init(NO_DEVICE));
}

void run_segment_tests(void)
{
    RUN_TEST(test_segment_pattern_matches_standard_hex_digits);
    RUN_TEST(test_segment_pattern_uses_only_the_low_nibble);
    RUN_TEST(test_segment_show_sends_active_low_pattern);
    RUN_TEST(test_segment_init_finds_pcf8574_without_a);
    RUN_TEST(test_segment_init_without_answer_uses_schematic_address);
}
