#include "Screens.h"
#include "NavShell.h"
#include "BacklightIdle.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

namespace {

lv_obj_t *rows_[3] = {};

void onBack(lv_event_t *) { NavShell::back(); }

void paintRows() {
    const BacklightIdle::Mode cur = BacklightIdle::mode();
    for (uint8_t i = 0; i < 3; ++i) {
        if (!rows_[i]) {
            continue;
        }
        UiKit::applySelectionStyle(rows_[i], static_cast<uint8_t>(cur) == i, false);
    }
}

void onPick(lv_event_t *e) {
    const auto m = static_cast<BacklightIdle::Mode>(
        reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    BacklightIdle::setMode(m);
    paintRows();
}

}  // namespace

lv_obj_t *Screens::createBacklight(lv_obj_t *parent) {
    rows_[0] = rows_[1] = rows_[2] = nullptr;

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);

    UiKit::styleHeader(root, Strings::tr(Msg::BacklightTitle), onBack);

    lv_obj_t *hint = lv_label_create(root);
    lv_label_set_text(hint, Strings::tr(Msg::BacklightHint));
    UiKit::styleHint(hint);
    lv_obj_set_width(hint, LCD_H_RES - 24);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 12, AppTheme::MENU_LIST_TOP);
    lv_obj_update_layout(hint);
    const lv_coord_t top = AppTheme::MENU_LIST_TOP + lv_obj_get_height(hint) + 8;

    lv_obj_t *list = lv_obj_create(root);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, LCD_H_RES, LCD_V_RES - top);
    lv_obj_align(list, LV_ALIGN_TOP_LEFT, 0, top);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);

    const Msg labels[3] = {Msg::BacklightAlways, Msg::BacklightAuto, Msg::BacklightForcedOff};
    lv_coord_t y = 0;
    const lv_coord_t step = AppTheme::MENU_ROW_H + 4;
    for (uint8_t i = 0; i < 3; ++i) {
        rows_[i] = UiKit::makeMenuRow(list, Strings::tr(labels[i]), onPick,
                                      reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
        lv_obj_set_pos(rows_[i], 12, y);
        y += step;
    }

    paintRows();
    return root;
}

void Screens::refreshBacklight(lv_obj_t *root) {
    (void)root;
    paintRows();
}
