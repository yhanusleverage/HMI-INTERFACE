#include "Screens.h"
#include "NavShell.h"
#include "FactoryReset.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

namespace {

void onBack(lv_event_t *) { NavShell::back(); }

void onConfirm(lv_event_t *) {
    UiKit::showRebootSplash(Strings::tr(Msg::FactoryResetTitle));
    FactoryReset::wipeAndReboot();
}

}  // namespace

lv_obj_t *Screens::createFactoryReset(lv_obj_t *parent) {
    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root, Strings::tr(Msg::FactoryResetTitle), onBack);

    lv_obj_t *warn = lv_label_create(root);
    lv_label_set_text(warn, Strings::tr(Msg::FactoryResetWarn));
    lv_obj_set_style_text_color(warn, AppTheme::text(), 0);
    lv_obj_set_style_text_font(warn, &lv_font_montserrat_14, 0);
    lv_obj_set_width(warn, LCD_H_RES - 32);
    lv_label_set_long_mode(warn, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(warn, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(warn, LV_ALIGN_TOP_MID, 0, AppTheme::MENU_LIST_TOP);

    lv_obj_t *hint = lv_label_create(root);
    lv_label_set_text(hint, Strings::tr(Msg::FactoryResetHint));
    UiKit::styleHint(hint);
    lv_obj_set_width(hint, LCD_H_RES - 32);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(hint, warn, LV_ALIGN_OUT_BOTTOM_MID, 0, 12);

    lv_obj_t *ok =
        UiKit::makePrimaryButton(root, Strings::tr(Msg::FactoryResetConfirm), onConfirm);
    lv_obj_set_size(ok, 260, AppTheme::BTN_PRIMARY_H);
    lv_obj_align(ok, LV_ALIGN_BOTTOM_MID, 0, -16);

    return root;
}

void Screens::refreshFactoryReset(lv_obj_t *root) { (void)root; }
