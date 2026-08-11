#include "Screens.h"
#include "NavShell.h"
#include "NutrientConfig.h"
#include "PumpConfig.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "DoseChannel.h"
#include "BoardPins.h"

#include <cstdio>

namespace {

lv_obj_t *phUpHint_ = nullptr;
lv_obj_t *phDownHint_ = nullptr;
uint8_t lastPhUp_ = 255;
uint8_t lastPhDown_ = 255;

void onBack(lv_event_t *) { NavShell::back(); }
void onRecipe(lv_event_t *) { NavShell::goTo(ScreenId::Nutrients); }
void onManual(lv_event_t *) { NavShell::goTo(ScreenId::Dosing); }
void onPhUp(lv_event_t *) { NavShell::goToPhPumpActions(true); }
void onPhDown(lv_event_t *) { NavShell::goToPhPumpActions(false); }

void fillPhHint(uint8_t relay1to6, char *buf, size_t n) {
    if (!buf || n == 0) {
        return;
    }
    if (relay1to6 < 1 || relay1to6 > PUMP_RELAY_COUNT) {
        snprintf(buf, n, "%s", Strings::tr(Msg::DoseHubPhNeedRelay));
        return;
    }
    /* Alias de bomba (nutriente/pH) o bombaN — nunca concatenar ambos. */
    PumpConfig::formatTitle(doseFromRelayNumber(relay1to6), buf, n);
}

lv_obj_t *hubHintLabel(lv_obj_t *row) {
    /* makeHubMenuRow: title=0, hint=1, chevron=2 */
    return row ? lv_obj_get_child(row, 1) : nullptr;
}

void syncPhHints(bool force) {
    const uint8_t up = NutrientConfig::phUpRelay();
    const uint8_t down = NutrientConfig::phDownRelay();
    if (!force && up == lastPhUp_ && down == lastPhDown_) {
        return;
    }
    lastPhUp_ = up;
    lastPhDown_ = down;
    char buf[56];
    if (phUpHint_) {
        fillPhHint(up, buf, sizeof(buf));
        lv_label_set_text(phUpHint_, buf);
    }
    if (phDownHint_) {
        fillPhHint(down, buf, sizeof(buf));
        lv_label_set_text(phDownHint_, buf);
    }
}

}  // namespace

lv_obj_t *Screens::createDosingHub(lv_obj_t *parent) {
    phUpHint_ = nullptr;
    phDownHint_ = nullptr;
    lastPhUp_ = 255;
    lastPhDown_ = 255;

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);

    UiKit::styleHeader(root, Strings::tr(Msg::Dosing), onBack);

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

    lv_obj_t *r0 = UiKit::makeHubMenuRow(host, Strings::tr(Msg::DoseHubRecipe),
                                        Strings::tr(Msg::DoseHubRecipeHint), onRecipe, nullptr);
    lv_obj_set_pos(r0, 12, y);
    y += step;

    lv_obj_t *r1 = UiKit::makeHubMenuRow(host, Strings::tr(Msg::DoseHubManual),
                                        Strings::tr(Msg::DoseHubManualHint), onManual, nullptr);
    lv_obj_set_pos(r1, 12, y);
    y += step;

    char upHint[56];
    char downHint[56];
    fillPhHint(NutrientConfig::phUpRelay(), upHint, sizeof(upHint));
    fillPhHint(NutrientConfig::phDownRelay(), downHint, sizeof(downHint));

    lv_obj_t *r2 =
        UiKit::makeHubMenuRow(host, Strings::tr(Msg::PhUpLabel), upHint, onPhUp, nullptr);
    lv_obj_set_pos(r2, 12, y);
    phUpHint_ = hubHintLabel(r2);
    y += step;

    lv_obj_t *r3 =
        UiKit::makeHubMenuRow(host, Strings::tr(Msg::PhDownLabel), downHint, onPhDown, nullptr);
    lv_obj_set_pos(r3, 12, y);
    phDownHint_ = hubHintLabel(r3);

    lastPhUp_ = NutrientConfig::phUpRelay();
    lastPhDown_ = NutrientConfig::phDownRelay();
    return root;
}

void Screens::refreshDosingHub(lv_obj_t *root) {
    (void)root;
    syncPhHints(false);
}
