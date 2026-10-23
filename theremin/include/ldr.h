#ifndef LDR_H
#define LDR_H

#include <stdbool.h>
#include <stdint.h>

#include "filter.h"

typedef enum {
    LDR_STATE_INIT,
    LDR_STATE_CALIBRATE,
    LDR_STATE_MEASURE
} LdrState;

typedef struct {
    LdrState state;
    uint32_t calibrationStartMs;
    uint32_t lastSampleMs;
    uint16_t adcMin;        /* hand close to the sensor (dark) */
    uint16_t adcMax;        /* no hand above the sensor (light) */
    uint8_t distanceCm;
    uint16_t frequencyHz;
} DistanceSensor;

void ldr_init(DistanceSensor *sensor);
bool ldr_update(DistanceSensor *sensor, MedianFilter *filter);
void ldr_calculate(DistanceSensor *sensor, uint16_t level);
uint8_t ldr_calibration_seconds_left(const DistanceSensor *sensor, uint32_t now);

#endif
