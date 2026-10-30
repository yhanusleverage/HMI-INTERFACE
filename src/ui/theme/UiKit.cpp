#include "theme/UiKit.h"
#include "theme/AppTheme.h"
#include "BoardPins.h"
#include "AppStrings.h"

#include <Arduino.h>

namespace UiKit {

void forceOpaqueBg(lv_obj_t *obj, lv_color_t color) {
    if (!obj) {
        return;
    }
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_NONE, 0);
}

void applySolidBlack(lv_obj_t *root) {
    forceOpaqueBg(root, AppTheme::bg());
}

void styleScreen(lv_obj_t *root) {
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, LCD_H_RES, LCD_V_RES);
    forceOpaqueBg(root, AppTheme::bg());
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_radius(root, 0, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
}

void styleTitle(lv_obj_t *label) {
    lv_obj_set_style_text_color(label, AppTheme::text(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
}

void styleHint(lv_obj_t *label) {
    lv_obj_set_style_text_color(label, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
}

static void styleBtnLabel(lv_obj_t *btn, const char *text, lv_color_t fg) {
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, fg, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl);
}

static void prepBtn(lv_obj_t *btn) {
    lv_obj_remove_style_all(btn);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
}

void applyPressStyle(lv_obj_t *obj, lv_color_t idleBg, lv_color_t pressedBg, lv_color_t idleBorder,
                     lv_color_t pressedBorder) {
    if (!obj) {
        return;
    }
    lv_obj_set_style_bg_color(obj, idleBg, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, pressedBg, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_STATE_PRESSED);

    lv_obj_set_style_border_color(obj, idleBorder, 0);
    lv_obj_set_style_border_color(obj, pressedBorder, LV_STATE_PRESSED);
    /* Si idle no tenía borde, PRESSED aún puede mostrar 1 px de acento. */
    const lv_coord_t idleW = lv_obj_get_style_border_width(obj, 0);
    if (idleW <= 0) {
        lv_obj_set_style_border_width(obj, 0, 0);
        lv_obj_set_style_border_width(obj, 1, LV_STATE_PRESSED);
    } else {
        lv_obj_set_style_border_width(obj, idleW, LV_STATE_PRESSED);
    }
}

void applySelectionStyle(lv_obj_t *row, bool selected, bool disabled) {
    if (!row) {
        return;
    }
    const uint32_t n = lv_obj_get_child_cnt(row);
    if (disabled) {
        forceOpaqueBg(row, AppTheme::surface());
        lv_obj_set_style_border_color(row, AppTheme::gridLine(), 0);
        lv_obj_set_style_border_width(row, AppTheme::BORDER_IDLE, 0);
        for (uint32_t i = 0; i < n; ++i) {
            lv_obj_t *ch = lv_obj_get_child(row, i);
            if (ch) {
                lv_obj_set_style_text_color(ch, AppTheme::muted(), 0);
            }
        }
        lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
        return;
    }
    if (selected) {
        forceOpaqueBg(row, AppTheme::surfaceSelected());
        lv_obj_set_style_border_color(row, AppTheme::accent(), 0);
        lv_obj_set_style_border_width(row, 3, 0);
        for (uint32_t i = 0; i < n; ++i) {
            lv_obj_t *ch = lv_obj_get_child(row, i);
            if (!ch) {
                continue;
            }
            /* Título (0) accentText; chevron/hint accent o muted. */
            lv_obj_set_style_text_color(ch, (i == 0) ? AppTheme::accentText() : AppTheme::accent(),
                                        0);
        }
    } else {
        forceOpaqueBg(row, AppTheme::surface());
        lv_obj_set_style_border_color(row, AppTheme::gridLine(), 0);
        lv_obj_set_style_border_width(row, AppTheme::BORDER_IDLE, 0);
        for (uint32_t i = 0; i < n; ++i) {
            lv_obj_t *ch = lv_obj_get_child(row, i);
            if (!ch) {
                continue;
            }
            lv_obj_set_style_text_color(ch, (i == 0) ? AppTheme::text() : AppTheme::muted(), 0);
        }
    }
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
}

lv_obj_t *makeBackButton(lv_obj_t *parent, lv_event_cb_t cb) {
    lv_obj_t *btn = lv_btn_create(parent);
    prepBtn(btn);
    lv_obj_set_size(btn, AppTheme::BACK_W, AppTheme::BACK_H);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, AppTheme::PAD - 4, AppTheme::PAD - 4);
    lv_obj_set_style_radius(btn, AppTheme::CARD_RADIUS, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    applyPressStyle(btn, AppTheme::surfaceAlt(), AppTheme::surface(), AppTheme::gridLine(),
                    AppTheme::accent());
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    styleBtnLabel(btn, Strings::tr(Msg::Back), AppTheme::text());
    return btn;
}

lv_obj_t *styleHeader(lv_obj_t *parent, const char *title, lv_event_cb_t onBack,
                      lv_coord_t rightReserve) {
    makeBackButton(parent, onBack);

    const lv_coord_t leftPad = AppTheme::BACK_W + AppTheme::PAD;
    const lv_coord_t rightPad = (rightReserve > 0) ? rightReserve : AppTheme::PAD;
    const lv_coord_t maxW = LCD_H_RES - leftPad - rightPad;
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, title);
    styleTitle(lbl);
    lv_obj_set_width(lbl, maxW > 40 ? maxW : 40);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, leftPad,
                 (AppTheme::BACK_H - 20) / 2 + (AppTheme::PAD - 4));
    return lbl;
}

