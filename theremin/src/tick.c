/* System tick: Timer1 in CTC mode gives an interrupt every millisecond. */

#include "tick.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>

#define TICK_PRESCALER          64UL
#define TICK_FREQUENCY_HZ       1000UL
#define TICK_COMPARE_VALUE      (F_CPU / TICK_PRESCALER / TICK_FREQUENCY_HZ - 1)

static volatile uint32_t systemTimeMs;

ISR(TIMER1_COMPA_vect)
{
    systemTimeMs++;
}

void tick_init(void)
{
    TCCR1A = 0;
    OCR1A = TICK_COMPARE_VALUE;
    TIMSK1 = _BV(OCIE1A);
    TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);   /* CTC, prescaler 64; starts the timer */
}

uint32_t tick_ms(void)
{
    uint32_t now;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {             /* 32-bit read is not atomic on an 8-bit CPU */
        now = systemTimeMs;
    }
    return now;
}
