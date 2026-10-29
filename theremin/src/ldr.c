/*
 * Distance sensor: a lamp next to the LDR shines up and the hand reflects its
 * light into the LDR. The reflection is measured once with the hand at 5, 10,
 * 15 and 20 cm and kept in the EEPROM; at every start only the light without a
 * hand is measured. Between the points the distance is interpolated linearly in
 * 1 / sqrt(reflection), which is exact for a lamp that follows the inverse square law.
 */

#include "ldr.h"

#include <math.h>

#include "buzzer.h"
#include "storage.h"
#include "tick.h"
#include "volume.h"

#define LDR_SAMPLE_INTERVAL_MS  20U         /* filter window = filter size * interval */
#define NO_HAND_TIME_MS         2000U
#define POINT_TIME_MS           3000U
#define MIN_REFLECTION_RISE     (10U * LDR_LEVEL_SCALE)     /* assumed when the hand reflected (almost) nothing */
#define DISTANCE_MAX_CM         (LDR_CALIBRATION_POINTS * LDR_CALIBRATION_STEP_CM)
#define LEVEL_STEPS             ((ADC_VALUE_MAX + 1.0f) * LDR_LEVEL_SCALE)
#define LDR_GAMMA               0.7f        /* GL5537 datasheet */
#define NO_HAND_MARGIN          (LDR_LEVEL_SCALE / 2)       /* half an ADC step of noise */

void ldr_init(DistanceSensor *sensor)
{
    sensor->state = LDR_STATE_INIT;
    sensor->lastSampleMs = 0;
}

static bool ldr_is_calibrating(const DistanceSensor *sensor)
{
    return sensor->state == LDR_STATE_CALIBRATE_NO_HAND || sensor->state == LDR_STATE_CALIBRATE_POINT;
}

/* Light on the LDR, 1 = the light at which the LDR equals the 10 kOhm resistor. */
static float ldr_light(uint16_t level)
{
    return powf((LEVEL_STEPS - level) / level, -1 / LDR_GAMMA);  /* divider 5V - LDR - ADC - 10 kOhm - GND */
}

static void ldr_start_calibration(DistanceSensor *sensor, uint32_t now, bool allPoints)
{
    sensor->calibratePoints = allPoints || !storage_load_points(sensor->reflection);
    sensor->noHandLevel = 0;
    sensor->stepStartMs = now;
    sensor->state = LDR_STATE_CALIBRATE_NO_HAND;
}

void ldr_recalibrate(DistanceSensor *sensor)
{
    ldr_start_calibration(sensor, tick_ms(), true);
}

/* Each point must reflect less than the one before it; a bad point is filled in with the inverse square law. */
static void ldr_repair_points(DistanceSensor *sensor)
{
    float *reflection = sensor->reflection;
    uint16_t risenLevel = (sensor->noHandLevel < LDR_LEVEL_MAX - MIN_REFLECTION_RISE)
        ? sensor->noHandLevel + MIN_REFLECTION_RISE : LDR_LEVEL_MAX;
    float minimum = ldr_light(risenLevel) - ldr_light(sensor->noHandLevel);

    if (!(reflection[0] >= minimum)) {
        reflection[0] = minimum;
    }
    for (uint8_t point = 1; point < LDR_CALIBRATION_POINTS; point++) {
        float ratio = (float)point / (point + 1);           /* previous height / this height */

        if (!(reflection[point] > 0 && reflection[point] < reflection[point - 1])) {
            reflection[point] = reflection[point - 1] * ratio * ratio;
        }
    }
}

