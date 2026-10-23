#include <unity.h>

#include "buzzer.h"
#include "fakes.h"
#include "filter.h"
#include "ldr.h"
#include "tests.h"
#include "volume.h"

#define DISTANCE_MAX_CM         65U         /* NFE.2 */
#define CALIBRATION_TIME_MS     5000U
#define SAMPLE_INTERVAL_MS      20U
#define LEVEL_DARK              300U        /* hand on the sensor */
#define LEVEL_LIGHT             800U        /* no hand */
#define LEVEL_AMBIENT           700U
#define FREQUENCY_TOLERANCE_HZ  20U
#define DISTANCE_TOLERANCE_CM   1U

static DistanceSensor calibrated_sensor(void)
{
    DistanceSensor sensor = { .state = LDR_STATE_MEASURE, .adcMin = LEVEL_DARK, .adcMax = LEVEL_LIGHT };

    return sensor;
}

static uint16_t level_at_distance(uint8_t distanceCm)
{
    return LEVEL_DARK + (uint16_t)((LEVEL_LIGHT - LEVEL_DARK) * distanceCm / DISTANCE_MAX_CM);
}

/* Feeds one LDR level for durationMs; returns the result of the last ldr_update(). */
static bool feed_level(DistanceSensor *sensor, MedianFilter *filter, uint16_t level, uint32_t durationMs)
{
    bool updated = false;

    fakeLdrLevel = level;
    for (uint32_t elapsed = 0; elapsed < durationMs; elapsed += SAMPLE_INTERVAL_MS) {
        fakeTimeMs += SAMPLE_INTERVAL_MS;
        updated = ldr_update(sensor, filter);
    }
    return updated;
}

static void test_ldr_no_hand_gives_max_distance_and_lowest_tone(void)
{
    DistanceSensor sensor = calibrated_sensor();

    ldr_calculate(&sensor, LEVEL_LIGHT);
    TEST_ASSERT_EQUAL_UINT8(DISTANCE_MAX_CM, sensor.distanceCm);
    TEST_ASSERT_EQUAL_UINT16(FREQUENCY_MIN_HZ, sensor.frequencyHz);
}

static void test_ldr_reading_above_range_is_clamped(void)
{
    DistanceSensor sensor = calibrated_sensor();

    ldr_calculate(&sensor, ADC_VALUE_MAX);
    TEST_ASSERT_EQUAL_UINT8(DISTANCE_MAX_CM, sensor.distanceCm);
    TEST_ASSERT_EQUAL_UINT16(FREQUENCY_MIN_HZ, sensor.frequencyHz);
}

static void test_ldr_hand_on_sensor_gives_highest_tone(void)
{
    DistanceSensor sensor = calibrated_sensor();

    ldr_calculate(&sensor, LEVEL_DARK);
    TEST_ASSERT_EQUAL_UINT8(0, sensor.distanceCm);
    TEST_ASSERT_EQUAL_UINT16(FREQUENCY_MAX_HZ, sensor.frequencyHz);

    ldr_calculate(&sensor, 0);                                  /* never above the maximum (NFE.1) */
    TEST_ASSERT_EQUAL_UINT16(FREQUENCY_MAX_HZ, sensor.frequencyHz);
}

static void test_ldr_hand_at_35_cm_gives_about_770_hz(void)
{
    DistanceSensor sensor = calibrated_sensor();

    ldr_calculate(&sensor, level_at_distance(35));
    TEST_ASSERT_UINT8_WITHIN(DISTANCE_TOLERANCE_CM, 35, sensor.distanceCm);
    TEST_ASSERT_UINT16_WITHIN(FREQUENCY_TOLERANCE_HZ, 770, sensor.frequencyHz);
}

static void test_ldr_hand_at_10_cm_gives_about_1220_hz(void)
{
    DistanceSensor sensor = calibrated_sensor();

    ldr_calculate(&sensor, level_at_distance(10));
    TEST_ASSERT_UINT8_WITHIN(DISTANCE_TOLERANCE_CM, 10, sensor.distanceCm);
    TEST_ASSERT_UINT16_WITHIN(FREQUENCY_TOLERANCE_HZ, 1220, sensor.frequencyHz);
}

