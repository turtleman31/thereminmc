/*
 * Buzzer interface: Timer0 (CTC) interrupts every half period of the tone and
 * switches the Timer2 fast PWM on OC2B (PD3) on and off. The PWM duty cycle is the volume.
 */

#include "buzzer.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdbool.h>

#define BUZZER_PIN              PD3
#define TONE_PRESCALER          256UL       /* must match CS02 in buzzer_init() */
#define HALF_PERIODS_PER_CYCLE  2UL
#define VOLUME_MUTE_THRESHOLD   3U          /* potmeter readings below this are silent */

static volatile uint8_t toneCompareValue;
static volatile bool buzzerMuted = true;

ISR(TIMER0_COMPA_vect)
{
    OCR0A = toneCompareValue;               /* counter was just reset: safe moment to change pitch */
    if (buzzerMuted) {
        TCCR2A &= ~_BV(COM2B1);
    } else {
        TCCR2A ^= _BV(COM2B1);
    }
}

uint8_t buzzer_compare_value(uint16_t frequencyHz)
{
    uint32_t divider = HALF_PERIODS_PER_CYCLE * TONE_PRESCALER * frequencyHz;

    return (uint8_t)((F_CPU + divider / 2) / divider - 1);     /* rounded to the nearest count */
}

void buzzer_init(void)
{
    DDRD |= _BV(BUZZER_PIN);
    PORTD &= ~_BV(BUZZER_PIN);              /* silent while OC2B is disconnected */
    TCCR2A = _BV(WGM21) | _BV(WGM20);       /* fast PWM, COM2B1 is switched by the tone ISR */
    TCCR2B = _BV(CS20);                     /* no prescaler: 62.5 kHz */

    toneCompareValue = buzzer_compare_value(FREQUENCY_MIN_HZ);
    OCR0A = toneCompareValue;
    TCCR0A = _BV(WGM01);                    /* CTC */
    TIMSK0 = _BV(OCIE0A);
    TCCR0B = _BV(CS02);                     /* prescaler 256 */
}

void buzzer_set_frequency(uint16_t frequencyHz)
{
    toneCompareValue = buzzer_compare_value(frequencyHz);
}

void buzzer_set_volume(uint8_t volume)
{
    OCR2B = volume;
    buzzerMuted = volume < VOLUME_MUTE_THRESHOLD;
}
