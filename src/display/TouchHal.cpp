#include "TouchHal.h"
#include "BoardPins.h"
#include <Wire.h>

namespace {

bool touchReady = false;
bool loggedFirstTouch = false;

const uint8_t kAxsReadCmd[8] = {0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08};

void pulseReset() {
    if (TOUCH_PIN_RST < 0) {
        return;
    }
    pinMode(TOUCH_PIN_RST, OUTPUT);
    digitalWrite(TOUCH_PIN_RST, LOW);
    delay(10);
    digitalWrite(TOUCH_PIN_RST, HIGH);
    delay(50);
}

bool probeAddr() {
    Wire.beginTransmission(TOUCH_I2C_ADDR);
    return Wire.endTransmission() == 0;
}

/**
 * Panel nativo (px,py) → LVGL.
 * Portrait: coords directas.
 * Landscape (spike): lx = py, ly = (PANEL_W - 1) - px
 */
void mapPanelToLvgl(uint16_t px, uint16_t py, int16_t &lx, int16_t &ly) {
#if LCD_LANDSCAPE
    int32_t x = static_cast<int32_t>(py);
    int32_t y = static_cast<int32_t>(PANEL_W - 1) - static_cast<int32_t>(px);
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (x >= LCD_H_RES) {
        x = LCD_H_RES - 1;
    }
    if (y >= LCD_V_RES) {
        y = LCD_V_RES - 1;
    }
    lx = static_cast<int16_t>(x);
    ly = static_cast<int16_t>(y);
#else
    (void)px;
    (void)py;
    lx = static_cast<int16_t>(px);
    ly = static_cast<int16_t>(py);
#endif
}

}  // namespace

bool TouchHal::begin() {
    touchReady = false;
    loggedFirstTouch = false;
    pulseReset();
    if (TOUCH_PIN_INT >= 0) {
        pinMode(TOUCH_PIN_INT, INPUT);
    }
    Wire.begin(TOUCH_I2C_SDA, TOUCH_I2C_SCL);
    Wire.setClock(400000);
    delay(20);

    if (!probeAddr()) {
        Serial.printf("[TOUCH] addr 0x%02X fail\n", TOUCH_I2C_ADDR);
        pulseReset();
        delay(30);
        if (!probeAddr()) {
            Serial.println("[TOUCH] no responde — usa Serial: menu / back");
            return false;
        }
    }

    touchReady = true;
    Serial.printf("[TOUCH] ok map panel %dx%d -> lvgl %dx%d (land=%d)\n",
                  PANEL_W, PANEL_H, LCD_H_RES, LCD_V_RES, LCD_LANDSCAPE);
    return true;
}

bool TouchHal::ready() { return touchReady; }

bool TouchHal::read(int16_t &x, int16_t &y) {
    if (!touchReady) {
        return false;
    }

    Wire.beginTransmission(TOUCH_I2C_ADDR);
    Wire.write(kAxsReadCmd, sizeof(kAxsReadCmd));
    if (Wire.endTransmission() != 0) {
        return false;
    }
    delayMicroseconds(50);

    const size_t n = Wire.requestFrom(static_cast<uint8_t>(TOUCH_I2C_ADDR), static_cast<uint8_t>(8));
    if (n < 8) {
        return false;
    }

    uint8_t data[8];
    for (int i = 0; i < 8; ++i) {
        data[i] = Wire.read();
    }

    if (data[0] != 0 || data[1] == 0) {
        return false;
    }

    uint16_t rawX = static_cast<uint16_t>(((data[2] & 0x0F) << 8) | data[3]);
    uint16_t rawY = static_cast<uint16_t>(((data[4] & 0x0F) << 8) | data[5]);

    if (rawX >= PANEL_W) {
        rawX = PANEL_W - 1;
    }
    if (rawY >= PANEL_H) {
        rawY = PANEL_H - 1;
    }

    mapPanelToLvgl(rawX, rawY, x, y);

    if (!loggedFirstTouch) {
        loggedFirstTouch = true;
        Serial.printf("[TOUCH] first press panel=%u,%u -> lvgl=%d,%d\n", rawX, rawY, x, y);
    }
    return true;
}
