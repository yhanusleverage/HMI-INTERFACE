#include "Screens.h"
#include "NavShell.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

namespace {

void onBack(lv_event_t *) { NavShell::back(); }
void onNiveles(lv_event_t *) { NavShell::goTo(ScreenId::Niveles); }
void onAuto(lv_event_t *) { NavShell::goTo(ScreenId::ControleAuto); }
void onReservoir(lv_event_t *) { NavShell::goTo(ScreenId::Reservoir); }

}  // namespace

lv_obj_t *Screens::createControle(lv_obj_t *parent) {
    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);

    UiKit::styleHeader(root, Strings::tr(Msg::ControleTitle), onBack);

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
    const lv_coord_t step = AppTheme::HUB_ROW_H + 4;

    lv_obj_t *r0 = UiKit::makeHubMenuRow(host, Strings::tr(Msg::Levels),
                                        Strings::tr(Msg::ControleNivelesHint), onNiveles, nullptr);
    lv_obj_set_pos(r0, 12, y);
    y += step;

    lv_obj_t *r1 = UiKit::makeHubMenuRow(host, Strings::tr(Msg::ControleAuto),
                                        Strings::tr(Msg::ControleAutoHint), onAuto, nullptr);
    lv_obj_set_pos(r1, 12, y);
    y += step;

    lv_obj_t *r2 = UiKit::makeHubMenuRow(host, Strings::tr(Msg::ReservoirTitle),
                                        Strings::tr(Msg::ControleReservoirHint), onReservoir,
                                        nullptr);
    lv_obj_set_pos(r2, 12, y);

    return root;
}

void Screens::refreshControle(lv_obj_t *root) { (void)root; }
