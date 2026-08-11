#include "Screens.h"
#include "NavShell.h"
#include "PumpConfig.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "DoseChannel.h"
#include "BoardPins.h"

#include <cstdio>

/**
 * Hub por bomba: manual o pH Up/Down (misma UI; pH añade fila Relé).
 */

namespace {

DoseChannel ch_ = DoseChannel::R1;
lv_obj_t *headerLbl_ = nullptr;
lv_obj_t *relayHintLbl_ = nullptr;

void onBack(lv_event_t *) { NavShell::back(); }

void onPhRelay(lv_event_t *) { NavShell::goToPhRelay(NavShell::phPumpIsUp(ch_)); }

void onName(lv_event_t *) { NavShell::goTo(ScreenId::PumpName); }
void onPrime(lv_event_t *) { NavShell::goTo(ScreenId::PumpPrime); }
void onCalib(lv_event_t *) { NavShell::goTo(ScreenId::PumpCalib); }
void onQuick(lv_event_t *) { NavShell::goTo(ScreenId::DosingChannel); }
void onTime(lv_event_t *) { NavShell::goTo(ScreenId::PumpTimeDose); }
void onQty(lv_event_t *) { NavShell::goTo(ScreenId::PumpQuantity); }

void syncPumpActionsUi() {
    if (headerLbl_) {
        char title[40];
        if (NavShell::isPhPumpChannel(ch_)) {
            snprintf(title, sizeof(title), "%s",
                     Strings::tr(NavShell::phPumpIsUp(ch_) ? Msg::PhUpLabel : Msg::PhDownLabel));
        } else {
            PumpConfig::formatTitle(ch_, title, sizeof(title));
        }
        lv_label_set_text(headerLbl_, title);
    }
    if (relayHintLbl_ && NavShell::isPhPumpChannel(ch_)) {
        char rHint[56];
        PumpConfig::formatTitle(ch_, rHint, sizeof(rHint));
        lv_label_set_text(relayHintLbl_, rHint);
    }
}

}  // namespace

lv_obj_t *Screens::createPumpActions(lv_obj_t *parent, DoseChannel channel) {
    ch_ = channel;
    headerLbl_ = nullptr;
    relayHintLbl_ = nullptr;

    char title[40];
    if (NavShell::isPhPumpChannel(channel)) {
        snprintf(title, sizeof(title), "%s",
                 Strings::tr(NavShell::phPumpIsUp(channel) ? Msg::PhUpLabel : Msg::PhDownLabel));
    } else {
        PumpConfig::formatTitle(channel, title, sizeof(title));
    }

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root, title, onBack);
    headerLbl_ = lv_obj_get_child(root, 1);

    lv_obj_t *list = lv_obj_create(root);
    lv_obj_remove_style_all(list);
    const lv_coord_t top = AppTheme::MENU_LIST_TOP;
    lv_obj_set_size(list, LCD_H_RES, LCD_V_RES - top);
    lv_obj_align(list, LV_ALIGN_TOP_LEFT, 0, top);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_bottom(list, 8, 0);

    lv_coord_t y = 0;
    const lv_coord_t step = AppTheme::HUB_ROW_H + 4;

    if (NavShell::isPhPumpChannel(channel)) {
        char rHint[56];
        PumpConfig::formatTitle(channel, rHint, sizeof(rHint));
        lv_obj_t *relayRow =
            UiKit::makeHubMenuRow(list, Strings::tr(Msg::RulesRelay), rHint, onPhRelay, nullptr);
        lv_obj_set_pos(relayRow, 12, y);
        relayHintLbl_ = lv_obj_get_child(relayRow, 1);
        y += step;
    }

    struct Row {
        Msg title;
        Msg hint;
        lv_event_cb_t cb;
    };
    const Row rows[] = {
        {Msg::PumpActionName, Msg::PumpNameHint, onName},
        {Msg::PumpActionPrime, Msg::PumpPrimeHint, onPrime},
        {Msg::PumpActionCalib, Msg::PumpActionCalibHint, onCalib},
        {Msg::PumpActionQuick, Msg::PumpActionQuickHint, onQuick},
        {Msg::PumpActionTime, Msg::PumpActionTimeHint, onTime},
        {Msg::PumpActionQuantity, Msg::PumpQuantityHint, onQty},
    };
    for (const Row &r : rows) {
        lv_obj_t *row = UiKit::makeHubMenuRow(list, Strings::tr(r.title), Strings::tr(r.hint), r.cb,
                                              nullptr);
        lv_obj_set_pos(row, 12, y);
        y += step;
    }

    return root;
}

void Screens::refreshPumpActions(lv_obj_t *root) {
    (void)root;
    syncPumpActionsUi();
}
