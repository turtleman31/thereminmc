/* Test double for <avr/interrupt.h>: an ISR becomes a normal function the tests can call. */

#ifndef FAKE_AVR_INTERRUPT_H
#define FAKE_AVR_INTERRUPT_H

#define ISR(vector) void vector(void); void vector(void)

#endif
