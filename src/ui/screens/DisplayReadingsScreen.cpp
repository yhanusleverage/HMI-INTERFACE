#include "Screens.h"
#include "NavShell.h"
#include "DisplayConfig.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

/**
 * Display Readings: pares ON | OFF bien visibles (no lv_switch).
 * OFF en Central → valor "--".
 */

namespace {

struct Row {
    lv_obj_t *onBtn;
    lv_obj_t *offBtn;
    bool (*getter)();
    void (*setter)(bool);
};

Row rows_[5] = {};

void onBack(lv_event_t *) { NavShell::back(); }

void paintRow(Row &r) {
    if (!r.onBtn || !r.offBtn || !r.getter) {
        return;
    }
    const bool on = r.getter();
    lv_obj_set_style_bg_color(r.onBtn, on ? AppTheme::accent() : AppTheme::surfaceAlt(), 0);
    lv_obj_set_style_bg_opa(r.onBtn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(r.offBtn, !on ? AppTheme::alarm() : AppTheme::surfaceAlt(), 0);
    lv_obj_set_style_bg_opa(r.offBtn, LV_OPA_COVER, 0);

    lv_obj_t *onLbl = lv_obj_get_child(r.onBtn, 0);
    lv_obj_t *offLbl = lv_obj_get_child(r.offBtn, 0);
    if (onLbl) {
        lv_obj_set_style_text_color(onLbl, on ? AppTheme::bg() : AppTheme::text(), 0);
        lv_obj_set_style_text_opa(onLbl, LV_OPA_COVER, 0);
    }
    if (offLbl) {
        lv_obj_set_style_text_color(offLbl, !on ? AppTheme::text() : AppTheme::muted(), 0);
        lv_obj_set_style_text_opa(offLbl, LV_OPA_COVER, 0);
    }
}

void syncAll() {
    for (int i = 0; i < 5; ++i) {
        paintRow(rows_[i]);
    }
}

void onOn(lv_event_t *e) {
    Row *r = static_cast<Row *>(lv_event_get_user_data(e));
    if (r && r->setter) {
        r->setter(true);
    }
    syncAll();
}

void onOff(lv_event_t *e) {
    Row *r = static_cast<Row *>(lv_event_get_user_data(e));
    if (r && r->setter) {
        r->setter(false);
    }
    syncAll();
}

lv_obj_t *makeSegBtn(lv_obj_t *parent, const char *txt, lv_event_cb_t cb, void *ud) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, 72, 38);
    lv_obj_set_style_radius(btn, 4, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    UiKit::applyPressStyle(btn, AppTheme::surfaceAlt(), AppTheme::surface(), AppTheme::gridLine(),
                           AppTheme::accent());
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, ud);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, txt);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_opa(lbl, LV_OPA_COVER, 0);
    lv_obj_center(lbl);
    return btn;
}

void makeToggleRow(lv_obj_t *parent, const char *label, lv_coord_t y, int ix, bool (*getter)(),
                   void (*setter)(bool)) {
    rows_[ix].getter = getter;
    rows_[ix].setter = setter;

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LCD_H_RES - 12, AppTheme::MENU_ROW_H);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_color(row, AppTheme::surface(), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, AppTheme::gridLine(), 0);
    lv_obj_set_style_pad_hor(row, 8, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    lv_obj_t *name = lv_label_create(row);
    lv_label_set_text(name, label);
    lv_obj_set_style_text_color(name, AppTheme::text(), 0);
    lv_obj_set_style_text_opa(name, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_16, 0);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 4, 0);

    void *ud = &rows_[ix];
    rows_[ix].onBtn = makeSegBtn(row, Strings::tr(Msg::OnLabel), onOn, ud);
    lv_obj_align(rows_[ix].onBtn, LV_ALIGN_RIGHT_MID, -80, 0);
    rows_[ix].offBtn = makeSegBtn(row, Strings::tr(Msg::OffLabel), onOff, ud);
    lv_obj_align(rows_[ix].offBtn, LV_ALIGN_RIGHT_MID, -4, 0);
}

}  // namespace

lv_obj_t *Screens::createDisplayReadings(lv_obj_t *parent) {
    for (int i = 0; i < 5; ++i) {
        rows_[i] = {};
    }

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_add_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(root, LV_DIR_VER);

    UiKit::styleHeader(root, Strings::tr(Msg::DisplayReadingsTitle), onBack);

    lv_coord_t y = 44;
    constexpr lv_coord_t step = AppTheme::MENU_ROW_H + 4;

    makeToggleRow(root, Strings::tr(Msg::DisplayEc), y, 1, DisplayConfig::showEc,
                  DisplayConfig::setShowEc);
    y += step;
    makeToggleRow(root, Strings::tr(Msg::DisplayPh), y, 0, DisplayConfig::showPh,
                  DisplayConfig::setShowPh);
    y += step;
    makeToggleRow(root, Strings::tr(Msg::DisplayOrp), y, 3, DisplayConfig::showOrp,
                  DisplayConfig::setShowOrp);
    y += step;
    makeToggleRow(root, Strings::tr(Msg::DisplayTemp), y, 2, DisplayConfig::showTemp,
                  DisplayConfig::setShowTemp);
    y += step;
    makeToggleRow(root, Strings::tr(Msg::DisplayDo), y, 4, DisplayConfig::showDo,
                  DisplayConfig::setShowDo);

    syncAll();
    return root;
}

void Screens::refreshDisplayReadings(lv_obj_t *root) {
    (void)root;
    syncAll();
}
