#include "Screens.h"
#include "NavShell.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"

namespace {

void onBack(lv_event_t *) { NavShell::back(); }
void onCalib(lv_event_t *) { NavShell::goTo(ScreenId::CalibList); }
void onDisplay(lv_event_t *) { NavShell::goTo(ScreenId::DisplayReadings); }
void onUnits(lv_event_t *) { NavShell::goTo(ScreenId::Units); }

}  // namespace

lv_obj_t *Screens::createSensors(lv_obj_t *parent) {
    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_add_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root, Strings::tr(Msg::SensorsTitle), onBack);

    lv_coord_t y = AppTheme::MENU_LIST_TOP;
    const lv_coord_t step = AppTheme::MENU_ROW_H + 4;

    lv_obj_t *r0 = UiKit::makeMenuRow(root, Strings::tr(Msg::Calibration), onCalib, nullptr);
    lv_obj_align(r0, LV_ALIGN_TOP_MID, 0, y);
    y += step;

    lv_obj_t *r1 = UiKit::makeMenuRow(root, Strings::tr(Msg::DisplayReadingsMenu), onDisplay, nullptr);
    lv_obj_align(r1, LV_ALIGN_TOP_MID, 0, y);
    y += step;

    lv_obj_t *r2 = UiKit::makeMenuRow(root, Strings::tr(Msg::UnitsMenu), onUnits, nullptr);
    lv_obj_align(r2, LV_ALIGN_TOP_MID, 0, y);

    return root;
}

void Screens::refreshSensors(lv_obj_t *root) { (void)root; }
