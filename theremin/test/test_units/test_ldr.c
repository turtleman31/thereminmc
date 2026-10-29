#include <math.h>
#include <unity.h>

#include "buzzer.h"
#include "fakes.h"
#include "filter.h"
#include "ldr.h"
#include "tests.h"
#include "volume.h"

#define DISTANCE_MAX_CM         20U         /* reach of the reflection (test finding) */
#define NO_HAND_TIME_MS         2000U
#define CALIBRATION_TIME_MS     5000U
#define SAMPLE_INTERVAL_MS      20U
#define LEVEL_NO_HAND           (500U * LDR_LEVEL_SCALE)    /* lamp light without a hand */
#define LEVEL_REFERENCE         (650U * LDR_LEVEL_SCALE)    /* hand at LDR_REFERENCE_CM */
#define LEVEL_NOISE             (2U * LDR_LEVEL_SCALE)
#define NO_HAND_MARGIN          (LDR_LEVEL_SCALE / 2)
#define LEVEL_STEPS             (1024.0f * LDR_LEVEL_SCALE)
#define LDR_GAMMA               0.7f        /* reflection model, see ldr.c */
#define FREQUENCY_TOLERANCE_HZ  20U
#define DISTANCE_TOLERANCE_CM   1U
#define FREQUENCY_SPAN_HZ       (FREQUENCY_MAX_HZ - FREQUENCY_MIN_HZ)
#define HALF_CM_TOLERANCE_HZ    (FREQUENCY_SPAN_HZ / (2 * DISTANCE_MAX_CM) + 1)  /* distance is shown in whole cm */

static DistanceSensor calibrated_sensor(void)
{
    DistanceSensor sensor = {
        .state = LDR_STATE_MEASURE, .noHandLevel = LEVEL_NO_HAND, .referenceLevel = LEVEL_REFERENCE
    };

    return sensor;
}

/* LDR resistance relative to the 10 kOhm resistor. */
static float resistance(float level)
{
    return (LEVEL_STEPS - level) / level;
}

