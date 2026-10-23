/*
 * Theremin - controller.
 * Initialises all components and runs the non-blocking main loop.
 */

#include <avr/interrupt.h>

#include "buttons.h"
#include "buzzer.h"
#include "filter.h"
#include "ldr.h"
#include "tick.h"
#include "volume.h"

#define FILTER_START_VALUE      ADC_VALUE_MAX   /* "no hand"; replaced by real samples during calibration */

static void filter_size_update(MedianFilter *filter)
{
    uint8_t requestedSize;

    if (!buttons_filter_size_changed(&requestedSize)) {
        return;
    }
    filter_resize(filter, requestedSize);
    if (filter->size != requestedSize) {
        buttons_set_filter_size(filter->size);  /* out of memory: keep the buttons in sync */
    }
}

int main(void)
{
    MedianFilter filter;
    DistanceSensor sensor;

    filter_init(&filter, FILTER_SIZE_DEFAULT, FILTER_START_VALUE);
    ldr_init(&sensor);
    tick_init();
    volume_init();
    buzzer_init();
    buttons_init();
    sei();

    for (;;) {
        buzzer_set_volume(sensor.state == LDR_STATE_MEASURE ? volume_get() : VOLUME_OFF);
        filter_size_update(&filter);
        if (ldr_update(&sensor, &filter)) {
            buzzer_set_frequency(sensor.frequencyHz);
        }
    }
}
