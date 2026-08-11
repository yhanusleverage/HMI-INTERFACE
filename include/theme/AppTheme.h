#ifndef APP_THEME_H
#define APP_THEME_H

#include <lvgl.h>

/**
 * Tokens HIDRO HMI — Nuravine / ISA-101.
 * Fondo estructural: bgPlate en NavShell (#000000).
 */
namespace AppTheme {

inline lv_color_t bg() { return lv_color_hex(0x000000); }
inline lv_color_t bgMid() { return lv_color_hex(0x050D14); }
inline lv_color_t surface() { return lv_color_hex(0x0A1218); }
inline lv_color_t surfaceAlt() { return lv_color_hex(0x101C24); }
/** Fondo de fila seleccionada (aqua oscuro — contraste claro vs surface). */
inline lv_color_t surfaceSelected() { return lv_color_hex(0x163A42); }

inline lv_color_t text() { return lv_color_hex(0xE8EEF2); }
inline lv_color_t muted() { return lv_color_hex(0x6A858E); }

inline lv_color_t accent() { return lv_color_hex(0x2EC4B6); }
/** Texto sobre fondo seleccionado (más brillante que text). */
inline lv_color_t accentText() { return lv_color_hex(0x7EEDE3); }
/** Fondo CTA al pulsar (aqua más oscuro). */
inline lv_color_t accentPressed() { return lv_color_hex(0x1F9A8F); }
inline lv_color_t gridLine() { return lv_color_hex(0x70A9BE); }

inline lv_color_t ok() { return muted(); }
inline lv_color_t warn() { return lv_color_hex(0xE8B84A); }
/** Caution al pulsar (ámbar más oscuro). */
inline lv_color_t warnPressed() { return lv_color_hex(0xC49A3A); }
inline lv_color_t alarm() { return lv_color_hex(0xE85D5D); }

inline lv_color_t simBadge() { return lv_color_hex(0x5B8DEF); }
inline lv_color_t liveBadge() { return lv_color_hex(0x7FA8B0); }

constexpr lv_coord_t PAD = 8;
constexpr lv_coord_t GAP = 0;
constexpr lv_coord_t TOUCH_MIN_H = 44;
constexpr lv_coord_t TOUCH_MIN_W = 48;
constexpr lv_coord_t BTN_PRIMARY_H = 48;
constexpr lv_coord_t BACK_W = 100;
constexpr lv_coord_t BACK_H = 44;
constexpr lv_coord_t NAV_BTN_H = 44;
constexpr lv_coord_t CARD_RADIUS = 0;
constexpr lv_coord_t BORDER_IDLE = 1;
constexpr lv_coord_t BORDER_ALERT = 2;
constexpr lv_coord_t HEADER_H = 44;
constexpr lv_coord_t MENU_ROW_H = 48;
/** Fila hub con título + hint (Dosificacion / Controle). Hint 16 px. */
constexpr lv_coord_t HUB_ROW_H = 70;
/** Primer ítem de lista bajo header (Atrás + título). */
constexpr lv_coord_t MENU_LIST_TOP = BACK_H + PAD;
/** Teclado on-screen (landscape 480×320): teclas > TOUCH_MIN_H. */
constexpr lv_coord_t KEYBOARD_H = 220;

}  // namespace AppTheme

#endif
