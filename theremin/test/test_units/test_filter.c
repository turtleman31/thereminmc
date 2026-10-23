#include <stdbool.h>
#include <unity.h>

#include "filter.h"
#include "tests.h"

#define START_VALUE     500U
#define SPIKE_VALUE     900U
#define SMALL_SIZE      3U
#define LARGE_SIZE      7U

static MedianFilter filter;

static bool ages_are_unique(const MedianFilter *f)
{
    uint16_t seen = 0;

    for (const Sample *p = f->samples; p < f->samples + f->size; p++) {
        if (p->age >= f->size || (seen & (1U << p->age))) {
            return false;
        }
        seen |= 1U << p->age;
    }
    return true;
}

static void test_filter_init_fills_all_samples_with_start_value(void)
{
    filter_init(&filter, FILTER_SIZE_DEFAULT, START_VALUE);

    TEST_ASSERT_NOT_NULL(filter.samples);
    TEST_ASSERT_EQUAL_UINT8(FILTER_SIZE_DEFAULT, filter.size);
    for (const Sample *p = filter.samples; p < filter.samples + filter.size; p++) {
        TEST_ASSERT_EQUAL_UINT16(START_VALUE, p->value);
    }
    TEST_ASSERT_TRUE(ages_are_unique(&filter));
    filter_free(&filter);
}

static void test_filter_init_with_size_one_allocates_nothing(void)
{
    filter_init(&filter, FILTER_SIZE_MIN, START_VALUE);

    TEST_ASSERT_NULL(filter.samples);
    TEST_ASSERT_EQUAL_UINT8(FILTER_SIZE_MIN, filter.size);
}

static void test_filter_add_returns_median_of_last_samples(void)
{
    filter_init(&filter, SMALL_SIZE, 0);

    TEST_ASSERT_EQUAL_UINT16(0, filter_add(&filter, 10));     /* 0, 0, 10 */
    TEST_ASSERT_EQUAL_UINT16(10, filter_add(&filter, 30));    /* 0, 10, 30 */
    TEST_ASSERT_EQUAL_UINT16(20, filter_add(&filter, 20));    /* 10, 30, 20 */
    TEST_ASSERT_EQUAL_UINT16(20, filter_add(&filter, 5));     /* 30, 20, 5 */
    filter_free(&filter);
}

static void test_filter_add_removes_single_spike(void)
{
    filter_init(&filter, FILTER_SIZE_DEFAULT, START_VALUE);

    TEST_ASSERT_EQUAL_UINT16(START_VALUE, filter_add(&filter, START_VALUE));
    TEST_ASSERT_EQUAL_UINT16(START_VALUE, filter_add(&filter, SPIKE_VALUE));
    TEST_ASSERT_EQUAL_UINT16(START_VALUE, filter_add(&filter, START_VALUE));
    filter_free(&filter);
}

static void test_filter_add_with_size_one_passes_value_through(void)
{
    filter_init(&filter, FILTER_SIZE_MIN, START_VALUE);

    TEST_ASSERT_EQUAL_UINT16(SPIKE_VALUE, filter_add(&filter, SPIKE_VALUE));
}

static void test_filter_resize_grow_fills_new_samples_with_last_median(void)
{
    filter_init(&filter, SMALL_SIZE, START_VALUE);
    filter_resize(&filter, LARGE_SIZE);

    TEST_ASSERT_EQUAL_UINT8(LARGE_SIZE, filter.size);
    TEST_ASSERT_TRUE(ages_are_unique(&filter));
    TEST_ASSERT_EQUAL_UINT16(START_VALUE, filter_add(&filter, SPIKE_VALUE));
    filter_free(&filter);
}

static void test_filter_resize_shrink_keeps_newest_samples(void)
{
    filter_init(&filter, FILTER_SIZE_DEFAULT, 0);
    for (uint16_t value = 10; value <= 50; value += 10) {
        filter_add(&filter, value);
    }
    filter_resize(&filter, SMALL_SIZE);                         /* keeps 30, 40, 50 */

    TEST_ASSERT_EQUAL_UINT8(SMALL_SIZE, filter.size);
    TEST_ASSERT_TRUE(ages_are_unique(&filter));
    TEST_ASSERT_EQUAL_UINT16(50, filter_add(&filter, 60));     /* 40, 50, 60 */
    filter_free(&filter);
}

static void test_filter_resize_to_one_frees_memory(void)
{
    filter_init(&filter, FILTER_SIZE_DEFAULT, START_VALUE);
    filter_resize(&filter, FILTER_SIZE_MIN);

    TEST_ASSERT_NULL(filter.samples);
    TEST_ASSERT_EQUAL_UINT8(FILTER_SIZE_MIN, filter.size);
}

static void test_filter_resize_from_off_allocates_again(void)
{
    filter_init(&filter, FILTER_SIZE_MIN, 0);
    filter_add(&filter, START_VALUE);
    filter_resize(&filter, FILTER_SIZE_MAX);

    TEST_ASSERT_NOT_NULL(filter.samples);
    TEST_ASSERT_EQUAL_UINT8(FILTER_SIZE_MAX, filter.size);
    TEST_ASSERT_TRUE(ages_are_unique(&filter));
    TEST_ASSERT_EQUAL_UINT16(START_VALUE, filter.samples[0].value);
    filter_free(&filter);
}

static void test_filter_free_clears_pointer_and_can_be_repeated(void)
{
    filter_init(&filter, FILTER_SIZE_DEFAULT, START_VALUE);
    filter_free(&filter);

    TEST_ASSERT_NULL(filter.samples);
    TEST_ASSERT_EQUAL_UINT8(FILTER_SIZE_MIN, filter.size);
    filter_free(&filter);
    TEST_ASSERT_NULL(filter.samples);
}

static void test_filter_compare_value_orders_by_value(void)
{
    Sample low = { .age = 4, .value = 100 };
    Sample high = { .age = 0, .value = 200 };

    TEST_ASSERT_TRUE(filter_compare_value(&low, &high) < 0);
    TEST_ASSERT_TRUE(filter_compare_value(&high, &low) > 0);
    TEST_ASSERT_EQUAL_INT(0, filter_compare_value(&low, &low));
}

static void test_filter_compare_age_orders_by_age(void)
{
    Sample young = { .age = 0, .value = 200 };
    Sample old = { .age = 4, .value = 100 };

    TEST_ASSERT_TRUE(filter_compare_age(&young, &old) < 0);
    TEST_ASSERT_TRUE(filter_compare_age(&old, &young) > 0);
    TEST_ASSERT_EQUAL_INT(0, filter_compare_age(&old, &old));
}

void run_filter_tests(void)
{
    RUN_TEST(test_filter_init_fills_all_samples_with_start_value);
    RUN_TEST(test_filter_init_with_size_one_allocates_nothing);
    RUN_TEST(test_filter_add_returns_median_of_last_samples);
    RUN_TEST(test_filter_add_removes_single_spike);
    RUN_TEST(test_filter_add_with_size_one_passes_value_through);
    RUN_TEST(test_filter_resize_grow_fills_new_samples_with_last_median);
    RUN_TEST(test_filter_resize_shrink_keeps_newest_samples);
    RUN_TEST(test_filter_resize_to_one_frees_memory);
    RUN_TEST(test_filter_resize_from_off_allocates_again);
    RUN_TEST(test_filter_free_clears_pointer_and_can_be_repeated);
    RUN_TEST(test_filter_compare_value_orders_by_value);
    RUN_TEST(test_filter_compare_age_orders_by_age);
}
