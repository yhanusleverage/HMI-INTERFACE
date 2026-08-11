#include "Screens.h"
#include "NavShell.h"
#include "UnitsConfig.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

namespace {

lv_obj_t *ecUsBtn = nullptr;
lv_obj_t *ec500Btn = nullptr;
lv_obj_t *ec640Btn = nullptr;
lv_obj_t *tempCBtn = nullptr;
lv_obj_t *tempFBtn = nullptr;

void onBack(lv_event_t *) { NavShell::back(); }

void stylePick(lv_obj_t *btn, bool on) {
    if (!btn) {
        return;
    }
    lv_obj_set_style_bg_color(btn, on ? AppTheme::accent() : AppTheme::surface(), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
}

void refreshPicks() {
    stylePick(ecUsBtn, UnitsConfig::ecUnit() == EcUnit::Us);
    stylePick(ec500Btn, UnitsConfig::ecUnit() == EcUnit::Ppm500);
    stylePick(ec640Btn, UnitsConfig::ecUnit() == EcUnit::Ppm640);
    stylePick(tempCBtn, UnitsConfig::tempUnit() == TempUnit::Celsius);
    stylePick(tempFBtn, UnitsConfig::tempUnit() == TempUnit::Fahrenheit);
}

void onEcUs(lv_event_t *) {
    UnitsConfig::setEcUnit(EcUnit::Us);
    refreshPicks();
}
void onEc500(lv_event_t *) {
    UnitsConfig::setEcUnit(EcUnit::Ppm500);
    refreshPicks();
}
void onEc640(lv_event_t *) {
    UnitsConfig::setEcUnit(EcUnit::Ppm640);
    refreshPicks();
}
void onTempC(lv_event_t *) {
    UnitsConfig::setTempUnit(TempUnit::Celsius);
    refreshPicks();
}
void onTempF(lv_event_t *) {
    UnitsConfig::setTempUnit(TempUnit::Fahrenheit);
    refreshPicks();
}

lv_obj_t *makePick(lv_obj_t *parent, const char *txt, lv_coord_t x, lv_coord_t y, lv_coord_t w,
                   lv_event_cb_t cb) {
    lv_obj_t *btn = UiKit::makeSecondaryButton(parent, txt, w, 36, cb);
    lv_obj_set_pos(btn, x, y);
    return btn;
}

}  // namespace

lv_obj_t *Screens::createUnits(lv_obj_t *parent) {
    ecUsBtn = ec500Btn = ec640Btn = tempCBtn = tempFBtn = nullptr;

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root, Strings::tr(Msg::UnitsTitle), onBack);

    lv_coord_t y = AppTheme::MENU_LIST_TOP;

    lv_obj_t *ecLbl = lv_label_create(root);
    lv_label_set_text(ecLbl, Strings::tr(Msg::UnitsEcLabel));
    lv_obj_set_style_text_color(ecLbl, AppTheme::text(), 0);
    lv_obj_align(ecLbl, LV_ALIGN_TOP_LEFT, 16, y);
    lv_obj_update_layout(ecLbl);
    y += lv_obj_get_height(ecLbl) + 8;

    const lv_coord_t bw = (LCD_H_RES - 40) / 3;
    ecUsBtn = makePick(root, Strings::tr(Msg::EcUnitUs), 12, y, bw, onEcUs);
    ec500Btn = makePick(root, Strings::tr(Msg::EcUnitPpm500), 16 + bw, y, bw, onEc500);
    ec640Btn = makePick(root, Strings::tr(Msg::EcUnitPpm640), 20 + 2 * bw, y, bw, onEc640);
    y += 36 + 16;

    lv_obj_t *tLbl = lv_label_create(root);
    lv_label_set_text(tLbl, Strings::tr(Msg::UnitsTempLabel));
    lv_obj_set_style_text_color(tLbl, AppTheme::text(), 0);
    lv_obj_align(tLbl, LV_ALIGN_TOP_LEFT, 16, y);
    lv_obj_update_layout(tLbl);
    y += lv_obj_get_height(tLbl) + 8;

    const lv_coord_t tw = (LCD_H_RES - 36) / 2;
    tempCBtn = makePick(root, Strings::tr(Msg::TempUnitC), 12, y, tw, onTempC);
    tempFBtn = makePick(root, Strings::tr(Msg::TempUnitF), 20 + tw, y, tw, onTempF);

    refreshPicks();
    return root;
}

void Screens::refreshUnits(lv_obj_t *root) {
    (void)root;
    refreshPicks();
}
