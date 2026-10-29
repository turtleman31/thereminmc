/*
 * Distance sensor: calibrates the LDR for 5 seconds, then turns the filtered
 * reading into an estimated distance and a tone frequency.
 * Shadow model: a hand at height h lets h^2 / (HAND_RADIUS_CM^2 + h^2) of the
 * room light through, and the LDR resistance follows light^-LDR_GAMMA.
 */

#include "ldr.h"

#include <math.h>

#include "buzzer.h"
#include "tick.h"
#include "volume.h"

#define LDR_SAMPLE_INTERVAL_MS  20U         /* filter window = filter size * interval */
#define CALIBRATION_TIME_MS     5000U
#define CALIBRATION_MIN_RANGE   (50U * LDR_LEVEL_SCALE)
#define LDR_DEFAULT_ADC_MIN     (300U * LDR_LEVEL_SCALE)    /* used when the calibration fails */
#define LDR_DEFAULT_ADC_MAX     (800U * LDR_LEVEL_SCALE)
#define DISTANCE_MAX_CM         20U         /* above this the hand's shadow misses the LDR (test finding) */
#define LEVEL_STEPS             ((ADC_VALUE_MAX + 1.0f) * LDR_LEVEL_SCALE)
#define LDR_GAMMA               0.7f        /* GL5537 datasheet */
#define HAND_RADIUS_CM          6.0f        /* the hand blocks half the light at this height */
#define NO_HAND_MARGIN          (LDR_LEVEL_SCALE / 2)       /* half an ADC step of noise */

void ldr_init(DistanceSensor *sensor)
{
    sensor->state = LDR_STATE_INIT;
    sensor->lastSampleMs = 0;
}

static void ldr_start_calibration(DistanceSensor *sensor, uint32_t now)
{
    sensor->adcMin = LDR_LEVEL_MAX;
    sensor->adcMax = 0;
    sensor->calibrationStartMs = now;
    sensor->state = LDR_STATE_CALIBRATE;
}

/* Returns true when the calibration is finished. */
static bool ldr_calibrate(DistanceSensor *sensor, uint16_t level, uint32_t now)
{
    if (level < sensor->adcMin) {
        sensor->adcMin = level;
    }
    if (level > sensor->adcMax) {
        sensor->adcMax = level;
    }
    if (now - sensor->calibrationStartMs < CALIBRATION_TIME_MS) {
        return false;
    }

    uint16_t range = sensor->adcMax - sensor->adcMin;

    if (range < CALIBRATION_MIN_RANGE) {
        sensor->adcMin = LDR_DEFAULT_ADC_MIN;
        sensor->adcMax = LDR_DEFAULT_ADC_MAX;
    }
    sensor->state = LDR_STATE_MEASURE;
    return true;
}

/* Light on the LDR relative to the no-hand light; divider 5V - LDR - ADC - 10 kOhm - GND. */
static float ldr_light(uint16_t level, uint16_t noHandLevel)
{
    float resistanceRatio = ((LEVEL_STEPS - noHandLevel) * level) / (noHandLevel * (LEVEL_STEPS - level));

    return powf(resistanceRatio, 1 / LDR_GAMMA);                /* R(no hand) / R, to the power 1/gamma */
}

/* adcMin = 0 cm and the highest tone, adcMax (no hand) = DISTANCE_MAX_CM and 230 Hz. */
void ldr_calculate(DistanceSensor *sensor, uint16_t level)
{
    uint16_t noHandLevel = sensor->adcMax - NO_HAND_MARGIN;
    float distanceCm = DISTANCE_MAX_CM;

    if (level <= sensor->adcMin) {
        distanceCm = 0;
    } else if (level < noHandLevel) {
        float darkest = ldr_light(sensor->adcMin, noHandLevel);
        float passed = (ldr_light(level, noHandLevel) - darkest) / (1 - darkest);     /* the shadow model fraction */

        distanceCm = fminf(HAND_RADIUS_CM * sqrtf(passed / (1 - passed)), DISTANCE_MAX_CM);
    }
    sensor->distanceCm = (uint8_t)lroundf(distanceCm);
    sensor->frequencyHz = FREQUENCY_MAX_HZ
        - (uint16_t)lroundf((FREQUENCY_MAX_HZ - FREQUENCY_MIN_HZ) * distanceCm / DISTANCE_MAX_CM);
}

uint8_t ldr_calibration_seconds_left(const DistanceSensor *sensor, uint32_t now)
{
    uint32_t elapsed = (sensor->state == LDR_STATE_CALIBRATE) ? now - sensor->calibrationStartMs : 0;

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
    if (sensor->state == LDR_STATE_CALIBRATE && !ldr_calibrate(sensor, level, now)) {
        return false;
    }
    ldr_calculate(sensor, filtered);
    return true;
}
