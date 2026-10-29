#ifndef LCD_H
#define LCD_H

#include <stdint.h>

#include "ldr.h"

void lcd_init(void);
void lcd_show_calibration(LdrState step, uint8_t secondsLeft);
void lcd_show_measurement(uint8_t distanceCm, uint16_t frequencyHz);

#endif
