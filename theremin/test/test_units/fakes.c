/* Fake registers and stubs for the modules that are not under test (tick, volume, twi). */

#include "fakes.h"

#include <avr/io.h>

#include "tick.h"
#include "twi.h"
#include "volume.h"

uint8_t DDRD, PORTD, PIND;
uint8_t TCCR0A, TCCR0B, OCR0A, TIMSK0;
uint8_t TCCR2A, TCCR2B, OCR2B;
uint8_t PCICR, PCMSK2;

uint32_t fakeTimeMs;
uint16_t fakeLdrLevel;
uint8_t sentAddress;
uint8_t sentData;
uint8_t sentLength;

uint32_t tick_ms(void)
{
    return fakeTimeMs;
}

uint16_t volume_ldr_level(void)
{
    return fakeLdrLevel;
}

void twi_send(uint8_t address, const uint8_t *data, uint8_t length)
{
    sentAddress = address;
    sentData = (length > 0) ? *data : 0;
    sentLength = length;
}
