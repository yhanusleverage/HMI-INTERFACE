#include "Screens.h"
#include "NavShell.h"
#include "PumpConfig.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "DoseChannel.h"
#include "BoardPins.h"

#include <cstdio>

/** Quantity: total ml dispensados + reset. */

namespace {

DoseChannel ch_ = DoseChannel::R1;
lv_obj_t *qtyLbl = nullptr;

void refreshQty() {
    if (!qtyLbl) {
        return;
    }
    char buf[40];
    snprintf(buf, sizeof(buf), Strings::tr(Msg::PumpQuantityFmt), PumpConfig::totalMl(ch_));
    lv_label_set_text(qtyLbl, buf);
}

void onBack(lv_event_t *) { NavShell::back(); }

void onReset(lv_event_t *) {
    PumpConfig::resetTotal(ch_);
    refreshQty();
}

}  // namespace

lv_obj_t *Screens::createPumpQuantity(lv_obj_t *parent, DoseChannel channel) {
    ch_ = channel;
    qtyLbl = nullptr;

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root, Strings::tr(Msg::PumpActionQuantity), onBack);

    lv_coord_t y = AppTheme::MENU_LIST_TOP + 8;

    lv_obj_t *title = lv_label_create(root);
    lv_label_set_text(title, Strings::tr(Msg::PumpQuantityTitle));
    lv_obj_set_style_text_color(title, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(title, 12, y);
    y += 22;

    lv_obj_t *hint = lv_label_create(root);
    lv_label_set_text(hint, Strings::tr(Msg::PumpQuantityHint));
    UiKit::styleHint(hint);
    lv_obj_set_width(hint, LCD_H_RES - 24);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(hint, 12, y);
    lv_obj_update_layout(hint);
    y += lv_obj_get_height(hint) + 8;

    qtyLbl = lv_label_create(root);
    lv_obj_set_style_text_color(qtyLbl, AppTheme::text(), 0);
    lv_obj_set_style_text_font(qtyLbl, &lv_font_montserrat_28, 0);
    lv_obj_set_width(qtyLbl, LCD_H_RES);
    lv_obj_set_style_text_align(qtyLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(qtyLbl, 0, y);
    refreshQty();
    y += 48;

    lv_obj_t *reset = UiKit::makeCautionButton(root, Strings::tr(Msg::PumpQuantityReset), onReset);
    lv_obj_set_size(reset, LCD_H_RES - 16, AppTheme::BTN_PRIMARY_H);
    lv_obj_set_pos(reset, 8, y);

    return root;
}

void Screens::refreshPumpQuantity(lv_obj_t *root) {
    (void)root;
    refreshQty();
}
