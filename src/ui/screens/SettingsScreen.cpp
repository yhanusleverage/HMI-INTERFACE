#include "Screens.h"
#include "NavShell.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

namespace {

void onBack(lv_event_t *) { NavShell::back(); }
void onControle(lv_event_t *) { NavShell::goTo(ScreenId::Controle); }
void onRelays(lv_event_t *) { NavShell::goTo(ScreenId::RelaysHub); }
void onDosingHub(lv_event_t *) { NavShell::goTo(ScreenId::DosingHub); }
void onSetup(lv_event_t *) { NavShell::goTo(ScreenId::Setup); }
void onSystem(lv_event_t *) { NavShell::goTo(ScreenId::System); }
void onSensors(lv_event_t *) { NavShell::goTo(ScreenId::Sensors); }

}  // namespace

lv_obj_t *Screens::createSettings(lv_obj_t *parent) {
    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);

    UiKit::styleHeader(root, Strings::tr(Msg::SettingsTitle), onBack);

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

    lv_coord_t y = 0;
    const lv_coord_t step = AppTheme::MENU_ROW_H + 4;

    lv_obj_t *r0 = UiKit::makeMenuRow(host, Strings::tr(Msg::ControleTitle), onControle, nullptr);
    lv_obj_set_pos(r0, 12, y);
    y += step;

    lv_obj_t *rRelays = UiKit::makeMenuRow(host, Strings::tr(Msg::RelaysMenu), onRelays, nullptr);
    lv_obj_set_pos(rRelays, 12, y);
    y += step;

    lv_obj_t *rDose = UiKit::makeMenuRow(host, Strings::tr(Msg::Dosing), onDosingHub, nullptr);
    lv_obj_set_pos(rDose, 12, y);
    y += step;

    lv_obj_t *r2 = UiKit::makeMenuRow(host, Strings::tr(Msg::SetupMenu), onSetup, nullptr);
    lv_obj_set_pos(r2, 12, y);
    y += step;

    lv_obj_t *r3 = UiKit::makeMenuRow(host, Strings::tr(Msg::System), onSystem, nullptr);
    lv_obj_set_pos(r3, 12, y);
    y += step;

    lv_obj_t *r4 = UiKit::makeMenuRow(host, Strings::tr(Msg::SensorsMenu), onSensors, nullptr);
    lv_obj_set_pos(r4, 12, y);

    return root;
}

void Screens::refreshSettings(lv_obj_t *root) { (void)root; }
