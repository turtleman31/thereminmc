#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

#define FREQUENCY_MIN_HZ    230U
#define FREQUENCY_MAX_HZ    1400U
#define VOLUME_OFF          0U
#define VOLUME_MAX          UINT8_MAX

void buzzer_init(void);
void buzzer_set_frequency(uint16_t frequencyHz);
void buzzer_set_volume(uint8_t volume);
uint8_t buzzer_compare_value(uint16_t frequencyHz);

#endif
