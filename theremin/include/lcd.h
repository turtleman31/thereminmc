#ifndef LCD_H
#define LCD_H

#include <stdint.h>

void lcd_init(void);
void lcd_show_calibration(uint8_t handHeightCm, uint8_t secondsLeft);  /* 0 = no hand */
void lcd_show_measurement(uint8_t distanceCm, uint16_t frequencyHz);

#endif
