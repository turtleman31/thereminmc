/*
 * 2x16 LCD (HD44780 in 4-bit mode) behind a PCF8574 I2C backpack.
 * Labels are written once; after that only the numbers are overwritten.
 */

#include "lcd.h"

#include <avr/io.h>
#include <stdbool.h>
#include <util/delay.h>

#include "twi.h"

#define LCD_ADDRESS             0x27
#define LCD_COMMAND             0
#define LCD_DATA                _BV(0)      /* backpack P0 = RS, P2 = E, P3 = backlight, P4..P7 = D4..D7 */
#define LCD_ENABLE              _BV(2)
#define LCD_BACKLIGHT           _BV(3)
#define LCD_HIGH_NIBBLE         0xF0
#define LCD_NIBBLE_SHIFT        4
#define LCD_CMD_CLEAR           0x01
#define LCD_CMD_ENTRY_MODE      0x06        /* cursor moves right, no display shift */
#define LCD_CMD_DISPLAY_ON      0x0C        /* display on, cursor and blink off */
#define LCD_CMD_FUNCTION_SET    0x28        /* 4-bit bus, 2 lines, 5x8 font */
#define LCD_CMD_SET_ADDRESS     0x80
#define LCD_INIT_8BIT           0x30
#define LCD_INIT_4BIT           0x20
#define LCD_INIT_8BIT_REPEATS   3
#define LCD_POWER_UP_DELAY_MS   50
#define LCD_INIT_DELAY_MS       5
#define LCD_CLEAR_DELAY_MS      2
#define LCD_ROW_OFFSET          0x40        /* DDRAM address of the second row */
#define LCD_COLUMNS             16
#define LCD_VALUE_COLUMN        9
#define LCD_VALUE_WIDTH         4
#define LCD_TOP_ROW             0
#define LCD_BOTTOM_ROW          1
#define DECIMAL_BASE            10

static void lcd_write_nibble(uint8_t bits)
{
    uint8_t frames[] = { bits | LCD_ENABLE, bits };    /* the LCD reads on the falling edge of E */

    twi_send(LCD_ADDRESS, frames, sizeof frames);
}

static void lcd_send(uint8_t value, uint8_t mode)
{
    uint8_t high = (value & LCD_HIGH_NIBBLE) | mode | LCD_BACKLIGHT;
    uint8_t low = (uint8_t)(value << LCD_NIBBLE_SHIFT) | mode | LCD_BACKLIGHT;
    uint8_t frames[] = { high | LCD_ENABLE, high, low | LCD_ENABLE, low };

    twi_send(LCD_ADDRESS, frames, sizeof frames);
}

static void lcd_set_cursor(uint8_t row, uint8_t column)
{
    lcd_send(LCD_CMD_SET_ADDRESS | (row * LCD_ROW_OFFSET + column), LCD_COMMAND);
}

static void lcd_write_text(const char *text)
{
    for (; *text != '\0'; text++) {
        lcd_send(*text, LCD_DATA);
    }
}

/* Writes a label and clears the rest of the row. */
static void lcd_write_line(uint8_t row, const char *label)
{
    lcd_set_cursor(row, 0);
    for (uint8_t column = 0; column < LCD_COLUMNS; column++) {
        lcd_send((*label != '\0') ? *label++ : ' ', LCD_DATA);
    }
}

/* Right-aligned number plus unit; only this part of the row is overwritten. */
static void lcd_write_number(uint8_t row, uint16_t value, const char *unit)
{
    char text[LCD_VALUE_WIDTH + 1];
    char *digit = text + LCD_VALUE_WIDTH;

    *digit = '\0';
    do {
        *--digit = '0' + value % DECIMAL_BASE;
        value /= DECIMAL_BASE;
    } while (value > 0 && digit > text);
    while (digit > text) {
        *--digit = ' ';
    }
    lcd_set_cursor(row, LCD_VALUE_COLUMN);
    lcd_write_text(text);
    lcd_write_text(unit);
}

void lcd_init(void)
{
    _delay_ms(LCD_POWER_UP_DELAY_MS);
    for (uint8_t i = 0; i < LCD_INIT_8BIT_REPEATS; i++) {
        lcd_write_nibble(LCD_INIT_8BIT | LCD_BACKLIGHT);
        _delay_ms(LCD_INIT_DELAY_MS);
    }
    lcd_write_nibble(LCD_INIT_4BIT | LCD_BACKLIGHT);
    lcd_send(LCD_CMD_FUNCTION_SET, LCD_COMMAND);
    lcd_send(LCD_CMD_DISPLAY_ON, LCD_COMMAND);
    lcd_send(LCD_CMD_CLEAR, LCD_COMMAND);
    _delay_ms(LCD_CLEAR_DELAY_MS);
    lcd_send(LCD_CMD_ENTRY_MODE, LCD_COMMAND);

    lcd_write_line(LCD_TOP_ROW, "Kalibreren...");
    lcd_write_line(LCD_BOTTOM_ROW, "Nog:");
}

void lcd_show_calibration(uint8_t secondsLeft)
{
    lcd_write_number(LCD_BOTTOM_ROW, secondsLeft, " s");
}

void lcd_show_measurement(uint8_t distanceCm, uint16_t frequencyHz)
{
    static bool labelsShown;
    static uint8_t shownDistanceCm;
    static uint16_t shownFrequencyHz;

    if (!labelsShown) {
        lcd_write_line(LCD_TOP_ROW, "Afstand:");
        lcd_write_line(LCD_BOTTOM_ROW, "Freq:");
        labelsShown = true;
        shownDistanceCm = UINT8_MAX;            /* force the first update */
        shownFrequencyHz = UINT16_MAX;
    }
    if (distanceCm != shownDistanceCm) {
        shownDistanceCm = distanceCm;
        lcd_write_number(LCD_TOP_ROW, distanceCm, " cm");
    }
    if (frequencyHz != shownFrequencyHz) {
        shownFrequencyHz = frequencyHz;
        lcd_write_number(LCD_BOTTOM_ROW, frequencyHz, " Hz");
    }
}
