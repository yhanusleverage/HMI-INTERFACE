#include "Screens.h"
#include "NavShell.h"
#include "DataStore.h"
#include "HydroRanges.h"
#include "NutrientConfig.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

namespace {

void onBack(lv_event_t *) { NavShell::back(); }

void onPick(lv_event_t *e) {
    const ParamId id = static_cast<ParamId>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    NavShell::goToParam(id, true);
}

void onResetNiveles(lv_event_t *) {
    DataStore &store = DataStore::instance();
    ParamConfig defaults[static_cast<size_t>(ParamId::Count)];
    HydroRanges::applyDefaults(defaults);
    const ParamId ids[] = {ParamId::Ph, ParamId::Ec, ParamId::Orp, ParamId::Do};
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i) {
        const ParamId id = ids[i];
        ParamConfig cfg = store.config(id);
        const size_t ix = static_cast<size_t>(id);
        cfg.setpoint = defaults[ix].setpoint;
        cfg.low = defaults[ix].low;
        cfg.high = defaults[ix].high;
        store.setConfig(id, cfg);
    }
    /* Una fuente: HydroRanges → NutrientConfig (EC/pH) → store ya arriba. */
    NutrientConfig::setEcLow(defaults[static_cast<size_t>(ParamId::Ec)].low);
    NutrientConfig::setEcHigh(defaults[static_cast<size_t>(ParamId::Ec)].high);
    NutrientConfig::setPhLow(defaults[static_cast<size_t>(ParamId::Ph)].low);
    NutrientConfig::setPhHigh(defaults[static_cast<size_t>(ParamId::Ph)].high);
}

}  // namespace

lv_obj_t *Screens::createNiveles(lv_obj_t *parent) {
    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);

    UiKit::styleHeader(root, Strings::tr(Msg::Levels), onBack);

    const lv_coord_t top = AppTheme::MENU_LIST_TOP;
    lv_obj_t *host = lv_obj_create(root);
    lv_obj_remove_style_all(host);
    lv_obj_set_size(host, LCD_H_RES, LCD_V_RES - top);
    lv_obj_align(host, LV_ALIGN_TOP_LEFT, 0, top);
    lv_obj_set_style_bg_opa(host, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(host, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(host, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(host, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_bottom(host, 8, 0);

    DataStore &store = DataStore::instance();
    const ParamId ids[] = {ParamId::Ph, ParamId::Ec, ParamId::Orp, ParamId::Do};
    lv_coord_t y = 0;
    const lv_coord_t step = AppTheme::MENU_ROW_H + 4;
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i) {
        lv_obj_t *row = UiKit::makeMenuRow(host, store.labelWithUnit(ids[i]), onPick,
                                           reinterpret_cast<void *>(static_cast<uintptr_t>(ids[i])));
        lv_obj_set_pos(row, 12, y);
        y += step;
    }

    lv_obj_t *rst = UiKit::makeCautionButton(host, Strings::tr(Msg::ResetLevels), onResetNiveles);
    lv_obj_set_pos(rst, 20, y + 4);

    return root;
}

void Screens::refreshNiveles(lv_obj_t *root) { (void)root; }
