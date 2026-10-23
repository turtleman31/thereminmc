/*
 * Theremin - controller.
 * Initialises all components and runs the non-blocking main loop.
 */

#include <avr/interrupt.h>

#include "buzzer.h"
#include "tick.h"
#include "volume.h"

int main(void)
{
    tick_init();
    volume_init();
    buzzer_init();
    sei();

    for (;;) {
        buzzer_set_volume(volume_get());
    }
}
