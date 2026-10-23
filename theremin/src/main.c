/*
 * Theremin - controller.
 * Initialises all components and runs the non-blocking main loop.
 */

#include <avr/interrupt.h>

#include "buzzer.h"
#include "tick.h"

int main(void)
{
    tick_init();
    buzzer_init();
    sei();

    buzzer_set_volume(VOLUME_MAX);              /* fixed test tone until the potmeter is added */

    for (;;) {
    }
}
