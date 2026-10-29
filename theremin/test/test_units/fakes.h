#ifndef FAKES_H
#define FAKES_H

#include <stdint.h>

extern uint32_t fakeTimeMs;         /* returned by tick_ms() */
extern uint16_t fakeLdrLevel;       /* returned by volume_ldr_level() */
extern uint8_t fakeDeviceAddress;   /* the only address twi_probe() finds, 0 = none */
extern uint8_t sentAddress;         /* last twi_send() call */
extern uint8_t sentData;
extern uint8_t sentLength;

#endif
