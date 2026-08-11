#ifndef WIFI_INTRO_LAYOUT_H
#define WIFI_INTRO_LAYOUT_H

#include <lvgl.h>
#include <stdint.h>

struct WifiIntroCallbacks {
    void (*onContinue)(const char *ssid, const char *pass);
    void (*onSkip)();
    /** Solo settingsMode: botón Atrás. */
    void (*onBack)();
};

struct WifiIntroConfig {
    int stepNum;
    int stepTotal;
    /** Ajuste post-wizard: Atrás + sin badge de paso; Skip oculto. */
    bool settingsMode;
    const char *initialSsid;
    const char *initialPass;
    WifiIntroCallbacks callbacks;
};

namespace WifiIntroLayout {

lv_obj_t *create(lv_obj_t *parent, const WifiIntroConfig &cfg);
void refresh(lv_obj_t *root);

}  // namespace WifiIntroLayout

#endif
