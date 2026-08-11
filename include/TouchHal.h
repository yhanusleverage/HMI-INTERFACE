#ifndef TOUCH_HAL_H
#define TOUCH_HAL_H

#include <Arduino.h>

namespace TouchHal {

bool begin();
/** true si hay toque; x/y en coords LVGL (landscape 0..479 / 0..319) */
bool read(int16_t &x, int16_t &y);
bool ready();

}

#endif
