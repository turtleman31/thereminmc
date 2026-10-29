/* 7-segment display (common anode) behind a PCF8574A or PCF8574: shows one hexadecimal digit. */

#include "segment.h"

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdbool.h>

#include "twi.h"

#define ADDRESS_PCF8574A        0x39        /* gameshield schematic: A0 = 1, A1 = A2 = 0 */
#define ADDRESS_PCF8574         0x21        /* same address pins, chip without the A */
#define HEX_DIGIT_MASK          0x0F
#define SEG_A                   _BV(0)      /* P0..P6 = segments A..G */
#define SEG_B                   _BV(1)
#define SEG_C                   _BV(2)
#define SEG_D                   _BV(3)
#define SEG_E                   _BV(4)
#define SEG_F                   _BV(5)
#define SEG_G                   _BV(6)

static const uint8_t HEX_SEGMENTS[] PROGMEM = {
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,          /* 0 */
    SEG_B | SEG_C,                                          /* 1 */
    SEG_A | SEG_B | SEG_D | SEG_E | SEG_G,                  /* 2 */
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_G,                  /* 3 */
    SEG_B | SEG_C | SEG_F | SEG_G,                          /* 4 */
    SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,                  /* 5 */
    SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,          /* 6 */
    SEG_A | SEG_B | SEG_C,                                  /* 7 */
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,  /* 8 */
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,          /* 9 */
    SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,          /* A */
    SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,                  /* b */
    SEG_A | SEG_D | SEG_E | SEG_F,                          /* C */
    SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,                  /* d */
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,                  /* E */
    SEG_A | SEG_E | SEG_F | SEG_G,                          /* F */
};

static uint8_t segmentAddress = ADDRESS_PCF8574A;

void segment_init(void)
{
    bool onlyPcf8574 = !twi_probe(ADDRESS_PCF8574A) && twi_probe(ADDRESS_PCF8574);

    segmentAddress = onlyPcf8574 ? ADDRESS_PCF8574 : ADDRESS_PCF8574A;
}

uint8_t segment_pattern(uint8_t value)
{
    return pgm_read_byte(&HEX_SEGMENTS[value & HEX_DIGIT_MASK]);
}

void segment_show(uint8_t value)
{
    uint8_t outputs = (uint8_t)~segment_pattern(value);    /* segments are active low */

    twi_send(segmentAddress, &outputs, sizeof outputs);
}
