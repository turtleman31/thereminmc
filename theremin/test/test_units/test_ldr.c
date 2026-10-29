#include <math.h>
#include <unity.h>

#include "buzzer.h"
#include "fakes.h"
#include "filter.h"
#include "ldr.h"
#include "storage.h"
#include "tests.h"
#include "volume.h"

#define DISTANCE_MAX_CM         20U         /* the last calibration point */
#define NO_HAND_TIME_MS         2000U
#define POINT_TIME_MS           3000U
#define SAMPLE_INTERVAL_MS      20U
#define LEVEL_NO_HAND           (500U * LDR_LEVEL_SCALE)    /* lamp and room light without a hand */
#define LEVEL_NOISE             (2U * LDR_LEVEL_SCALE)
#define NO_HAND_MARGIN          (LDR_LEVEL_SCALE / 2)
#define LEVEL_STEPS             (1024.0f * LDR_LEVEL_SCALE)
#define LDR_GAMMA               0.7f        /* light model, see ldr.c */
#define REFLECTION_AT_5_CM      2.0f
#define BARE_LAMP               2.0f        /* reflection falls off as distance^-2 */
#define FLASHLIGHT              1.2f        /* a reflector bundles the light, so it falls off slower */
#define FREQUENCY_TOLERANCE_HZ  20U
#define DISTANCE_TOLERANCE_CM   1U
#define FREQUENCY_SPAN_HZ       (FREQUENCY_MAX_HZ - FREQUENCY_MIN_HZ)
#define HALF_CM_TOLERANCE_HZ    (FREQUENCY_SPAN_HZ / (2 * DISTANCE_MAX_CM) + 1)  /* distance is shown in whole cm */

/* Light on the LDR as in ldr.c: 1 = the light at which the LDR equals the 10 kOhm resistor. */
static float light(float level)
{
    return powf((LEVEL_STEPS - level) / level, -1 / LDR_GAMMA);
}

static float reflection_at(float distanceCm, float falloff)
{
    return REFLECTION_AT_5_CM * powf(LDR_CALIBRATION_STEP_CM / distanceCm, falloff);
}

/* The reading with the hand at distanceCm above a lamp whose reflection falls off as distance^-falloff. */
static uint16_t level_at(float distanceCm, float falloff)
{
    float lightOnLdr = light(LEVEL_NO_HAND) + reflection_at(distanceCm, falloff);

    return (uint16_t)lroundf(LEVEL_STEPS / (1 + powf(lightOnLdr, -LDR_GAMMA)));
}

static float point_cm(uint8_t point)
{
    return (point + 1) * LDR_CALIBRATION_STEP_CM;
}

