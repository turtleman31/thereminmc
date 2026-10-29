#ifndef VOLUME_H
#define VOLUME_H

#include <stdint.h>

#define ADC_VALUE_MAX           1023U
#define LDR_LEVEL_SCALE         16U         /* LDR levels are in 1/16 ADC steps */
#define LDR_LEVEL_MAX           (ADC_VALUE_MAX * LDR_LEVEL_SCALE)

void volume_init(void);
uint8_t volume_get(void);
uint16_t volume_ldr_level(void);    /* average LDR reading since the last call; the LDR shares the ADC */

#endif
