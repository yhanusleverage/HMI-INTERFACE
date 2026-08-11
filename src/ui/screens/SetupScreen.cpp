#include "Screens.h"
#include "NavShell.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"

namespace {

void onBack(lv_event_t *) { NavShell::back(); }
void onDisplayWifi(lv_event_t *) { NavShell::goTo(ScreenId::MasterWifi); }
void onLanguage(lv_event_t *) { NavShell::goTo(ScreenId::Language); }
void onTimeZone(lv_event_t *) { NavShell::goTo(ScreenId::TimeZone); }
void onBacklight(lv_event_t *) { NavShell::goTo(ScreenId::Backlight); }
void onFactory(lv_event_t *) { NavShell::goTo(ScreenId::FactoryReset); }

}  // namespace

lv_obj_t *Screens::createSetup(lv_obj_t *parent) {
    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_add_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root, Strings::tr(Msg::SetupTitle), onBack);

    lv_coord_t y = AppTheme::MENU_LIST_TOP;
    const lv_coord_t step = AppTheme::MENU_ROW_H + 4;

    lv_obj_t *r0 = UiKit::makeMenuRow(root, Strings::tr(Msg::Wifi), onDisplayWifi, nullptr);
    lv_obj_align(r0, LV_ALIGN_TOP_MID, 0, y);
    y += step;

    lv_obj_t *r3 = UiKit::makeMenuRow(root, Strings::tr(Msg::LanguageMenu), onLanguage, nullptr);
    lv_obj_align(r3, LV_ALIGN_TOP_MID, 0, y);
    y += step;

    lv_obj_t *r4 = UiKit::makeMenuRow(root, Strings::tr(Msg::TimeZoneMenu), onTimeZone, nullptr);
    lv_obj_align(r4, LV_ALIGN_TOP_MID, 0, y);
    y += step;

    lv_obj_t *rBl = UiKit::makeMenuRow(root, Strings::tr(Msg::BacklightTitle), onBacklight, nullptr);
    lv_obj_align(rBl, LV_ALIGN_TOP_MID, 0, y);
    y += step;

    lv_obj_t *r5 = UiKit::makeMenuRow(root, Strings::tr(Msg::FactoryResetMenu), onFactory, nullptr);
    lv_obj_align(r5, LV_ALIGN_TOP_MID, 0, y);

    return root;
}

void Screens::refreshSetup(lv_obj_t *root) { (void)root; }