static DistanceSensor calibrated_sensor(float falloff)
{
    DistanceSensor sensor = { .state = LDR_STATE_MEASURE, .noHandLevel = LEVEL_NO_HAND };

    for (uint8_t point = 0; point < LDR_CALIBRATION_POINTS; point++) {
        sensor.reflection[point] = reflection_at(point_cm(point), falloff);
    }
    return sensor;
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

static void start_sensor(DistanceSensor *sensor, MedianFilter *filter)
{
    filter_init(filter, FILTER_SIZE_MIN, 0);
    ldr_init(sensor);
}

/* No hand for 2 s, then the hand at every calibration point; returns the result of the last ldr_update(). */
static bool calibrate_all_points(DistanceSensor *sensor, MedianFilter *filter, float falloff)
{
    bool updated = false;

    feed_level(sensor, filter, LEVEL_NO_HAND, NO_HAND_TIME_MS + SAMPLE_INTERVAL_MS);
    for (uint8_t point = 0; point < LDR_CALIBRATION_POINTS; point++) {
        updated = feed_level(sensor, filter, level_at(point_cm(point), falloff), POINT_TIME_MS);
    }
    return updated;
}

static void test_ldr_no_hand_gives_max_distance_and_lowest_tone(void)
{
    DistanceSensor sensor = calibrated_sensor(FLASHLIGHT);

    ldr_calculate(&sensor, LEVEL_NO_HAND);
    TEST_ASSERT_EQUAL_UINT8(DISTANCE_MAX_CM, sensor.distanceCm);
    TEST_ASSERT_EQUAL_UINT16(FREQUENCY_MIN_HZ, sensor.frequencyHz);
}

static void test_ldr_darker_than_no_hand_is_clamped(void)
{
    DistanceSensor sensor = calibrated_sensor(FLASHLIGHT);

    ldr_calculate(&sensor, 0);
    TEST_ASSERT_EQUAL_UINT8(DISTANCE_MAX_CM, sensor.distanceCm);
    TEST_ASSERT_EQUAL_UINT16(FREQUENCY_MIN_HZ, sensor.frequencyHz);
}

static void test_ldr_noise_just_above_no_hand_reads_max_distance(void)
{
    DistanceSensor sensor = calibrated_sensor(FLASHLIGHT);

    ldr_calculate(&sensor, LEVEL_NO_HAND + NO_HAND_MARGIN);
    TEST_ASSERT_EQUAL_UINT8(DISTANCE_MAX_CM, sensor.distanceCm);
}

static void test_ldr_brightest_reading_gives_highest_tone(void)
{
    DistanceSensor sensor = calibrated_sensor(FLASHLIGHT);

    ldr_calculate(&sensor, LDR_LEVEL_MAX);
    TEST_ASSERT_EQUAL_UINT8(0, sensor.distanceCm);
    TEST_ASSERT_UINT16_WITHIN(FREQUENCY_TOLERANCE_HZ, FREQUENCY_MAX_HZ, sensor.frequencyHz);
    TEST_ASSERT_TRUE(sensor.frequencyHz <= FREQUENCY_MAX_HZ);   /* NFE.1 */
}

static void test_ldr_each_calibration_point_reads_its_height(void)
{
    DistanceSensor sensor = calibrated_sensor(FLASHLIGHT);

    for (uint8_t point = 0; point < LDR_CALIBRATION_POINTS; point++) {
        ldr_calculate(&sensor, level_at(point_cm(point), FLASHLIGHT));
        TEST_ASSERT_UINT8_WITHIN(DISTANCE_TOLERANCE_CM, point_cm(point), sensor.distanceCm);
    }
}

static void test_ldr_between_points_follows_the_flashlight(void)
{
    static const uint8_t betweenCm[] = { 7, 12, 17 };
    DistanceSensor sensor = calibrated_sensor(FLASHLIGHT);

    for (uint8_t i = 0; i < sizeof betweenCm; i++) {
        ldr_calculate(&sensor, level_at(betweenCm[i], FLASHLIGHT));
        TEST_ASSERT_UINT8_WITHIN(DISTANCE_TOLERANCE_CM, betweenCm[i], sensor.distanceCm);
    }
}

static void test_ldr_hand_at_10_cm_gives_about_815_hz(void)
{
    DistanceSensor sensor = calibrated_sensor(FLASHLIGHT);

    ldr_calculate(&sensor, level_at(10, FLASHLIGHT));
    TEST_ASSERT_UINT16_WITHIN(FREQUENCY_TOLERANCE_HZ, 815, sensor.frequencyHz);
}

static void test_ldr_below_first_point_follows_inverse_square_law(void)
{
    DistanceSensor sensor = calibrated_sensor(BARE_LAMP);

    ldr_calculate(&sensor, level_at(2, BARE_LAMP));
    TEST_ASSERT_UINT8_WITHIN(DISTANCE_TOLERANCE_CM, 2, sensor.distanceCm);
}

static void test_ldr_hand_beyond_last_point_reads_max_distance(void)
{
    DistanceSensor sensor = calibrated_sensor(FLASHLIGHT);

    ldr_calculate(&sensor, level_at(30, FLASHLIGHT));
    TEST_ASSERT_EQUAL_UINT8(DISTANCE_MAX_CM, sensor.distanceCm);
    TEST_ASSERT_EQUAL_UINT16(FREQUENCY_MIN_HZ, sensor.frequencyHz);
}

static void test_ldr_distance_never_rises_when_it_gets_brighter(void)
{
    DistanceSensor sensor = calibrated_sensor(FLASHLIGHT);
    uint8_t previousCm = DISTANCE_MAX_CM;

    for (uint16_t level = 0; level <= LDR_LEVEL_MAX; level++) {
        ldr_calculate(&sensor, level);
        TEST_ASSERT_TRUE(sensor.distanceCm <= previousCm);
        previousCm = sensor.distanceCm;
    }
}

static void test_ldr_frequency_drops_evenly_with_distance(void)
{
    DistanceSensor sensor = calibrated_sensor(FLASHLIGHT);

    for (uint16_t level = 0; level <= LDR_LEVEL_MAX; level++) {
        ldr_calculate(&sensor, level);
        TEST_ASSERT_UINT16_WITHIN(HALF_CM_TOLERANCE_HZ,
                                  FREQUENCY_MAX_HZ - FREQUENCY_SPAN_HZ * sensor.distanceCm / DISTANCE_MAX_CM,
                                  sensor.frequencyHz);
    }
}

static void test_ldr_first_start_asks_no_hand_then_each_height(void)
{
    DistanceSensor sensor;
    MedianFilter filter;

    fakePointsStored = false;
    start_sensor(&sensor, &filter);
    TEST_ASSERT_FALSE(feed_level(&sensor, &filter, LEVEL_NO_HAND, NO_HAND_TIME_MS));
    TEST_ASSERT_EQUAL(LDR_STATE_CALIBRATE_NO_HAND, sensor.state);
    TEST_ASSERT_EQUAL_UINT8(0, ldr_calibration_height_cm(&sensor));

    feed_level(&sensor, &filter, LEVEL_NO_HAND, SAMPLE_INTERVAL_MS);
    for (uint8_t point = 0; point < LDR_CALIBRATION_POINTS; point++) {
        TEST_ASSERT_EQUAL(LDR_STATE_CALIBRATE_POINT, sensor.state);
        TEST_ASSERT_EQUAL_UINT8(point_cm(point), ldr_calibration_height_cm(&sensor));
        feed_level(&sensor, &filter, level_at(point_cm(point), FLASHLIGHT), POINT_TIME_MS);
    }
    TEST_ASSERT_EQUAL(LDR_STATE_MEASURE, sensor.state);
}

static void test_ldr_calibration_is_stored(void)
{
    DistanceSensor sensor;
    MedianFilter filter;

    fakePointsStored = false;
    start_sensor(&sensor, &filter);
    TEST_ASSERT_TRUE(calibrate_all_points(&sensor, &filter, FLASHLIGHT));
    TEST_ASSERT_TRUE(fakePointsStored);
    for (uint8_t point = 0; point < LDR_CALIBRATION_POINTS; point++) {
        float expected = reflection_at(point_cm(point), FLASHLIGHT);

        TEST_ASSERT_FLOAT_WITHIN(expected / 100, expected, fakeStoredReflection[point]);
    }
    TEST_ASSERT_TRUE(feed_level(&sensor, &filter, level_at(12, FLASHLIGHT), SAMPLE_INTERVAL_MS));
    TEST_ASSERT_UINT8_WITHIN(DISTANCE_TOLERANCE_CM, 12, sensor.distanceCm);
}

static void test_ldr_next_start_only_needs_no_hand(void)
{
    DistanceSensor stored = calibrated_sensor(FLASHLIGHT);
    DistanceSensor sensor;
    MedianFilter filter;

    storage_save_points(stored.reflection);
    start_sensor(&sensor, &filter);
    TEST_ASSERT_TRUE(feed_level(&sensor, &filter, LEVEL_NO_HAND, NO_HAND_TIME_MS + SAMPLE_INTERVAL_MS));
    TEST_ASSERT_EQUAL(LDR_STATE_MEASURE, sensor.state);

    TEST_ASSERT_TRUE(feed_level(&sensor, &filter, level_at(10, FLASHLIGHT), SAMPLE_INTERVAL_MS));
    TEST_ASSERT_UINT8_WITHIN(DISTANCE_TOLERANCE_CM, 10, sensor.distanceCm);
}

static void test_ldr_recalibrate_measures_all_heights_again(void)
{
    DistanceSensor stored = calibrated_sensor(FLASHLIGHT);
    DistanceSensor sensor;
    MedianFilter filter;

    storage_save_points(stored.reflection);
    start_sensor(&sensor, &filter);
    feed_level(&sensor, &filter, LEVEL_NO_HAND, NO_HAND_TIME_MS + SAMPLE_INTERVAL_MS);

    ldr_recalibrate(&sensor);
    TEST_ASSERT_EQUAL(LDR_STATE_CALIBRATE_NO_HAND, sensor.state);
    TEST_ASSERT_FALSE(feed_level(&sensor, &filter, LEVEL_NO_HAND, NO_HAND_TIME_MS + SAMPLE_INTERVAL_MS));
    TEST_ASSERT_EQUAL(LDR_STATE_CALIBRATE_POINT, sensor.state);
}

static void test_ldr_noise_without_hand_is_not_seen_as_hand(void)
{
    DistanceSensor stored = calibrated_sensor(FLASHLIGHT);
    DistanceSensor sensor;
    MedianFilter filter;

    storage_save_points(stored.reflection);
    start_sensor(&sensor, &filter);
    feed_level(&sensor, &filter, LEVEL_NO_HAND, NO_HAND_TIME_MS / 2);
    feed_level(&sensor, &filter, LEVEL_NO_HAND + LEVEL_NOISE, NO_HAND_TIME_MS / 2 + SAMPLE_INTERVAL_MS);

    TEST_ASSERT_TRUE(feed_level(&sensor, &filter, LEVEL_NO_HAND + LEVEL_NOISE, SAMPLE_INTERVAL_MS));
    TEST_ASSERT_EQUAL_UINT8(DISTANCE_MAX_CM, sensor.distanceCm);
}

static void test_ldr_calibration_without_hand_still_gives_valid_tone(void)
{
    DistanceSensor sensor;
    MedianFilter filter;

    fakePointsStored = false;
    start_sensor(&sensor, &filter);
    feed_level(&sensor, &filter, LEVEL_NO_HAND,
               NO_HAND_TIME_MS + LDR_CALIBRATION_POINTS * POINT_TIME_MS + SAMPLE_INTERVAL_MS);
    TEST_ASSERT_EQUAL(LDR_STATE_MEASURE, sensor.state);
    for (uint8_t point = 1; point < LDR_CALIBRATION_POINTS; point++) {
        TEST_ASSERT_TRUE(sensor.reflection[point] > 0 && sensor.reflection[point] < sensor.reflection[point - 1]);
    }

    TEST_ASSERT_TRUE(feed_level(&sensor, &filter, LEVEL_NO_HAND, SAMPLE_INTERVAL_MS));
    TEST_ASSERT_EQUAL_UINT16(FREQUENCY_MIN_HZ, sensor.frequencyHz);
    TEST_ASSERT_TRUE(feed_level(&sensor, &filter, level_at(10, FLASHLIGHT), SAMPLE_INTERVAL_MS));
    TEST_ASSERT_TRUE(sensor.frequencyHz >= FREQUENCY_MIN_HZ && sensor.frequencyHz <= FREQUENCY_MAX_HZ);
}

static void test_ldr_seconds_left_counts_down_per_step(void)
{
    DistanceSensor sensor = { .state = LDR_STATE_CALIBRATE_NO_HAND, .stepStartMs = 1000 };

    TEST_ASSERT_EQUAL_UINT8(2, ldr_calibration_seconds_left(&sensor, 1000));
    TEST_ASSERT_EQUAL_UINT8(2, ldr_calibration_seconds_left(&sensor, 1001));   /* rounds up */
    TEST_ASSERT_EQUAL_UINT8(1, ldr_calibration_seconds_left(&sensor, 2500));
    TEST_ASSERT_EQUAL_UINT8(0, ldr_calibration_seconds_left(&sensor, 3000));

    sensor.state = LDR_STATE_CALIBRATE_POINT;
    TEST_ASSERT_EQUAL_UINT8(3, ldr_calibration_seconds_left(&sensor, 1000));
    TEST_ASSERT_EQUAL_UINT8(1, ldr_calibration_seconds_left(&sensor, 3500));
}

void run_ldr_tests(void)
{
    RUN_TEST(test_ldr_no_hand_gives_max_distance_and_lowest_tone);
    RUN_TEST(test_ldr_darker_than_no_hand_is_clamped);
    RUN_TEST(test_ldr_noise_just_above_no_hand_reads_max_distance);
    RUN_TEST(test_ldr_brightest_reading_gives_highest_tone);
    RUN_TEST(test_ldr_each_calibration_point_reads_its_height);
    RUN_TEST(test_ldr_between_points_follows_the_flashlight);
    RUN_TEST(test_ldr_hand_at_10_cm_gives_about_815_hz);
    RUN_TEST(test_ldr_below_first_point_follows_inverse_square_law);
    RUN_TEST(test_ldr_hand_beyond_last_point_reads_max_distance);
    RUN_TEST(test_ldr_distance_never_rises_when_it_gets_brighter);
    RUN_TEST(test_ldr_frequency_drops_evenly_with_distance);
    RUN_TEST(test_ldr_first_start_asks_no_hand_then_each_height);
    RUN_TEST(test_ldr_calibration_is_stored);
    RUN_TEST(test_ldr_next_start_only_needs_no_hand);
    RUN_TEST(test_ldr_recalibrate_measures_all_heights_again);
    RUN_TEST(test_ldr_noise_without_hand_is_not_seen_as_hand);
    RUN_TEST(test_ldr_calibration_without_hand_still_gives_valid_tone);
    RUN_TEST(test_ldr_seconds_left_counts_down_per_step);
}
