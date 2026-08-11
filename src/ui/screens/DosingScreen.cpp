#include "Screens.h"
#include "NavShell.h"
#include "PumpConfig.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "DoseChannel.h"
#include "BoardPins.h"

#include <cstdio>

namespace {

void onBack(lv_event_t *) { NavShell::back(); }

void onPick(lv_event_t *e) {
    const DoseChannel ch =
        static_cast<DoseChannel>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    NavShell::goToDose(ch);
}

}  // namespace

lv_obj_t *Screens::createDosing(lv_obj_t *parent) {
    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root, Strings::tr(Msg::DoseHubManual), onBack);

    lv_obj_t *hint = lv_label_create(root);
    lv_label_set_text(hint, Strings::tr(Msg::DoseHubManualHint));
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
    lv_obj_set_style_pad_bottom(list, 8, 0);

    lv_coord_t y = 0;
    const lv_coord_t step = AppTheme::MENU_ROW_H + 4;
    char lab[40];

    for (uint8_t i = 0; i < PUMP_RELAY_COUNT; ++i) {
        const DoseChannel ch = static_cast<DoseChannel>(i);
        PumpConfig::formatTitle(ch, lab, sizeof(lab));
        lv_obj_t *row = UiKit::makeMenuRow(list, lab, onPick,
                                           reinterpret_cast<void *>(static_cast<uintptr_t>(ch)));
        lv_obj_set_pos(row, 12, y);
        y += step;
    }

    return root;
}

void Screens::refreshDosing(lv_obj_t *root) { (void)root; }
