#include "Screens.h"
#include "NavShell.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

/**
 * Accionamiento: ON/OFF · Ciclo · Timer.
 */

namespace {

void onBack(lv_event_t *) { NavShell::back(); }
void onOnOff(lv_event_t *) { NavShell::goToAtlasScreen(ScreenId::RelayOnOff); }
void onCycle(lv_event_t *) { NavShell::goToAtlasScreen(ScreenId::RelayCycle); }
void onTimer(lv_event_t *) { NavShell::goToAtlasScreen(ScreenId::RelayTimer); }

}  // namespace

lv_obj_t *Screens::createRelayActuation(lv_obj_t *parent) {
    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root, Strings::tr(Msg::RelayActionActuation), onBack);

    lv_obj_t *list = lv_obj_create(root);
    lv_obj_remove_style_all(list);
    const lv_coord_t top = AppTheme::MENU_LIST_TOP;
    lv_obj_set_size(list, LCD_H_RES, LCD_V_RES - top);
    lv_obj_align(list, LV_ALIGN_TOP_LEFT, 0, top);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_style_pad_bottom(list, 8, 0);

    lv_coord_t y = 0;
    const lv_coord_t step = AppTheme::HUB_ROW_H + 4;

    lv_obj_t *r0 = UiKit::makeHubMenuRow(list, Strings::tr(Msg::RelaysOnOffTitle),
                                        Strings::tr(Msg::RelaysOnOffHint), onOnOff, nullptr);
    lv_obj_set_pos(r0, 12, y);
    y += step;

    lv_obj_t *r1 = UiKit::makeHubMenuRow(list, Strings::tr(Msg::RelaysHubCycle),
                                        Strings::tr(Msg::RelaysHubCycleHint), onCycle, nullptr);
    lv_obj_set_pos(r1, 12, y);
    y += step;

    lv_obj_t *r2 = UiKit::makeHubMenuRow(list, Strings::tr(Msg::RelaysHubTimer),
                                        Strings::tr(Msg::RelaysHubTimerHint), onTimer, nullptr);
    lv_obj_set_pos(r2, 12, y);

    return root;
}

void Screens::refreshRelayActuation(lv_obj_t *root) { (void)root; }
