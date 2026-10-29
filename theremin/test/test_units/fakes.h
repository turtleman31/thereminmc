#ifndef FAKES_H
#define FAKES_H

#include <stdbool.h>
#include <stdint.h>

#include "ldr.h"

extern uint32_t fakeTimeMs;         /* returned by tick_ms() */
extern uint16_t fakeLdrLevel;       /* returned by volume_ldr_level() */
extern uint8_t fakeDeviceAddress;   /* the only address twi_probe() finds, 0 = none */
extern uint8_t sentAddress;         /* last twi_send() call */
extern uint8_t sentData;
extern uint8_t sentLength;
extern bool fakePointsStored;       /* the EEPROM of storage_load_points() / storage_save_points() */
extern float fakeStoredReflection[LDR_CALIBRATION_POINTS];

#endif
