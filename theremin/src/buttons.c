/*
 * Filter size buttons on PD4 (minus) and PD5 (plus) via pin change interrupt.
 * The ISR only stores the requested size; the main loop resizes the filter.
 */

#include "buttons.h"

#include <avr/interrupt.h>
#include <avr/io.h>

#include "filter.h"
#include "tick.h"

#define BUTTON_MINUS_PIN        PD4
#define BUTTON_PLUS_PIN         PD5
#define BUTTON_MASK             (_BV(BUTTON_MINUS_PIN) | _BV(BUTTON_PLUS_PIN))
#define BUTTON_DEBOUNCE_MS      50U

static volatile uint8_t requestedFilterSize = FILTER_SIZE_DEFAULT;
static volatile bool filterSizeChanged = true;     /* show the start size once */

uint8_t buttons_step_filter_size(uint8_t size, bool increase)
{
    if (increase) {
        return (size < FILTER_SIZE_MAX) ? size + FILTER_SIZE_STEP : FILTER_SIZE_MAX;
    }
    return (size > FILTER_SIZE_MIN) ? size - FILTER_SIZE_STEP : FILTER_SIZE_MIN;
}

ISR(PCINT2_vect)
{
    static uint8_t previousButtons = BUTTON_MASK;   /* released = high (pull-up) */
    static uint32_t lastEdgeMs;
    uint32_t now = tick_ms();
    uint8_t buttons = PIND & BUTTON_MASK;
    uint8_t pressed = previousButtons & (uint8_t)~buttons;
    bool bouncing = (now - lastEdgeMs) < BUTTON_DEBOUNCE_MS;

    previousButtons = buttons;
    lastEdgeMs = now;
    if (bouncing || pressed == 0) {
        return;
    }

    uint8_t newSize = buttons_step_filter_size(requestedFilterSize, pressed & _BV(BUTTON_PLUS_PIN));

    if (newSize != requestedFilterSize) {
        requestedFilterSize = newSize;
        filterSizeChanged = true;
    }
}

void buttons_init(void)
{
    DDRD &= ~BUTTON_MASK;
    PORTD |= BUTTON_MASK;                       /* internal pull-ups, pressed = low */
    PCMSK2 = _BV(PCINT20) | _BV(PCINT21);       /* PD4 and PD5 */
    PCICR = _BV(PCIE2);
}

bool buttons_filter_size_changed(uint8_t *requestedSize)
{
    if (!filterSizeChanged) {
        return false;
    }
    filterSizeChanged = false;                  /* cleared before reading, so no press is lost */
    *requestedSize = requestedFilterSize;
    return true;
}

void buttons_set_filter_size(uint8_t size)
{
    requestedFilterSize = size;
}
