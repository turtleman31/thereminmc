/* I2C (TWI) master, write only. Shared by the LCD and the 7-segment display. */

#include "twi.h"

#include <avr/io.h>
#include <stdbool.h>
#include <util/twi.h>

#define TWI_FREQUENCY           100000UL
#define TWI_BIT_RATE            ((F_CPU / TWI_FREQUENCY - 16) / 2)  /* datasheet: SCL = F_CPU / (16 + 2 * TWBR) */
#define TWI_ADDRESS_SHIFT       1

static void twi_wait(void)
{
    while (!(TWCR & _BV(TWINT))) {
    }
}

static bool twi_start(uint8_t address)
{
    TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
    twi_wait();
    TWDR = (uint8_t)(address << TWI_ADDRESS_SHIFT) | TW_WRITE;
    TWCR = _BV(TWINT) | _BV(TWEN);
    twi_wait();
    return TW_STATUS == TW_MT_SLA_ACK;
}

static void twi_write(uint8_t data)
{
    TWDR = data;
    TWCR = _BV(TWINT) | _BV(TWEN);
    twi_wait();
}

static void twi_stop(void)
{
    TWCR = _BV(TWINT) | _BV(TWSTO) | _BV(TWEN);
    while (TWCR & _BV(TWSTO)) {
    }
}

void twi_init(void)
{
    TWSR = 0;                                   /* prescaler 1 */
    TWBR = TWI_BIT_RATE;
}

bool twi_probe(uint8_t address)
{
    bool present = twi_start(address);

    twi_stop();
    return present;
}

void twi_send(uint8_t address, const uint8_t *data, uint8_t length)
{
    if (twi_start(address)) {                   /* skip the data if nobody answers */
        for (const uint8_t *end = data + length; data < end; data++) {
            twi_write(*data);
        }
    }
    twi_stop();
}