lv_obj_t *styleHeader(lv_obj_t *parent, const char *title, lv_event_cb_t onBack) {
    return styleHeader(parent, title, onBack, 100);
}

lv_obj_t *makeNavButton(lv_obj_t *parent, const char *text, lv_coord_t w, lv_event_cb_t cb) {
    lv_obj_t *btn = lv_btn_create(parent);
    prepBtn(btn);
    lv_obj_set_size(btn, w, AppTheme::NAV_BTN_H);
    lv_obj_set_style_radius(btn, AppTheme::CARD_RADIUS, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    applyPressStyle(btn, AppTheme::surfaceAlt(), AppTheme::surface(), AppTheme::gridLine(),
                    AppTheme::accent());
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    styleBtnLabel(btn, text, AppTheme::text());
    return btn;
}

lv_obj_t *makePrimaryButton(lv_obj_t *parent, const char *text, lv_event_cb_t cb) {
    lv_obj_t *btn = lv_btn_create(parent);
    prepBtn(btn);
    lv_obj_set_size(btn, LCD_H_RES - 40, AppTheme::BTN_PRIMARY_H);
    lv_obj_set_style_radius(btn, AppTheme::CARD_RADIUS, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    applyPressStyle(btn, AppTheme::accent(), AppTheme::accentPressed(), AppTheme::accent(),
                    AppTheme::text());
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    styleBtnLabel(btn, text, AppTheme::bg());
    return btn;
}

lv_obj_t *makeSecondaryButton(lv_obj_t *parent, const char *text, lv_coord_t w, lv_coord_t h,
                              lv_event_cb_t cb) {
    lv_obj_t *btn = lv_btn_create(parent);
    prepBtn(btn);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, AppTheme::CARD_RADIUS, 0);
    applyPressStyle(btn, AppTheme::surfaceAlt(), AppTheme::surface(), AppTheme::gridLine(),
                    AppTheme::accent());
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    styleBtnLabel(btn, text, AppTheme::text());
    return btn;
}

lv_obj_t *makeCautionButton(lv_obj_t *parent, const char *text, lv_event_cb_t cb) {
    lv_obj_t *btn = lv_btn_create(parent);
    prepBtn(btn);
    lv_obj_set_size(btn, LCD_H_RES - 40, AppTheme::BTN_PRIMARY_H);
    lv_obj_set_style_radius(btn, AppTheme::CARD_RADIUS, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    applyPressStyle(btn, AppTheme::warn(), AppTheme::warnPressed(), AppTheme::warn(),
                    AppTheme::text());
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    styleBtnLabel(btn, text, AppTheme::bg());
    return btn;
}

lv_obj_t *makeMenuRow(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *userData) {
    lv_obj_t *btn = lv_btn_create(parent);
    prepBtn(btn);
    lv_obj_set_width(btn, LCD_H_RES - 24);
    lv_obj_set_height(btn, AppTheme::MENU_ROW_H);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    applyPressStyle(btn, AppTheme::surface(), AppTheme::surfaceAlt(), AppTheme::gridLine(),
                    AppTheme::accent());
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, userData);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, AppTheme::text(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_t *chev = lv_label_create(btn);
    lv_label_set_text(chev, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(chev, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(chev, &lv_font_montserrat_20, 0);
    lv_obj_align(chev, LV_ALIGN_RIGHT_MID, -14, 0);
    return btn;
}

lv_obj_t *makeHubMenuRow(lv_obj_t *parent, const char *title, const char *hint, lv_event_cb_t cb,
                         void *userData) {
    lv_obj_t *btn = lv_btn_create(parent);
    prepBtn(btn);
    lv_obj_set_width(btn, LCD_H_RES - 24);
    lv_obj_set_height(btn, AppTheme::HUB_ROW_H);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    applyPressStyle(btn, AppTheme::surface(), AppTheme::surfaceAlt(), AppTheme::gridLine(),
                    AppTheme::accent());
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, userData);

    lv_obj_t *t = lv_label_create(btn);
    lv_label_set_text(t, title ? title : "");
    lv_obj_set_style_text_color(t, AppTheme::text(), 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_set_width(t, LCD_H_RES - 24 - 48);
    lv_label_set_long_mode(t, LV_LABEL_LONG_CLIP);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 14, 8);

    lv_obj_t *h = lv_label_create(btn);
    lv_label_set_text(h, hint ? hint : "");
    lv_obj_set_style_text_color(h, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(h, &lv_font_montserrat_16, 0);
    lv_obj_set_width(h, LCD_H_RES - 24 - 48);
    lv_label_set_long_mode(h, LV_LABEL_LONG_DOT);
    lv_obj_align(h, LV_ALIGN_BOTTOM_LEFT, 14, -6);

    lv_obj_t *chev = lv_label_create(btn);
    lv_label_set_text(chev, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(chev, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(chev, &lv_font_montserrat_20, 0);
    lv_obj_align(chev, LV_ALIGN_RIGHT_MID, -14, 0);
    return btn;
}

void alignHubValueRight(lv_obj_t *row) {
    if (!row) {
        return;
    }
    lv_obj_t *title = lv_obj_get_child(row, 0);
    lv_obj_t *value = lv_obj_get_child(row, 1);
    if (!title || !value) {
        return;
    }
    constexpr lv_coord_t kValueW = 168;
    constexpr lv_coord_t kChevronPad = 40;
    lv_obj_set_style_text_font(value, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(value, AppTheme::accentText(), 0);
    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(value, kValueW);
    lv_label_set_long_mode(value, LV_LABEL_LONG_CLIP);
    lv_obj_align(value, LV_ALIGN_RIGHT_MID, -kChevronPad, 0);

    const lv_coord_t rowW = LCD_H_RES - 24;
    const lv_coord_t titleW = rowW - 14 - kValueW - kChevronPad - 8;
    lv_obj_set_width(title, titleW > 40 ? titleW : 40);
    lv_label_set_long_mode(title, LV_LABEL_LONG_CLIP);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 14, 0);
}

void styleParamCard(lv_obj_t *card) {
    styleMonitorCell(card);
}

void styleMonitorCell(lv_obj_t *cell) {
    lv_obj_remove_style_all(cell);
    forceOpaqueBg(cell, AppTheme::bg());
    lv_obj_set_style_border_width(cell, AppTheme::BORDER_IDLE, 0);
    lv_obj_set_style_border_color(cell, AppTheme::gridLine(), 0);
    lv_obj_set_style_radius(cell, 0, 0);
    lv_obj_set_style_pad_all(cell, 10, 0);
    lv_obj_set_style_shadow_width(cell, 0, 0);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
}

void styleDarkKeyboard(lv_obj_t *kb) {
    if (!kb) {
        return;
    }
    forceOpaqueBg(kb, AppTheme::bg());
    lv_obj_set_size(kb, LCD_H_RES, AppTheme::KEYBOARD_H);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_border_width(kb, 0, 0);
    lv_obj_set_style_pad_all(kb, 8, 0);
    lv_obj_set_style_pad_row(kb, 6, 0);
    lv_obj_set_style_pad_column(kb, 6, 0);

    lv_obj_set_style_bg_color(kb, AppTheme::surface(), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, AppTheme::text(), LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb, &lv_font_montserrat_20, LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(kb, AppTheme::gridLine(), LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 0, LV_PART_ITEMS);

    lv_obj_set_style_bg_color(kb, AppTheme::surfaceAlt(), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(kb, AppTheme::accent(), LV_PART_ITEMS | LV_STATE_CHECKED);
    const lv_style_selector_t pressed = LV_PART_ITEMS | LV_STATE_PRESSED;
    const lv_style_selector_t pressedFocus = LV_PART_ITEMS | LV_STATE_PRESSED | LV_STATE_FOCUSED;
    lv_obj_set_style_bg_color(kb, AppTheme::accent(), pressed);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, pressed);
    lv_obj_set_style_text_color(kb, AppTheme::bg(), pressed);
    lv_obj_set_style_border_color(kb, AppTheme::accentText(), pressed);
    lv_obj_set_style_border_width(kb, 2, pressed);
    lv_obj_set_style_bg_color(kb, AppTheme::accent(), pressedFocus);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, pressedFocus);
    lv_obj_set_style_text_color(kb, AppTheme::bg(), pressedFocus);
    lv_obj_set_style_border_color(kb, AppTheme::accentText(), pressedFocus);
    lv_obj_set_style_border_width(kb, 2, pressedFocus);
}

void styleNumericKeyboard(lv_obj_t *kb) {
    if (!kb) {
        return;
    }
    styleDarkKeyboard(kb);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
}

void attachNumericKeyboard(lv_obj_t *kb, lv_obj_t *ta, lv_event_cb_t onDone) {
    if (!kb) {
        return;
    }
    styleNumericKeyboard(kb);
    if (ta) {
        lv_keyboard_set_textarea(kb, ta);
    }
    if (onDone) {
        lv_obj_add_event_cb(kb, onDone, LV_EVENT_READY, nullptr);
        lv_obj_add_event_cb(kb, onDone, LV_EVENT_CANCEL, nullptr);
    }
}

lv_obj_t *makeKbPreviewBar(lv_obj_t *parent, lv_obj_t **outTitle, lv_obj_t **outTa) {
    if (!parent) {
        return nullptr;
    }
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, LCD_H_RES, KB_EDIT_BAR_H);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -AppTheme::KEYBOARD_H);
    forceOpaqueBg(bar, AppTheme::surface());
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_border_color(bar, AppTheme::gridLine(), 0);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(bar);
    lv_label_set_text(title, "");
    lv_obj_set_style_text_color(title, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t *ta = lv_textarea_create(bar);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, 32);
    lv_obj_set_size(ta, LCD_H_RES - 100, 32);
    lv_obj_align(ta, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_set_style_bg_color(ta, AppTheme::bg(), 0);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(ta, AppTheme::text(), 0);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_20, 0);
    lv_obj_set_style_border_width(ta, 1, 0);
    lv_obj_set_style_border_color(ta, AppTheme::accent(), 0);
    lv_obj_set_style_radius(ta, 0, 0);
    lv_obj_set_style_pad_all(ta, 4, 0);
    lv_obj_clear_flag(ta, LV_OBJ_FLAG_SCROLLABLE);

    if (outTitle) {
        *outTitle = title;
    }
    if (outTa) {
        *outTa = ta;
    }
    return bar;
}

void showKbPreview(lv_obj_t *bar, lv_obj_t *kb, lv_obj_t *ta, lv_obj_t *titleLbl,
                   const char *title, const char *text, bool numberMode, uint32_t maxLen,
                   const char *acceptedChars) {
    if (!bar || !kb || !ta) {
        return;
    }
    if (titleLbl) {
        lv_label_set_text(titleLbl, title ? title : "");
    }
    lv_textarea_set_max_length(ta, maxLen > 0 ? maxLen : 32);
    if (numberMode) {
        lv_textarea_set_accepted_chars(ta, acceptedChars ? acceptedChars : "0123456789.");
        lv_obj_set_width(ta, 160);
        styleNumericKeyboard(kb);
    } else {
        /* nullptr = todos los caracteres ("" en LVGL rechaza todo). */
        lv_textarea_set_accepted_chars(ta, nullptr);
        lv_obj_set_width(ta, LCD_H_RES - 100);
        styleDarkKeyboard(kb);
        lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    }
    lv_obj_align(ta, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_textarea_set_text(ta, text ? text : "");
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(bar);
    lv_obj_move_foreground(kb);
    lv_obj_add_state(ta, LV_STATE_FOCUSED);
}

void hideKbPreview(lv_obj_t *bar, lv_obj_t *kb) {
    if (kb) {
        lv_keyboard_set_textarea(kb, nullptr);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
    if (bar) {
        lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
    }
}

void setPumpStatus(lv_obj_t *lbl, PumpUiStatus st, const char *msg) {
    if (!lbl) {
        return;
    }
    if (msg) {
        lv_label_set_text(lbl, msg);
    }
    lv_color_t c = AppTheme::muted();
    switch (st) {
    case PumpUiStatus::Active:
    case PumpUiStatus::Done:
        c = AppTheme::text();
        break;
    case PumpUiStatus::Stopped:
        c = AppTheme::warn();
        break;
    case PumpUiStatus::Idle:
    default:
        c = AppTheme::muted();
        break;
    }
    lv_obj_set_style_text_color(lbl, c, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
}

void showRebootSplash(const char *caption) {
    lv_obj_t *layer = lv_layer_sys();
    if (!layer) {
        layer = lv_scr_act();
    }
    if (!layer) {
        return;
    }

    lv_obj_t *ov = lv_obj_create(layer);
    lv_obj_remove_style_all(ov);
    lv_obj_set_size(ov, LCD_H_RES, LCD_V_RES);
    lv_obj_set_pos(ov, 0, 0);
    forceOpaqueBg(ov, lv_color_hex(0x000000));
    lv_obj_set_style_border_width(ov, 0, 0);
    lv_obj_set_style_pad_all(ov, 0, 0);
    lv_obj_set_style_radius(ov, 0, 0);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ov, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(ov);

    lv_obj_t *icon = lv_label_create(ov);
    lv_label_set_text(icon, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_color(icon, AppTheme::text(), 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_48, 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -28);

    lv_obj_t *cap = lv_label_create(ov);
    lv_label_set_text(cap, caption && caption[0] ? caption : "");
    lv_obj_set_style_text_color(cap, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(cap, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(cap, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(cap, LV_ALIGN_CENTER, 0, 36);

    lv_obj_update_layout(ov);
    lv_refr_now(nullptr);
    for (int i = 0; i < 8; ++i) {
        lv_timer_handler();
        delay(40);
    }
    delay(700);
}

}  // namespace UiKit
