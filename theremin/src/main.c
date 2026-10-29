/*
 * Theremin - controller.
 * Initialises all components and runs the non-blocking main loop:
 * volume -> filter size -> distance/pitch -> display.
 */

#include <avr/interrupt.h>

#include "buttons.h"
#include "buzzer.h"
#include "filter.h"
#include "lcd.h"
#include "ldr.h"
#include "segment.h"
#include "tick.h"
#include "twi.h"
#include "volume.h"

#define DISPLAY_INTERVAL_MS     200U
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
    segment_show(filter->size);
}

static void display_update(const DistanceSensor *sensor)
{
    static uint32_t lastUpdateMs;
    uint32_t now = tick_ms();

    if (now - lastUpdateMs < DISPLAY_INTERVAL_MS) {
        return;
    }
    lastUpdateMs = now;

    if (sensor->state == LDR_STATE_MEASURE) {
        lcd_show_measurement(sensor->distanceCm, sensor->frequencyHz);
    } else {
        lcd_show_calibration(ldr_calibration_seconds_left(sensor, now));
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
    twi_init();
    lcd_init();
    segment_init();
    sei();

    for (;;) {
        buzzer_set_volume(sensor.state == LDR_STATE_MEASURE ? volume_get() : VOLUME_OFF);
        filter_size_update(&filter);
        if (ldr_update(&sensor, &filter)) {
            buzzer_set_frequency(sensor.frequencyHz);
        }
        display_update(&sensor);
    }
}