/* The reflection model of ldr.c in the forward direction: the reading with the hand at distanceCm. */
static uint16_t level_at_distance(float distanceCm)
{
    float referenceLight = powf(resistance(LEVEL_REFERENCE) / resistance(LEVEL_NO_HAND), -1 / LDR_GAMMA);
    float scale = LDR_REFERENCE_CM / distanceCm;
    float light = 1 + (referenceLight - 1) * scale * scale;

    return (uint16_t)lroundf(LEVEL_STEPS / (1 + resistance(LEVEL_NO_HAND) * powf(light, -LDR_GAMMA)));
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

static void start_calibration(DistanceSensor *sensor, MedianFilter *filter)
{
    filter_init(filter, FILTER_SIZE_MIN, 0);
    ldr_init(sensor);
}

static void test_ldr_no_hand_gives_max_distance_and_lowest_tone(void)
{
    DistanceSensor sensor = calibrated_sensor();

    ldr_calculate(&sensor, LEVEL_NO_HAND);
    TEST_ASSERT_EQUAL_UINT8(DISTANCE_MAX_CM, sensor.distanceCm);
    TEST_ASSERT_EQUAL_UINT16(FREQUENCY_MIN_HZ, sensor.frequencyHz);
}

static void test_ldr_darker_than_no_hand_is_clamped(void)
{
    DistanceSensor sensor = calibrated_sensor();

    ldr_calculate(&sensor, 0);
    TEST_ASSERT_EQUAL_UINT8(DISTANCE_MAX_CM, sensor.distanceCm);
    TEST_ASSERT_EQUAL_UINT16(FREQUENCY_MIN_HZ, sensor.frequencyHz);
}

static void test_ldr_noise_just_above_no_hand_reads_max_distance(void)
{
    DistanceSensor sensor = calibrated_sensor();

    ldr_calculate(&sensor, LEVEL_NO_HAND + NO_HAND_MARGIN);
    TEST_ASSERT_EQUAL_UINT8(DISTANCE_MAX_CM, sensor.distanceCm);
}

static void test_ldr_brightest_reading_gives_highest_tone(void)
{
    DistanceSensor sensor = calibrated_sensor();

    ldr_calculate(&sensor, LDR_LEVEL_MAX);
    TEST_ASSERT_EQUAL_UINT8(0, sensor.distanceCm);
    TEST_ASSERT_UINT16_WITHIN(FREQUENCY_TOLERANCE_HZ, FREQUENCY_MAX_HZ, sensor.frequencyHz);
    TEST_ASSERT_TRUE(sensor.frequencyHz <= FREQUENCY_MAX_HZ);   /* NFE.1 */
}

static void test_ldr_hand_at_reference_gives_815_hz(void)
{
    DistanceSensor sensor = calibrated_sensor();

    ldr_calculate(&sensor, LEVEL_REFERENCE);
    TEST_ASSERT_EQUAL_UINT8(LDR_REFERENCE_CM, sensor.distanceCm);
    TEST_ASSERT_UINT16_WITHIN(1, 815, sensor.frequencyHz);
}

static void test_ldr_hand_at_2_cm_reads_2_cm(void)
{
    DistanceSensor sensor = calibrated_sensor();

    ldr_calculate(&sensor, level_at_distance(2));
    TEST_ASSERT_UINT8_WITHIN(DISTANCE_TOLERANCE_CM, 2, sensor.distanceCm);
}

static void test_ldr_hand_at_5_cm_gives_about_1108_hz(void)
{
    DistanceSensor sensor = calibrated_sensor();

    ldr_calculate(&sensor, level_at_distance(5));
    TEST_ASSERT_UINT8_WITHIN(DISTANCE_TOLERANCE_CM, 5, sensor.distanceCm);
    TEST_ASSERT_UINT16_WITHIN(FREQUENCY_TOLERANCE_HZ, 1108, sensor.frequencyHz);
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

    ldr_calculate(&sensor, level_at_distance(40));
    TEST_ASSERT_EQUAL_UINT8(DISTANCE_MAX_CM, sensor.distanceCm);
    TEST_ASSERT_EQUAL_UINT16(FREQUENCY_MIN_HZ, sensor.frequencyHz);
}

static void test_ldr_distance_never_rises_when_it_gets_brighter(void)
{
    DistanceSensor sensor = calibrated_sensor();
    uint8_t previousCm = DISTANCE_MAX_CM;

    for (uint16_t level = 0; level <= LDR_LEVEL_MAX; level++) {
        ldr_calculate(&sensor, level);
        TEST_ASSERT_TRUE(sensor.distanceCm <= previousCm);
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

static void test_ldr_calibration_asks_no_hand_then_reference(void)
{
    DistanceSensor sensor;
    MedianFilter filter;

    start_calibration(&sensor, &filter);
    TEST_ASSERT_FALSE(feed_level(&sensor, &filter, LEVEL_NO_HAND, NO_HAND_TIME_MS));
    TEST_ASSERT_EQUAL(LDR_STATE_CALIBRATE_NO_HAND, sensor.state);
    TEST_ASSERT_FALSE(feed_level(&sensor, &filter, LEVEL_REFERENCE, SAMPLE_INTERVAL_MS));
    TEST_ASSERT_EQUAL(LDR_STATE_CALIBRATE_REFERENCE, sensor.state);
}

static void test_ldr_calibration_uses_no_hand_and_reference_reading(void)
{
    DistanceSensor sensor;
    MedianFilter filter;

    start_calibration(&sensor, &filter);
    feed_level(&sensor, &filter, LEVEL_NO_HAND, NO_HAND_TIME_MS);
    TEST_ASSERT_TRUE(feed_level(&sensor, &filter, LEVEL_REFERENCE,
                                CALIBRATION_TIME_MS - NO_HAND_TIME_MS + SAMPLE_INTERVAL_MS));
    TEST_ASSERT_EQUAL(LDR_STATE_MEASURE, sensor.state);
    TEST_ASSERT_EQUAL_UINT8(LDR_REFERENCE_CM, sensor.distanceCm);

    TEST_ASSERT_TRUE(feed_level(&sensor, &filter, LEVEL_NO_HAND, SAMPLE_INTERVAL_MS));
    TEST_ASSERT_EQUAL_UINT16(FREQUENCY_MIN_HZ, sensor.frequencyHz);
}

static void test_ldr_noise_without_hand_is_not_seen_as_hand(void)
{
    DistanceSensor sensor;
    MedianFilter filter;

    start_calibration(&sensor, &filter);
    feed_level(&sensor, &filter, LEVEL_NO_HAND, NO_HAND_TIME_MS / 2);
    feed_level(&sensor, &filter, LEVEL_NO_HAND + LEVEL_NOISE, NO_HAND_TIME_MS / 2);
    feed_level(&sensor, &filter, LEVEL_REFERENCE, CALIBRATION_TIME_MS - NO_HAND_TIME_MS + SAMPLE_INTERVAL_MS);

    TEST_ASSERT_TRUE(feed_level(&sensor, &filter, LEVEL_NO_HAND + LEVEL_NOISE, SAMPLE_INTERVAL_MS));
    TEST_ASSERT_EQUAL_UINT8(DISTANCE_MAX_CM, sensor.distanceCm);
}

static void test_ldr_failed_calibration_still_gives_valid_tone(void)
{
    DistanceSensor sensor;
    MedianFilter filter;

    start_calibration(&sensor, &filter);
    feed_level(&sensor, &filter, LEVEL_NO_HAND, CALIBRATION_TIME_MS + SAMPLE_INTERVAL_MS);
    TEST_ASSERT_EQUAL(LDR_STATE_MEASURE, sensor.state);

    TEST_ASSERT_TRUE(feed_level(&sensor, &filter, LEVEL_NO_HAND, SAMPLE_INTERVAL_MS));
    TEST_ASSERT_EQUAL_UINT16(FREQUENCY_MIN_HZ, sensor.frequencyHz);
    TEST_ASSERT_TRUE(feed_level(&sensor, &filter, LEVEL_REFERENCE, SAMPLE_INTERVAL_MS));
    TEST_ASSERT_TRUE(sensor.frequencyHz >= FREQUENCY_MIN_HZ && sensor.frequencyHz <= FREQUENCY_MAX_HZ);
}

static void test_ldr_seconds_left_counts_down(void)
{
    DistanceSensor sensor = { .state = LDR_STATE_CALIBRATE_NO_HAND, .calibrationStartMs = 1000 };

    TEST_ASSERT_EQUAL_UINT8(5, ldr_calibration_seconds_left(&sensor, 1000));
    TEST_ASSERT_EQUAL_UINT8(5, ldr_calibration_seconds_left(&sensor, 1001));   /* rounds up */
    sensor.state = LDR_STATE_CALIBRATE_REFERENCE;
    TEST_ASSERT_EQUAL_UINT8(1, ldr_calibration_seconds_left(&sensor, 5500));
    TEST_ASSERT_EQUAL_UINT8(0, ldr_calibration_seconds_left(&sensor, 6000));
}

void run_ldr_tests(void)
{
    RUN_TEST(test_ldr_no_hand_gives_max_distance_and_lowest_tone);
    RUN_TEST(test_ldr_darker_than_no_hand_is_clamped);
    RUN_TEST(test_ldr_noise_just_above_no_hand_reads_max_distance);
    RUN_TEST(test_ldr_brightest_reading_gives_highest_tone);
    RUN_TEST(test_ldr_hand_at_reference_gives_815_hz);
    RUN_TEST(test_ldr_hand_at_2_cm_reads_2_cm);
    RUN_TEST(test_ldr_hand_at_5_cm_gives_about_1108_hz);
    RUN_TEST(test_ldr_hand_at_15_cm_gives_about_523_hz);
    RUN_TEST(test_ldr_hand_at_18_cm_is_not_no_hand);
    RUN_TEST(test_ldr_hand_beyond_range_reads_max_distance);
    RUN_TEST(test_ldr_distance_never_rises_when_it_gets_brighter);
    RUN_TEST(test_ldr_frequency_drops_evenly_with_distance);
    RUN_TEST(test_ldr_calibration_asks_no_hand_then_reference);
    RUN_TEST(test_ldr_calibration_uses_no_hand_and_reference_reading);
    RUN_TEST(test_ldr_noise_without_hand_is_not_seen_as_hand);
    RUN_TEST(test_ldr_failed_calibration_still_gives_valid_tone);
    RUN_TEST(test_ldr_seconds_left_counts_down);
}
