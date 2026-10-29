#ifndef LDR_H
#define LDR_H

#include <stdbool.h>
#include <stdint.h>

#include "filter.h"

#define LDR_REFERENCE_CM        10U         /* hand height during the second calibration step */

typedef enum {
    LDR_STATE_INIT,
    LDR_STATE_CALIBRATE_NO_HAND,
    LDR_STATE_CALIBRATE_REFERENCE,
    LDR_STATE_MEASURE
} LdrState;

typedef struct {
    LdrState state;
    uint32_t calibrationStartMs;
    uint32_t lastSampleMs;
    uint16_t noHandLevel;       /* lamp light without a hand, in LDR levels */
    uint16_t referenceLevel;    /* hand at LDR_REFERENCE_CM, in LDR levels */
    uint8_t distanceCm;
    uint16_t frequencyHz;
} DistanceSensor;

void ldr_init(DistanceSensor *sensor);
bool ldr_update(DistanceSensor *sensor, MedianFilter *filter);
void ldr_calculate(DistanceSensor *sensor, uint16_t level);
uint8_t ldr_calibration_seconds_left(const DistanceSensor *sensor, uint32_t now);

#endif
