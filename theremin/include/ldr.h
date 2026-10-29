#ifndef LDR_H
#define LDR_H

#include <stdbool.h>
#include <stdint.h>

#include "filter.h"

#define LDR_CALIBRATION_POINTS  4U          /* hand at 5, 10, 15 and 20 cm */
#define LDR_CALIBRATION_STEP_CM 5U

typedef enum {
    LDR_STATE_INIT,
    LDR_STATE_CALIBRATE_NO_HAND,
    LDR_STATE_CALIBRATE_POINT,
    LDR_STATE_MEASURE
} LdrState;

typedef struct {
    LdrState state;
    bool calibratePoints;       /* also measure the hand heights, not only the no-hand light */
    uint8_t point;              /* calibration point being measured */
    uint32_t stepStartMs;
    uint32_t lastSampleMs;
    uint16_t noHandLevel;       /* lamp and room light without a hand, in LDR levels */
    float reflection[LDR_CALIBRATION_POINTS];   /* lamp light reflected by the hand at each point */
    uint8_t distanceCm;
    uint16_t frequencyHz;
} DistanceSensor;

void ldr_init(DistanceSensor *sensor);
void ldr_recalibrate(DistanceSensor *sensor);
bool ldr_update(DistanceSensor *sensor, MedianFilter *filter);
void ldr_calculate(DistanceSensor *sensor, uint16_t level);
uint8_t ldr_calibration_height_cm(const DistanceSensor *sensor);   /* 0 = no hand */
uint8_t ldr_calibration_seconds_left(const DistanceSensor *sensor, uint32_t now);

#endif
