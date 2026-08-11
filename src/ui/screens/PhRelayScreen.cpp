#include "Screens.h"
#include "NavShell.h"
#include "NutrientConfig.h"
#include "PumpConfig.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "DoseChannel.h"
#include "BoardPins.h"

/**
 * Asignación de bomba pH: lista estilo Manual (makeMenuRow) + Guardar real.
 * Tap = draft; Guardar = NVS + hub PumpActions; Atras = descarta draft.
 */

namespace {

lv_obj_t *optRows_[PUMP_RELAY_COUNT + 1] = {};
lv_obj_t *saveBtn_ = nullptr;
uint8_t draftRelay_ = 0;

bool phIsUp() { return NavShell::phRelayIsUp(); }

uint8_t persistedRelay() {
    return phIsUp() ? NutrientConfig::phUpRelay() : NutrientConfig::phDownRelay();
}

uint8_t otherPhRelay() {
    return phIsUp() ? NutrientConfig::phDownRelay() : NutrientConfig::phUpRelay();
}

bool relayBusy(uint8_t relayOrZero) {
    if (relayOrZero == 0) {
        return false;
    }
    return otherPhRelay() == relayOrZero || NutrientConfig::relaySharedByNutrient(relayOrZero);
}

void styleRow(lv_obj_t *row, uint8_t relayOrZero) {
    if (!row) {
        return;
    }
    UiKit::applySelectionStyle(row, draftRelay_ == relayOrZero, relayBusy(relayOrZero));
}

void refreshPickUi() {
    for (uint8_t i = 0; i <= PUMP_RELAY_COUNT; ++i) {
        styleRow(optRows_[i], i);
    }
    if (saveBtn_) {
        lv_obj_clear_flag(saveBtn_, LV_OBJ_FLAG_HIDDEN);
    }
}

void onBack(lv_event_t *) { NavShell::back(); }

void onPick(lv_event_t *e) {
    const uint8_t r =
        static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    if (relayBusy(r)) {
        return;
    }
    draftRelay_ = r;
    refreshPickUi();
}

void onSave(lv_event_t *) {
    if (relayBusy(draftRelay_)) {
        return;
    }
    if (phIsUp()) {
        NutrientConfig::setPhUpRelay(draftRelay_);
    } else {
        NutrientConfig::setPhDownRelay(draftRelay_);
    }
    NavShell::completePhRelayPick(phIsUp());
}

}  // namespace

lv_obj_t *Screens::createPhRelay(lv_obj_t *parent) {
    for (uint8_t i = 0; i <= PUMP_RELAY_COUNT; ++i) {
        optRows_[i] = nullptr;
    }
    saveBtn_ = nullptr;
    draftRelay_ = persistedRelay();

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);

    const bool up = phIsUp();
    UiKit::styleHeader(root, Strings::tr(up ? Msg::PhUpLabel : Msg::PhDownLabel), onBack);

    const lv_coord_t top = AppTheme::MENU_LIST_TOP;
    const lv_coord_t bottomReserve = AppTheme::BTN_PRIMARY_H + 24;
    lv_obj_t *list = lv_obj_create(root);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, LCD_H_RES, LCD_V_RES - top - bottomReserve);
    lv_obj_align(list, LV_ALIGN_TOP_LEFT, 0, top);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_bottom(list, 8, 0);

    lv_coord_t y = 0;
    const lv_coord_t step = AppTheme::MENU_ROW_H + 4;

    optRows_[0] = UiKit::makeMenuRow(list, "--", onPick,
                                    reinterpret_cast<void *>(static_cast<uintptr_t>(0)));
    lv_obj_set_pos(optRows_[0], 12, y);
    y += step;

    char lab[40];
    for (uint8_t r = 1; r <= PUMP_RELAY_COUNT; ++r) {
        PumpConfig::formatTitle(doseFromRelayNumber(r), lab, sizeof(lab));
        optRows_[r] = UiKit::makeMenuRow(list, lab, onPick,
                                        reinterpret_cast<void *>(static_cast<uintptr_t>(r)));
        lv_obj_set_pos(optRows_[r], 12, y);
        y += step;
    }

    saveBtn_ = UiKit::makePrimaryButton(root, Strings::tr(Msg::NutSave), onSave);
    lv_obj_align(saveBtn_, LV_ALIGN_BOTTOM_MID, 0, -12);

    refreshPickUi();
    return root;
}

void Screens::refreshPhRelay(lv_obj_t *root) {
    (void)root;
    refreshPickUi();
}
