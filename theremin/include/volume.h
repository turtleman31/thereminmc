#ifndef VOLUME_H
#define VOLUME_H

#include <stdint.h>

#define ADC_VALUE_MAX 1023U

void volume_init(void);
uint8_t volume_get(void);
uint16_t volume_ldr_level(void);    /* the LDR shares the ADC, so its reading comes from here */

#endif
