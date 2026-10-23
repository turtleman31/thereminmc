#ifndef FILTER_H
#define FILTER_H

#include <stdint.h>

#define FILTER_SIZE_MIN     1U
#define FILTER_SIZE_MAX     15U
#define FILTER_SIZE_STEP    2U
#define FILTER_SIZE_DEFAULT 5U

typedef struct {
    uint8_t age;            /* 0 = newest sample */
    uint16_t value;
} Sample;

typedef struct {
    Sample *samples;        /* heap array, NULL while the filter is off (size 1) */
    uint8_t size;
    uint16_t lastMedian;
} MedianFilter;

void filter_init(MedianFilter *f, uint8_t size, uint16_t startValue);
uint16_t filter_add(MedianFilter *f, uint16_t value);
void filter_resize(MedianFilter *f, uint8_t newSize);
void filter_free(MedianFilter *f);
int filter_compare_value(const void *first, const void *second);
int filter_compare_age(const void *first, const void *second);

#endif
