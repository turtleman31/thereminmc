/*
 * Distance sensor: a lamp next to the LDR shines up and the hand reflects its
 * light into the LDR. The reflection falls off with the distance squared, so
 * distance = LDR_REFERENCE_CM * sqrt(reflection at the reference / reflection).
 * Calibration: 2 s without a hand, then 3 s with the hand at LDR_REFERENCE_CM.
 */

#include "ldr.h"

#include <math.h>

#include "buzzer.h"
#include "tick.h"
#include "volume.h"

#define LDR_SAMPLE_INTERVAL_MS  20U         /* filter window = filter size * interval */
#define NO_HAND_TIME_MS         2000U
#define CALIBRATION_TIME_MS     5000U
#define REFERENCE_MIN_RISE      (10U * LDR_LEVEL_SCALE)     /* assumed when the hand reflected (almost) nothing */
#define DISTANCE_MAX_CM         20U         /* reach of the reflection (test finding) */
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
    return sensor->state == LDR_STATE_CALIBRATE_NO_HAND || sensor->state == LDR_STATE_CALIBRATE_REFERENCE;
}

static void ldr_start_calibration(DistanceSensor *sensor, uint32_t now)
{
    sensor->noHandLevel = 0;
    sensor->calibrationStartMs = now;
    sensor->state = LDR_STATE_CALIBRATE_NO_HAND;
}

/* Returns true when the calibration is finished. */
static bool ldr_calibrate(DistanceSensor *sensor, uint16_t level, uint16_t filtered, uint32_t now)
{
    uint32_t elapsed = now - sensor->calibrationStartMs;

    if (elapsed < NO_HAND_TIME_MS) {
        if (level > sensor->noHandLevel) {
            sensor->noHandLevel = level;        /* the highest reading, so noise does not look like a hand */
        }
        return false;
    }
    sensor->state = LDR_STATE_CALIBRATE_REFERENCE;
    if (elapsed < CALIBRATION_TIME_MS) {
        return false;
    }

    uint16_t minimumReference = sensor->noHandLevel + REFERENCE_MIN_RISE;

    sensor->referenceLevel = (filtered > minimumReference) ? filtered : minimumReference;
    sensor->state = LDR_STATE_MEASURE;
    return true;
}

/* Extra light on the LDR relative to the no-hand light; divider 5V - LDR - ADC - 10 kOhm - GND. */
static float ldr_reflection(uint16_t level, uint16_t noHandLevel)
{
    float resistanceRatio = ((LEVEL_STEPS - level) * noHandLevel) / (level * (LEVEL_STEPS - noHandLevel));

    return powf(resistanceRatio, -1 / LDR_GAMMA) - 1;          /* light = (R / R(no hand))^(-1/gamma) */
}

/* No hand = DISTANCE_MAX_CM and 230 Hz; a closer hand reflects more light and gives a higher tone. */
void ldr_calculate(DistanceSensor *sensor, uint16_t level)
{
    float distanceCm = DISTANCE_MAX_CM;

    if (level > sensor->noHandLevel + NO_HAND_MARGIN) {
        float reflection = ldr_reflection(level, sensor->noHandLevel);
        float referenceReflection = ldr_reflection(sensor->referenceLevel, sensor->noHandLevel);

        distanceCm = fminf(LDR_REFERENCE_CM * sqrtf(referenceReflection / reflection), DISTANCE_MAX_CM);
    }
    sensor->distanceCm = (uint8_t)lroundf(distanceCm);
    sensor->frequencyHz = FREQUENCY_MAX_HZ
        - (uint16_t)lroundf((FREQUENCY_MAX_HZ - FREQUENCY_MIN_HZ) * distanceCm / DISTANCE_MAX_CM);
}

uint8_t ldr_calibration_seconds_left(const DistanceSensor *sensor, uint32_t now)
{
    uint32_t elapsed = ldr_is_calibrating(sensor) ? now - sensor->calibrationStartMs : 0;

    if (elapsed >= CALIBRATION_TIME_MS) {
        return 0;
    }
    return (uint8_t)((CALIBRATION_TIME_MS - elapsed + MS_PER_SECOND - 1) / MS_PER_SECOND);  /* round up */
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
        ldr_start_calibration(sensor, now);
    }
    if (ldr_is_calibrating(sensor) && !ldr_calibrate(sensor, level, filtered, now)) {
        return false;
    }
    ldr_calculate(sensor, filtered);
    return true;
}
