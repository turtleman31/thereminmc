#ifndef STORAGE_H
#define STORAGE_H

#include <stdbool.h>

#include "ldr.h"

bool storage_load_points(float *reflection);       /* false when nothing valid is stored */
void storage_save_points(const float *reflection);

#endif
