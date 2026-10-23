/*
 * Volume control: the ADC ISR alternates between the potmeter (ADC0) and the
 * LDR (ADC2) and starts the next conversion itself, so the main loop never waits.
 */

#include "volume.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>

#define ADC_CHANNEL_VOLUME      0
#define ADC_CHANNEL_SWITCH      _BV(MUX1)   /* toggling MUX1 switches ADC0 <-> ADC2 */
#define ADC_LEFT_ADJUST_SHIFT   6           /* ADLAR = 1, so ADC >> 6 is the 10-bit result */

static volatile uint8_t volumeLevel;
static volatile uint16_t ldrLevel;

ISR(ADC_vect)
{
    if (ADMUX & ADC_CHANNEL_SWITCH) {
        ldrLevel = ADC >> ADC_LEFT_ADJUST_SHIFT;
    } else {
        volumeLevel = ADCH;
    }
    ADMUX ^= ADC_CHANNEL_SWITCH;
    ADCSRA |= _BV(ADSC);
}

void volume_init(void)
{
    ADMUX = _BV(REFS0) | _BV(ADLAR) | ADC_CHANNEL_VOLUME;      /* AVCC reference */
    ADCSRB = 0;
    DIDR0 = _BV(ADC0D) | _BV(ADC2D);                            /* no digital input on analog pins */
    ADCSRA = _BV(ADEN) | _BV(ADIE) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0) | _BV(ADSC);  /* 125 kHz */
}

uint8_t volume_get(void)
{
    return volumeLevel;
}

uint16_t volume_ldr_level(void)
{
    uint16_t level;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        level = ldrLevel;
    }
    return level;
}
