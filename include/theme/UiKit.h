#ifndef UI_KIT_H
#define UI_KIT_H

#include <lvgl.h>
#include <stdint.h>

namespace UiKit {

/** Fondo opaco (COVER) sin gradiente — evita blanco residual de LVGL. */
void forceOpaqueBg(lv_obj_t *obj, lv_color_t color);

/** Pantalla full-size; fondo negro opaco (COVER). */
void styleScreen(lv_obj_t *root);
void applySolidBlack(lv_obj_t *root);
void styleTitle(lv_obj_t *label);
void styleHint(lv_obj_t *label);

lv_obj_t *styleHeader(lv_obj_t *parent, const char *title, lv_event_cb_t onBack);
/** rightReserve: px a la derecha para + / Refresh / chrome (título con LONG_CLIP). */
lv_obj_t *styleHeader(lv_obj_t *parent, const char *title, lv_event_cb_t onBack,
                      lv_coord_t rightReserve);

lv_obj_t *makeBackButton(lv_obj_t *parent, lv_event_cb_t cb);
lv_obj_t *makeNavButton(lv_obj_t *parent, const char *text, lv_coord_t w, lv_event_cb_t cb);
lv_obj_t *makePrimaryButton(lv_obj_t *parent, const char *text, lv_event_cb_t cb);
lv_obj_t *makeSecondaryButton(lv_obj_t *parent, const char *text, lv_coord_t w, lv_coord_t h,
                              lv_event_cb_t cb);
lv_obj_t *makeCautionButton(lv_obj_t *parent, const char *text, lv_event_cb_t cb);
lv_obj_t *makeMenuRow(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *userData);
/**
 * Fila hub simétrica a Settings: título montserrat_20 + hint muted debajo + chevron.
 */
lv_obj_t *makeHubMenuRow(lv_obj_t *parent, const char *title, const char *hint, lv_event_cb_t cb,
                         void *userData);

/**
 * Feedback táctil: idle + LV_STATE_PRESSED (fondo/borde).
 * Usar también en filas custom (hub) con CLICKABLE.
 */
void applyPressStyle(lv_obj_t *obj, lv_color_t idleBg, lv_color_t pressedBg, lv_color_t idleBorder,
                     lv_color_t pressedBorder);

/**
 * Contraste de selección en filas makeMenuRow / botones secundarios.
 * selected: fondo surfaceSelected + borde accent 3 px + texto accentText.
 * disabled: muted, no clickable.
 */
void applySelectionStyle(lv_obj_t *row, bool selected, bool disabled = false);

void styleParamCard(lv_obj_t *card);
void styleMonitorCell(lv_obj_t *cell);

/** Teclado on-screen dark (sesión WiFi / texto). */
void styleDarkKeyboard(lv_obj_t *kb);

/** Teclado numérico dark (modo NUMBER — Reservorio y valores). */
void styleNumericKeyboard(lv_obj_t *kb);

/**
 * Adjunta kb NUMBER a ta: modo NUMBER + estilo + READY/CANCEL → onDone(code).
 * onDone recibe LV_EVENT_READY o LV_EVENT_CANCEL; puede ser nullptr.
 */
void attachNumericKeyboard(lv_obj_t *kb, lv_obj_t *ta, lv_event_cb_t onDone);

/** Status de pantalla bomba: Idle muted · Active/Done text · Stopped warn. */
enum class PumpUiStatus : uint8_t { Idle = 0, Active, Done, Stopped };
void setPumpStatus(lv_obj_t *lbl, PumpUiStatus st, const char *msg);

/**
 * Overlay full-screen negro + icono refresh + caption.
 * Fuerza paint y espera ~1 s. Llamar justo antes de ESP.restart().
 */
void showRebootSplash(const char *caption);

}  // namespace UiKit

#endif
