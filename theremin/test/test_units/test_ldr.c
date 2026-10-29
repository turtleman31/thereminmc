#include <math.h>
#include <unity.h>

#include "buzzer.h"
#include "fakes.h"
#include "filter.h"
#include "ldr.h"
#include "tests.h"
#include "volume.h"

#define DISTANCE_MAX_CM         20U         /* range of the LDR shadow (test finding) */
#define CALIBRATION_TIME_MS     5000U
#define SAMPLE_INTERVAL_MS      20U
#define LEVEL_DARK              (300U * LDR_LEVEL_SCALE)    /* hand on the sensor */
#define LEVEL_LIGHT             (800U * LDR_LEVEL_SCALE)    /* no hand */
#define LEVEL_AMBIENT           (700U * LDR_LEVEL_SCALE)
#define FREQUENCY_TOLERANCE_HZ  20U
#define DISTANCE_TOLERANCE_CM   1U
#define FREQUENCY_SPAN_HZ       (FREQUENCY_MAX_HZ - FREQUENCY_MIN_HZ)
#define HALF_CM_TOLERANCE_HZ    (FREQUENCY_SPAN_HZ / (2 * DISTANCE_MAX_CM) + 1)  /* distance is shown in whole cm */
#define LEVEL_STEPS             (1024.0f * LDR_LEVEL_SCALE)
#define LDR_GAMMA               0.7f        /* shadow model, see ldr.c */
#define HAND_RADIUS_CM          6.0f
#define NO_HAND_MARGIN          (LDR_LEVEL_SCALE / 2)

static DistanceSensor calibrated_sensor(void)
{
    DistanceSensor sensor = { .state = LDR_STATE_MEASURE, .adcMin = LEVEL_DARK, .adcMax = LEVEL_LIGHT };

    return sensor;
}

/* The shadow model of ldr.c in the forward direction: the reading with the hand at distanceCm. */
static uint16_t level_at_distance(float distanceCm)
{
    float noHandLevel = LEVEL_LIGHT - NO_HAND_MARGIN;
    float noHandResistance = (LEVEL_STEPS - noHandLevel) / noHandLevel;    /* relative to the 10 kOhm resistor */
    float darkResistance = (LEVEL_STEPS - LEVEL_DARK) / LEVEL_DARK;
    float darkest = powf(noHandResistance / darkResistance, 1 / LDR_GAMMA);
    float squared = distanceCm * distanceCm;
    float light = darkest + (1 - darkest) * squared / (HAND_RADIUS_CM * HAND_RADIUS_CM + squared);
    float resistance = noHandResistance * powf(light, -LDR_GAMMA);

    return (uint16_t)lroundf(LEVEL_STEPS / (1 + resistance));
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

    ldr_calculate(&sensor, LDR_LEVEL_MAX);
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

static void test_ldr_hand_just_above_sensor_reads_a_few_cm(void)
{
    DistanceSensor sensor = calibrated_sensor();

    ldr_calculate(&sensor, level_at_distance(2));
    TEST_ASSERT_UINT8_WITHIN(DISTANCE_TOLERANCE_CM, 2, sensor.distanceCm);
}

static void test_ldr_hand_at_15_cm_gives_about_523_hz(void)
{
    DistanceSensor sensor = calibrated_sensor();

    ldr_calculate(&sensor, level_at_distance(15));
    TEST_ASSERT_UINT8_WITHIN(DISTANCE_TOLERANCE_CM, 15, sensor.distanceCm);
    TEST_ASSERT_UINT16_WITHIN(FREQUENCY_TOLERANCE_HZ, 523, sensor.frequencyHz);
}

static void test_ldr_hand_at_18_cm_is_not_no_hand(void)
{
    DistanceSensor sensor = calibrated_sensor();

    ldr_calculate(&sensor, level_at_distance(18));
    TEST_ASSERT_UINT8_WITHIN(DISTANCE_TOLERANCE_CM, 18, sensor.distanceCm);
}

static void test_ldr_hand_beyond_range_reads_max_distance(void)
{
    DistanceSensor sensor = calibrated_sensor();

    ldr_calculate(&sensor, level_at_distance(35));
    TEST_ASSERT_EQUAL_UINT8(DISTANCE_MAX_CM, sensor.distanceCm);
    TEST_ASSERT_EQUAL_UINT16(FREQUENCY_MIN_HZ, sensor.frequencyHz);
}

static void test_ldr_noise_just_below_no_hand_reads_max_distance(void)
{
    DistanceSensor sensor = calibrated_sensor();

    ldr_calculate(&sensor, LEVEL_LIGHT - NO_HAND_MARGIN);
    TEST_ASSERT_EQUAL_UINT8(DISTANCE_MAX_CM, sensor.distanceCm);
}

static void test_ldr_hand_at_10_cm_gives_about_815_hz(void)
{
    DistanceSensor sensor = calibrated_sensor();

    ldr_calculate(&sensor, level_at_distance(10));
    TEST_ASSERT_UINT8_WITHIN(DISTANCE_TOLERANCE_CM, 10, sensor.distanceCm);
    TEST_ASSERT_UINT16_WITHIN(FREQUENCY_TOLERANCE_HZ, 815, sensor.frequencyHz);
}

static void test_ldr_distance_never_drops_when_it_gets_lighter(void)
{
    DistanceSensor sensor = calibrated_sensor();
    uint8_t previousCm = 0;

    for (uint16_t level = 0; level <= LDR_LEVEL_MAX; level++) {
        ldr_calculate(&sensor, level);
        TEST_ASSERT_TRUE(sensor.distanceCm >= previousCm);
        previousCm = sensor.distanceCm;
    }
}

static void test_ldr_frequency_drops_evenly_with_distance(void)
{
    DistanceSensor sensor = calibrated_sensor();

    for (uint16_t level = 0; level <= LDR_LEVEL_MAX; level++) {
        ldr_calculate(&sensor, level);
        TEST_ASSERT_UINT16_WITHIN(HALF_CM_TOLERANCE_HZ,
                                  FREQUENCY_MAX_HZ - FREQUENCY_SPAN_HZ * sensor.distanceCm / DISTANCE_MAX_CM,
                                  sensor.frequencyHz);
    }
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
    RUN_TEST(test_ldr_hand_just_above_sensor_reads_a_few_cm);
    RUN_TEST(test_ldr_hand_at_10_cm_gives_about_815_hz);
    RUN_TEST(test_ldr_hand_at_15_cm_gives_about_523_hz);
    RUN_TEST(test_ldr_hand_at_18_cm_is_not_no_hand);
    RUN_TEST(test_ldr_hand_beyond_range_reads_max_distance);
    RUN_TEST(test_ldr_noise_just_below_no_hand_reads_max_distance);
    RUN_TEST(test_ldr_distance_never_drops_when_it_gets_lighter);
    RUN_TEST(test_ldr_frequency_drops_evenly_with_distance);
    RUN_TEST(test_ldr_no_result_during_calibration);
    RUN_TEST(test_ldr_calibration_uses_darkest_and_lightest_reading);
    RUN_TEST(test_ldr_failed_calibration_still_gives_valid_tone);
    RUN_TEST(test_ldr_seconds_left_counts_down);
}
