#include "Screens.h"
#include "NavShell.h"
#include "DataStore.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"

#include <cstdio>

namespace {

void onBack(lv_event_t *) { NavShell::back(); }

void onPick(lv_event_t *e) {
    const ParamId id = static_cast<ParamId>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    NavShell::goTo(ScreenId::Calib, id);
}

}  // namespace

lv_obj_t *Screens::createCalibList(lv_obj_t *parent) {
    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);

    UiKit::styleHeader(root, Strings::tr(Msg::Calibration), onBack);

    DataStore &store = DataStore::instance();
    const ParamId ids[] = {ParamId::Ph, ParamId::Ec, ParamId::Orp, ParamId::Do};
    lv_coord_t y = AppTheme::MENU_LIST_TOP;
    const lv_coord_t step = AppTheme::MENU_ROW_H + 4;
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i) {
        char label[40];
        snprintf(label, sizeof(label), Strings::tr(Msg::CalibrateFmt), store.name(ids[i]));
        lv_obj_t *row = UiKit::makeMenuRow(root, label, onPick,
                                           reinterpret_cast<void *>(static_cast<uintptr_t>(ids[i])));
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y);
        y += step;
    }

    return root;
}

void Screens::refreshCalibList(lv_obj_t *root) { (void)root; }
