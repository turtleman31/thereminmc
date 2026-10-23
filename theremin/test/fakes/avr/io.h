/* Test double for <avr/io.h>: registers are plain variables (defined in test_units/fakes.c). */

#ifndef FAKE_AVR_IO_H
#define FAKE_AVR_IO_H

#include <stdint.h>

#define _BV(bit)    (1U << (bit))

extern uint8_t DDRD, PORTD, PIND;
extern uint8_t TCCR0A, TCCR0B, OCR0A, TIMSK0;
extern uint8_t TCCR2A, TCCR2B, OCR2B;
extern uint8_t PCICR, PCMSK2;

#define PD3         3
#define PD4         4
#define PD5         5
#define WGM01       1
#define CS02        2
#define OCIE0A      1
#define WGM20       0
#define WGM21       1
#define COM2B1      5
#define CS20        0
#define PCIE2       2
#define PCINT20     4
#define PCINT21     5

#endif