static void test_ldr_no_result_during_calibration(void)
{
    DistanceSensor sensor;
    MedianFilter filter;

    filter_init(&filter, FILTER_SIZE_MIN, LEVEL_AMBIENT);
    ldr_init(&sensor);

    TEST_ASSERT_FALSE(feed_level(&sensor, &filter, LEVEL_AMBIENT, CALIBRATION_TIME_MS / 2));
    TEST_ASSERT_EQUAL(LDR_STATE_CALIBRATE, sensor.state);
}

static void test_ldr_calibration_uses_darkest_and_lightest_reading(void)
{
    DistanceSensor sensor;
    MedianFilter filter;

    filter_init(&filter, FILTER_SIZE_MIN, LEVEL_AMBIENT);
    ldr_init(&sensor);
    feed_level(&sensor, &filter, LEVEL_DARK, CALIBRATION_TIME_MS / 2);
    feed_level(&sensor, &filter, LEVEL_LIGHT, CALIBRATION_TIME_MS / 2 + SAMPLE_INTERVAL_MS);
    TEST_ASSERT_EQUAL(LDR_STATE_MEASURE, sensor.state);

    TEST_ASSERT_TRUE(feed_level(&sensor, &filter, LEVEL_DARK, SAMPLE_INTERVAL_MS));
    TEST_ASSERT_EQUAL_UINT16(FREQUENCY_MAX_HZ, sensor.frequencyHz);
    TEST_ASSERT_TRUE(feed_level(&sensor, &filter, LEVEL_LIGHT, SAMPLE_INTERVAL_MS));
    TEST_ASSERT_EQUAL_UINT16(FREQUENCY_MIN_HZ, sensor.frequencyHz);
}

static void test_ldr_failed_calibration_still_gives_valid_tone(void)
{
    DistanceSensor sensor;
    MedianFilter filter;

    filter_init(&filter, FILTER_SIZE_MIN, LEVEL_AMBIENT);
    ldr_init(&sensor);
    feed_level(&sensor, &filter, LEVEL_AMBIENT, CALIBRATION_TIME_MS + SAMPLE_INTERVAL_MS);
    TEST_ASSERT_EQUAL(LDR_STATE_MEASURE, sensor.state);

    TEST_ASSERT_TRUE(feed_level(&sensor, &filter, LEVEL_AMBIENT, SAMPLE_INTERVAL_MS));
    TEST_ASSERT_TRUE(sensor.frequencyHz >= FREQUENCY_MIN_HZ && sensor.frequencyHz <= FREQUENCY_MAX_HZ);
}

static void test_ldr_seconds_left_counts_down(void)
{
    DistanceSensor sensor = { .state = LDR_STATE_CALIBRATE, .calibrationStartMs = 1000 };

    TEST_ASSERT_EQUAL_UINT8(5, ldr_calibration_seconds_left(&sensor, 1000));
    TEST_ASSERT_EQUAL_UINT8(5, ldr_calibration_seconds_left(&sensor, 1001));   /* rounds up */
    TEST_ASSERT_EQUAL_UINT8(1, ldr_calibration_seconds_left(&sensor, 5500));
    TEST_ASSERT_EQUAL_UINT8(0, ldr_calibration_seconds_left(&sensor, 6000));
}

void run_ldr_tests(void)
{
    RUN_TEST(test_ldr_no_hand_gives_max_distance_and_lowest_tone);
    RUN_TEST(test_ldr_reading_above_range_is_clamped);
    RUN_TEST(test_ldr_hand_on_sensor_gives_highest_tone);
    RUN_TEST(test_ldr_hand_at_35_cm_gives_about_770_hz);
    RUN_TEST(test_ldr_hand_at_10_cm_gives_about_1220_hz);
    RUN_TEST(test_ldr_no_result_during_calibration);
    RUN_TEST(test_ldr_calibration_uses_darkest_and_lightest_reading);
    RUN_TEST(test_ldr_failed_calibration_still_gives_valid_tone);
    RUN_TEST(test_ldr_seconds_left_counts_down);
}
