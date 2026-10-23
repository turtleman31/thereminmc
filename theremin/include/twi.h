#ifndef TWI_H
#define TWI_H

#include <stdint.h>

void twi_init(void);
void twi_send(uint8_t address, const uint8_t *data, uint8_t length);

#endif
