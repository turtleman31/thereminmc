/* Running median filter on a heap array that grows and shrinks with the filter size. */

#include "filter.h"

#include <stdbool.h>
#include <stdlib.h>

typedef int (*SampleCompare)(const void *first, const void *second);

int filter_compare_value(const void *first, const void *second)
{
    const Sample *a = first;
    const Sample *b = second;

    return (a->value > b->value) - (a->value < b->value);
}

int filter_compare_age(const void *first, const void *second)
{
    const Sample *a = first;
    const Sample *b = second;

    return (a->age > b->age) - (a->age < b->age);
}

static void filter_sort(MedianFilter *f, SampleCompare compare)
{
    qsort(f->samples, f->size, sizeof(Sample), compare);
}

/* On failure *array is left untouched, so the old memory is not lost. */
static bool samples_realloc(Sample **array, uint8_t count)
{
    Sample *resized = realloc(*array, count * sizeof(Sample));

    if (resized == NULL) {
        return false;
    }
    *array = resized;
    return true;
}

void filter_free(MedianFilter *f)
{
    free(f->samples);
    f->samples = NULL;
    f->size = FILTER_SIZE_MIN;
}

void filter_resize(MedianFilter *f, uint8_t newSize)
{
    uint8_t storedCount = (f->samples == NULL) ? 0 : f->size;

    if (newSize == f->size) {
        return;
    }
    if (newSize <= FILTER_SIZE_MIN) {
        filter_free(f);                         /* size 1 = filter off, no memory needed */
        return;
    }
    if (newSize < storedCount) {
        filter_sort(f, filter_compare_age);     /* youngest first, so the oldest samples are dropped */
    }
    if (!samples_realloc(&f->samples, newSize)) {
        return;                                 /* out of memory: keep the old size */
    }
    for (Sample *p = f->samples + storedCount; p < f->samples + newSize; p++) {
        p->age = (uint8_t)(p - f->samples);     /* new samples count as oldest, so they are replaced first */
        p->value = f->lastMedian;
    }
    f->size = newSize;
}

void filter_init(MedianFilter *f, uint8_t size, uint16_t startValue)
{
    f->samples = NULL;
    f->size = FILTER_SIZE_MIN;
    f->lastMedian = startValue;
    filter_resize(f, size);                     /* stays off (size 1) if malloc fails */
}

uint16_t filter_add(MedianFilter *f, uint16_t value)
{
    Sample *oldest = f->samples;

    if (f->samples == NULL) {
        f->lastMedian = value;
        return value;
    }
    for (Sample *p = f->samples; p < f->samples + f->size; p++) {
        p->age++;
        if (p->age > oldest->age) {
            oldest = p;
        }
    }
    oldest->age = 0;
    oldest->value = value;
    filter_sort(f, filter_compare_value);
    f->lastMedian = f->samples[f->size / 2].value;     /* size is odd, so this is the middle */
    return f->lastMedian;
}