/* Returns true when the calibration is finished. */
static bool ldr_calibrate(DistanceSensor *sensor, uint16_t level, uint16_t filtered, uint32_t now)
{
    uint32_t elapsed = now - sensor->stepStartMs;

    if (sensor->state == LDR_STATE_CALIBRATE_NO_HAND) {
        if (level > sensor->noHandLevel) {
            sensor->noHandLevel = level;        /* the highest reading, so noise does not look like a hand */
        }
        if (elapsed < NO_HAND_TIME_MS) {
            return false;
        }
        if (sensor->calibratePoints) {
            sensor->state = LDR_STATE_CALIBRATE_POINT;
            sensor->point = 0;
            sensor->stepStartMs = now;
            return false;
        }
    } else {
        if (elapsed < POINT_TIME_MS) {
            return false;
        }
        sensor->reflection[sensor->point] = ldr_light(filtered) - ldr_light(sensor->noHandLevel);
        sensor->stepStartMs = now;
        if (++sensor->point < LDR_CALIBRATION_POINTS) {
            return false;
        }
        ldr_repair_points(sensor);
        storage_save_points(sensor->reflection);
    }
    sensor->state = LDR_STATE_MEASURE;
    return true;
}

/* From 0 cm to the first point and between the points: linear in 1 / sqrt(reflection). */
static float ldr_distance(const float *reflection, float measured)
{
    float x = 1 / sqrtf(measured);
    float previousX = 0;
    float previousCm = 0;

    for (uint8_t point = 0; point < LDR_CALIBRATION_POINTS; point++) {
        float pointX = 1 / sqrtf(reflection[point]);
        float pointCm = (point + 1) * LDR_CALIBRATION_STEP_CM;

        if (x <= pointX) {
            return previousCm + (pointCm - previousCm) * (x - previousX) / (pointX - previousX);
        }
        previousX = pointX;
        previousCm = pointCm;
    }
    return DISTANCE_MAX_CM;
}

/* No hand = DISTANCE_MAX_CM and 230 Hz; a closer hand reflects more light and gives a higher tone. */
void ldr_calculate(DistanceSensor *sensor, uint16_t level)
{
    float distanceCm = DISTANCE_MAX_CM;

    if (level > sensor->noHandLevel + NO_HAND_MARGIN) {
        distanceCm = ldr_distance(sensor->reflection, ldr_light(level) - ldr_light(sensor->noHandLevel));
    }
    sensor->distanceCm = (uint8_t)lroundf(distanceCm);
    sensor->frequencyHz = FREQUENCY_MAX_HZ
        - (uint16_t)lroundf((FREQUENCY_MAX_HZ - FREQUENCY_MIN_HZ) * distanceCm / DISTANCE_MAX_CM);
}

uint8_t ldr_calibration_height_cm(const DistanceSensor *sensor)
{
    return (sensor->state == LDR_STATE_CALIBRATE_POINT) ? (sensor->point + 1) * LDR_CALIBRATION_STEP_CM : 0;
}

/* Seconds left in the current calibration step. */
uint8_t ldr_calibration_seconds_left(const DistanceSensor *sensor, uint32_t now)
{
    uint32_t stepMs = (sensor->state == LDR_STATE_CALIBRATE_POINT) ? POINT_TIME_MS : NO_HAND_TIME_MS;
    uint32_t elapsed = ldr_is_calibrating(sensor) ? now - sensor->stepStartMs : 0;

    if (elapsed >= stepMs) {
        return 0;
    }
    return (uint8_t)((stepMs - elapsed + MS_PER_SECOND - 1) / MS_PER_SECOND);  /* round up */
}

/* Non-blocking: returns true when a new distance and frequency are available. */
bool ldr_update(DistanceSensor *sensor, MedianFilter *filter)
{
    uint32_t now = tick_ms();

    if (now - sensor->lastSampleMs < LDR_SAMPLE_INTERVAL_MS) {
        return false;
    }
    sensor->lastSampleMs = now;

    uint16_t level = volume_ldr_level();
    uint16_t filtered = filter_add(filter, level);

    if (sensor->state == LDR_STATE_INIT) {
        ldr_start_calibration(sensor, now, false);
    }
    if (ldr_is_calibrating(sensor) && !ldr_calibrate(sensor, level, filtered, now)) {
        return false;
    }
    ldr_calculate(sensor, filtered);
    return true;
}
