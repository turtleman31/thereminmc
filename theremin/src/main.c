/*
 * Theremin - controller.
 * Initialises all components and runs the non-blocking main loop.
 */

#include <avr/interrupt.h>

#include "tick.h"

int main(void)
{
    tick_init();
    sei();

    for (;;) {
    }
}
