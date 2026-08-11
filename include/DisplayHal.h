#ifndef DISPLAY_HAL_H
#define DISPLAY_HAL_H

#include <lvgl.h>
#include <Arduino.h>

namespace DisplayHal {
bool begin();
void loop();
lv_disp_t *disp();
bool ready();
void setBacklight(bool on);
void runSelfTest();
void fillRgb(uint8_t r, uint8_t g, uint8_t b);
void dumpStatus();
}

#endif
