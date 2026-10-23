#ifndef TICK_H
#define TICK_H

#include <stdint.h>

#define MS_PER_SECOND 1000U

void tick_init(void);
uint32_t tick_ms(void);

#endif
