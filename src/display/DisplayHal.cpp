#include "DisplayHal.h"
#include "TouchHal.h"
#include "BacklightIdle.h"
#include "BoardPins.h"
#include "Config.h"
#include <Arduino_GFX_Library.h>
#include <string.h>

#define SCR_BUF_LINES 40

static Arduino_DataBus *bus = nullptr;
static Arduino_GFX *panel = nullptr;
static Arduino_Canvas *canvas = nullptr;
static Arduino_GFX *gfx = nullptr;
static lv_disp_draw_buf_t drawBuf;
static lv_disp_drv_t dispDrv;
static lv_indev_drv_t indevDrv;
static lv_color_t dispDrawBuf[LCD_H_RES * SCR_BUF_LINES];
static lv_disp_t *dispHandle = nullptr;
static bool dispReady = false;
static uint32_t flushCount = 0;

static void clearCanvasBlack() {
    if (!gfx) {
        return;
    }
    gfx->fillScreen(BLACK);
    gfx->flush();
}

/** Pantalla activa LVGL negra opaca antes de montar NavShell. */
static void paintScrActBlack() {
    lv_obj_t *scr = lv_scr_act();
    if (!scr) {
        return;
    }
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_NONE, 0);
    lv_obj_invalidate(scr);
}

static void lvglFlush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    if (!gfx) {
        lv_disp_flush_ready(drv);
        return;
    }
    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;

#if (LV_COLOR_16_SWAP != 0)
    gfx->draw16bitBeRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t *>(color_p), w, h);
#else
    gfx->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t *>(color_p), w, h);
#endif

    if (lv_disp_flush_is_last(drv)) {
        gfx->flush();
    }
    ++flushCount;
    lv_disp_flush_ready(drv);
}

static void touchRead(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    (void)drv;
    int16_t x = 0;
    int16_t y = 0;
    const bool pressed = TouchHal::read(x, y);
    data->point.x = x;
    data->point.y = y;
    if (BacklightIdle::onTouchSample(pressed)) {
        /* Wake-only: no entregar el gesto a LVGL. */
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void DisplayHal::setBacklight(bool on) {
    if (LCD_PIN_BL < 0) {
        return;
    }
    const bool level = LCD_BL_ACTIVE_HIGH ? on : !on;
    digitalWrite(LCD_PIN_BL, level ? HIGH : LOW);
    Serial.printf("[DISP] backlight %s (GPIO %d -> %s)\n",
                  on ? "ON" : "OFF",
                  LCD_PIN_BL,
                  level ? "HIGH" : "LOW");
}

void DisplayHal::fillRgb(uint8_t r, uint8_t g, uint8_t b) {
    if (!gfx) {
        return;
    }
    const uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    gfx->fillScreen(c);
    gfx->flush();
}

void DisplayHal::runSelfTest() {
    if (!gfx) {
        return;
    }
    setBacklight(true);
    fillRgb(255, 0, 0);
    delay(200);
    fillRgb(0, 255, 0);
    delay(200);
    fillRgb(0, 0, 255);
    delay(200);
    fillRgb(0, 0, 0);
}

void DisplayHal::dumpStatus() {
    Serial.println("[DISP] ----- status JC3248W535 LANDSCAPE dark -----");
    Serial.printf("  ready=%d flushes=%lu\n", dispReady ? 1 : 0,
                  static_cast<unsigned long>(flushCount));
    Serial.printf("  lvgl=%dx%d  panel=%dx%d  canvas_rot=%d  ips=%d\n",
                  LCD_H_RES, LCD_V_RES, PANEL_W, PANEL_H, LCD_CANVAS_ROT, LCD_IPS);
    Serial.printf("  touch ready=%d\n", TouchHal::ready() ? 1 : 0);
    Serial.printf("  free heap=%u  psram free=%u / size=%u\n",
                  ESP.getFreeHeap(), ESP.getFreePsram(), ESP.getPsramSize());
    Serial.println("[DISP] Dark: IPS=0 + canvas black + buf0 + full_refresh + scr black");
    Serial.println("[DISP] --------------------------------------");
}

bool DisplayHal::begin() {
    Serial.println("[DISP] begin() landscape 480x320 (Canvas rot 1, IPS=0)...");

    if (ESP.getPsramSize() == 0) {
        Serial.println("[DISP] FATAL: PSRAM requerido");
        return false;
    }

    if (LCD_PIN_BL >= 0) {
        pinMode(LCD_PIN_BL, OUTPUT);
        setBacklight(true);
    }

    bus = new Arduino_ESP32QSPI(
        LCD_PIN_CS, LCD_PIN_SCK, LCD_PIN_D0, LCD_PIN_D1, LCD_PIN_D2, LCD_PIN_D3);
    if (!bus) {
        return false;
    }

    panel = new Arduino_AXS15231B(
        bus,
        (LCD_PIN_RST < 0) ? GFX_NOT_DEFINED : LCD_PIN_RST,
        LCD_ROTATION,
        LCD_IPS != 0,
        PANEL_W,
        PANEL_H);
    if (!panel) {
        return false;
    }

    canvas = new Arduino_Canvas(PANEL_W, PANEL_H, panel, 0, 0, LCD_CANVAS_ROT);
    gfx = canvas;
    if (!canvas || !canvas->begin()) {
        Serial.println("[DISP] FATAL: canvas begin failed");
        return false;
    }
    Serial.printf("[DISP] Canvas OK logical %dx%d (panel fb %dx%d rot=%d)\n",
                  canvas->width(), canvas->height(), PANEL_W, PANEL_H, LCD_CANVAS_ROT);

    if (canvas->width() != LCD_H_RES || canvas->height() != LCD_V_RES) {
        Serial.printf("[DISP] WARN: canvas %dx%d != LCD %dx%d\n",
                      canvas->width(), canvas->height(), LCD_H_RES, LCD_V_RES);
    }

#if DISP_SELFTEST
    runSelfTest();
#endif

    // 1) Canvas negro antes de LVGL
    clearCanvasBlack();

    // 2) LVGL + buffer a 0 (anti-blanco residual en draw buf)
    lv_init();
    const size_t bufPixels = static_cast<size_t>(LCD_H_RES) * SCR_BUF_LINES;
    memset(dispDrawBuf, 0, sizeof(dispDrawBuf));
    lv_disp_draw_buf_init(&drawBuf, dispDrawBuf, nullptr, bufPixels);

    lv_disp_drv_init(&dispDrv);
    dispDrv.hor_res = LCD_H_RES;
    dispDrv.ver_res = LCD_V_RES;
    dispDrv.flush_cb = lvglFlush;
    dispDrv.draw_buf = &drawBuf;
    dispDrv.full_refresh = 1;
    dispDrv.sw_rotate = 0;
    dispHandle = lv_disp_drv_register(&dispDrv);

    lv_disp_set_bg_color(dispHandle, lv_color_hex(0x000000));
    lv_disp_set_bg_opa(dispHandle, LV_OPA_COVER);

    // 3) scr_act negro opaco antes de NavShell::begin
    paintScrActBlack();

    TouchHal::begin();

    lv_indev_drv_init(&indevDrv);
    indevDrv.type = LV_INDEV_TYPE_POINTER;
    indevDrv.read_cb = touchRead;
    lv_indev_drv_register(&indevDrv);

    dispReady = true;
    dumpStatus();
    return true;
}

void DisplayHal::loop() {
    if (!dispReady) {
        return;
    }
    BacklightIdle::tick();
    lv_timer_handler();
}

lv_disp_t *DisplayHal::disp() { return dispHandle; }

bool DisplayHal::ready() { return dispReady; }
