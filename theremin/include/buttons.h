#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdbool.h>
#include <stdint.h>

void buttons_init(void);
bool buttons_filter_size_changed(uint8_t *requestedSize);
void buttons_set_filter_size(uint8_t size);
uint8_t buttons_step_filter_size(uint8_t size, bool increase);

#endif
