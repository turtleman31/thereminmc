/* Keeps the calibration points in the EEPROM, so they survive power-off and new uploads. */

#include "storage.h"

#include <avr/eeprom.h>

#define STORAGE_MARKER          0x7E02      /* written last; change it when the stored layout changes */

static float storedReflection[LDR_CALIBRATION_POINTS] EEMEM;
static uint16_t storedMarker EEMEM;

bool storage_load_points(float *reflection)
{
    if (eeprom_read_word(&storedMarker) != STORAGE_MARKER) {
        return false;                           /* a new Arduino reads 0xFFFF */
    }
    eeprom_read_block(reflection, storedReflection, sizeof storedReflection);
    return true;
}

void storage_save_points(const float *reflection)
{
    eeprom_update_block(reflection, storedReflection, sizeof storedReflection);
    eeprom_update_word(&storedMarker, STORAGE_MARKER);
}
