#include "Screens.h"
#include "NavShell.h"
#include "MasterLink.h"
#include "PumpConfig.h"
#include "ReservoirConfig.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "DoseChannel.h"
#include "BoardPins.h"

#include <cstdio>

/** Prime: hold mientras se mantiene + Stop. */

namespace {

DoseChannel ch_ = DoseChannel::R1;
lv_obj_t *statusLbl = nullptr;
bool holding_ = false;
unsigned long holdStartMs_ = 0;

void setStatus(UiKit::PumpUiStatus st, const char *msg) {
    UiKit::setPumpStatus(statusLbl, st, msg);
}

void stopHold() {
    if (holding_) {
        const unsigned long elapsed = millis() - holdStartMs_;
        holding_ = false;
        MasterLink::sendDoseHold(doseChannelKey(ch_), false);
        const float min = elapsed / 60000.0f;
        const float ml = PumpConfig::flowMlPerMin(ch_) * min;
        if (ml > 0.05f) {
            PumpConfig::addDispensed(ch_, ml);
        }
    }
}

void onBack(lv_event_t *) {
    stopHold();
    NavShell::back();
}

void onHoldClick(lv_event_t *) {}

void onHold(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        if (!ReservoirConfig::manualDoseAllowed()) {
            setStatus(UiKit::PumpUiStatus::Stopped, Strings::tr(Msg::DoseBlockedArmed));
            return;
        }
        holding_ = true;
        holdStartMs_ = millis();
        MasterLink::sendDoseHold(doseChannelKey(ch_), true);
        setStatus(UiKit::PumpUiStatus::Active, Strings::tr(Msg::HoldOn));
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (holding_) {
            stopHold();
            setStatus(UiKit::PumpUiStatus::Stopped, Strings::tr(Msg::HoldOff));
        }
    }
}

void onStop(lv_event_t *) {
    stopHold();
    MasterLink::sendDoseStop(doseChannelKey(ch_));
    setStatus(UiKit::PumpUiStatus::Stopped, Strings::tr(Msg::StopSent));
}

}  // namespace

lv_obj_t *Screens::createPumpPrime(lv_obj_t *parent, DoseChannel channel) {
    ch_ = channel;
    holding_ = false;
    statusLbl = nullptr;

    char title[48];
    char name[32];
    PumpConfig::formatTitle(channel, name, sizeof(name));
    snprintf(title, sizeof(title), "%s", Strings::tr(Msg::PumpActionPrime));

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root, title, onBack);

    lv_coord_t y = AppTheme::MENU_LIST_TOP + 4;

    lv_obj_t *sub = lv_label_create(root);
    lv_label_set_text(sub, name);
    lv_obj_set_style_text_color(sub, AppTheme::text(), 0);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(sub, 12, y);
    y += 24;

    lv_obj_t *hint = lv_label_create(root);
    lv_label_set_text(hint, Strings::tr(Msg::PumpPrimeHint));
    UiKit::styleHint(hint);
    lv_obj_set_width(hint, LCD_H_RES - 24);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(hint, 12, y);
    lv_obj_update_layout(hint);
    y += lv_obj_get_height(hint) + 8;

    lv_obj_t *hold = UiKit::makePrimaryButton(root, Strings::tr(Msg::HoldBtn), onHoldClick);
    lv_obj_set_size(hold, LCD_H_RES - 16, AppTheme::BTN_PRIMARY_H + 8);
    lv_obj_set_pos(hold, 8, y);
    lv_obj_add_event_cb(hold, onHold, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(hold, onHold, LV_EVENT_RELEASED, nullptr);
    lv_obj_add_event_cb(hold, onHold, LV_EVENT_PRESS_LOST, nullptr);
    y += AppTheme::BTN_PRIMARY_H + 16;

    lv_obj_t *stop = UiKit::makeCautionButton(root, Strings::tr(Msg::StopBtn), onStop);
    lv_obj_set_size(stop, LCD_H_RES - 16, AppTheme::BTN_PRIMARY_H);
    lv_obj_set_pos(stop, 8, y);
    y += AppTheme::BTN_PRIMARY_H + 12;

    statusLbl = lv_label_create(root);
    lv_obj_set_width(statusLbl, LCD_H_RES - 24);
    lv_label_set_long_mode(statusLbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(statusLbl, 12, y);
    setStatus(UiKit::PumpUiStatus::Idle, Strings::tr(Msg::ReadyStatus));

    return root;
}

void Screens::refreshPumpPrime(lv_obj_t *root) { (void)root; }
