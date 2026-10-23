/* Test double for <avr/pgmspace.h>: a PC has no separate program memory. */

#ifndef FAKE_AVR_PGMSPACE_H
#define FAKE_AVR_PGMSPACE_H

#include <stdint.h>

#define PROGMEM
#define pgm_read_byte(address)  (*(const uint8_t *)(address))

#endif
