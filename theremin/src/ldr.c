/*
 * Distance sensor: calibrates the LDR for 5 seconds, then turns the reading
 * into an estimated distance and a tone frequency.
 */

#include "ldr.h"

#include "buzzer.h"
#include "tick.h"
#include "volume.h"

#define LDR_SAMPLE_INTERVAL_MS  20U
#define CALIBRATION_TIME_MS     5000U
#define CALIBRATION_MIN_RANGE   50U
#define LDR_DEFAULT_ADC_MIN     300U        /* used when the calibration fails */
#define LDR_DEFAULT_ADC_MAX     800U
#define DISTANCE_MAX_CM         65U

void ldr_init(DistanceSensor *sensor)
{
    sensor->state = LDR_STATE_INIT;
    sensor->lastSampleMs = 0;
}

static void ldr_start_calibration(DistanceSensor *sensor, uint32_t now)
{
    sensor->adcMin = ADC_VALUE_MAX;
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

/* adcMin = 0 cm and the highest tone, adcMax (no hand) = 65 cm and 230 Hz. */
void ldr_calculate(DistanceSensor *sensor, uint16_t level)
{
    uint16_t range = sensor->adcMax - sensor->adcMin;
    uint16_t position;

    if (level <= sensor->adcMin) {
        position = 0;
    } else if (level >= sensor->adcMax) {
        position = range;
    } else {
        position = level - sensor->adcMin;
    }
    sensor->distanceCm = (uint8_t)((uint32_t)DISTANCE_MAX_CM * position / range);
    sensor->frequencyHz = FREQUENCY_MAX_HZ
        - (uint16_t)((uint32_t)(FREQUENCY_MAX_HZ - FREQUENCY_MIN_HZ) * position / range);
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
bool ldr_update(DistanceSensor *sensor)
{
    uint32_t now = tick_ms();

    if (now - sensor->lastSampleMs < LDR_SAMPLE_INTERVAL_MS) {
        return false;
    }
    sensor->lastSampleMs = now;

    uint16_t level = volume_ldr_level();

    if (sensor->state == LDR_STATE_INIT) {
        ldr_start_calibration(sensor, now);
    }
    if (sensor->state == LDR_STATE_CALIBRATE && !ldr_calibrate(sensor, level, now)) {
        return false;
    }
    ldr_calculate(sensor, level);
    return true;
}
