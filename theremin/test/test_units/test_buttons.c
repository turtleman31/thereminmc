#include <avr/io.h>
#include <unity.h>

#include "buttons.h"
#include "fakes.h"
#include "filter.h"
#include "tests.h"

#define BUTTONS_RELEASED    (_BV(PD4) | _BV(PD5))
#define BUTTON_MINUS        _BV(PD4)
#define BUTTON_PLUS         _BV(PD5)
#define IDLE_TIME_MS        1000U
#define SHORT_PRESS_MS      150U
#define LONG_PRESS_MS       3000U

void PCINT2_vect(void);

static void set_buttons(uint8_t pins, uint32_t timeMs)
{
    fakeTimeMs = timeMs;
    PIND = pins;
    PCINT2_vect();
}

/* A press with contact bounce on both the press and the release. */
static void press(uint8_t button, uint32_t holdMs)
{
    uint32_t start = fakeTimeMs + IDLE_TIME_MS;
    uint8_t pressed = BUTTONS_RELEASED & (uint8_t)~button;

    set_buttons(pressed, start);
    set_buttons(BUTTONS_RELEASED, start + 1);
    set_buttons(pressed, start + 2);
    set_buttons(BUTTONS_RELEASED, start + holdMs);
    set_buttons(pressed, start + holdMs + 1);
    set_buttons(BUTTONS_RELEASED, start + holdMs + 2);
}

static void reset_filter_size(void)
{
    uint8_t ignored;

    buttons_set_filter_size(FILTER_SIZE_DEFAULT);
    buttons_filter_size_changed(&ignored);
}

static void test_buttons_start_size_is_reported_once(void)
{
    uint8_t size = 0;

    TEST_ASSERT_TRUE(buttons_filter_size_changed(&size));
    TEST_ASSERT_EQUAL_UINT8(FILTER_SIZE_DEFAULT, size);
    TEST_ASSERT_FALSE(buttons_filter_size_changed(&size));
}

static void test_buttons_step_up_by_two(void)
{
    TEST_ASSERT_EQUAL_UINT8(7, buttons_step_filter_size(5, true));
}

static void test_buttons_step_down_by_two(void)
{
    TEST_ASSERT_EQUAL_UINT8(3, buttons_step_filter_size(5, false));
}

static void test_buttons_step_stays_at_maximum(void)
{
    TEST_ASSERT_EQUAL_UINT8(FILTER_SIZE_MAX, buttons_step_filter_size(FILTER_SIZE_MAX, true));
}

static void test_buttons_step_stays_at_minimum(void)
{
    TEST_ASSERT_EQUAL_UINT8(FILTER_SIZE_MIN, buttons_step_filter_size(FILTER_SIZE_MIN, false));
}

static void test_buttons_bouncing_press_counts_once(void)
{
    uint8_t size = 0;

    reset_filter_size();
    press(BUTTON_PLUS, SHORT_PRESS_MS);
    TEST_ASSERT_TRUE(buttons_filter_size_changed(&size));
    TEST_ASSERT_EQUAL_UINT8(7, size);
    TEST_ASSERT_FALSE(buttons_filter_size_changed(&size));
}

static void test_buttons_holding_counts_once(void)
{
    uint8_t size = 0;

    reset_filter_size();
    press(BUTTON_MINUS, LONG_PRESS_MS);
    TEST_ASSERT_TRUE(buttons_filter_size_changed(&size));
    TEST_ASSERT_EQUAL_UINT8(3, size);
}

static void test_buttons_press_at_limit_changes_nothing(void)
{
    uint8_t size = 0;

    buttons_set_filter_size(FILTER_SIZE_MAX);
    buttons_filter_size_changed(&size);
    press(BUTTON_PLUS, SHORT_PRESS_MS);
    TEST_ASSERT_FALSE(buttons_filter_size_changed(&size));
}

void run_buttons_tests(void)
{
    PIND = BUTTONS_RELEASED;
    RUN_TEST(test_buttons_start_size_is_reported_once);
    RUN_TEST(test_buttons_step_up_by_two);
    RUN_TEST(test_buttons_step_down_by_two);
    RUN_TEST(test_buttons_step_stays_at_maximum);
    RUN_TEST(test_buttons_step_stays_at_minimum);
    RUN_TEST(test_buttons_bouncing_press_counts_once);
    RUN_TEST(test_buttons_holding_counts_once);
    RUN_TEST(test_buttons_press_at_limit_changes_nothing);
}
