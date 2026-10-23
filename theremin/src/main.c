/*
 * Theremin - controller.
 * Initialises all components and runs the non-blocking main loop.
 */

#include <avr/interrupt.h>

#include "buzzer.h"
#include "filter.h"
#include "ldr.h"
#include "tick.h"
#include "volume.h"

#define FILTER_START_VALUE      ADC_VALUE_MAX   /* "no hand"; replaced by real samples during calibration */

int main(void)
{
    MedianFilter filter;
    DistanceSensor sensor;

    filter_init(&filter, FILTER_SIZE_DEFAULT, FILTER_START_VALUE);
    ldr_init(&sensor);
    tick_init();
    volume_init();
    buzzer_init();
    sei();

    for (;;) {
        buzzer_set_volume(sensor.state == LDR_STATE_MEASURE ? volume_get() : VOLUME_OFF);
        if (ldr_update(&sensor, &filter)) {
            buzzer_set_frequency(sensor.frequencyHz);
        }
    }
}
