#include "Screens.h"
#include "NavShell.h"
#include "RelayAliasConfig.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

#include <cstdio>

/**
 * Hub por relé Atlas: Nombre · Accionamiento.
 */

namespace {

void onBack(lv_event_t *) { NavShell::back(); }
void onName(lv_event_t *) { NavShell::goToAtlasScreen(ScreenId::RelayName); }
void onActuation(lv_event_t *) { NavShell::goToAtlasScreen(ScreenId::RelayActuation); }

void formatHeader(char *buf, size_t n) {
    const char *mac = NavShell::currentAtlasMac();
    const uint8_t r = NavShell::currentAtlasRelay();
    const char *alias = RelayAliasConfig::getName(mac, r);
    if (alias && alias[0]) {
        snprintf(buf, n, "%s", alias);
        return;
    }
    snprintf(buf, n, Strings::tr(Msg::AtlasRelayFmt), static_cast<int>(r + 1));
}

}  // namespace

lv_obj_t *Screens::createRelayActions(lv_obj_t *parent) {
    char title[40];
    formatHeader(title, sizeof(title));

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root, title, onBack);

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

    lv_obj_t *r0 = UiKit::makeHubMenuRow(list, Strings::tr(Msg::RelayActionName),
                                        Strings::tr(Msg::RelayActionNameHint), onName, nullptr);
    lv_obj_set_pos(r0, 12, y);
    y += step;

    lv_obj_t *r1 =
        UiKit::makeHubMenuRow(list, Strings::tr(Msg::RelayActionActuation),
                             Strings::tr(Msg::RelayActionActuationHint), onActuation, nullptr);
    lv_obj_set_pos(r1, 12, y);

    return root;
}

void Screens::refreshRelayActions(lv_obj_t *root) { (void)root; }
