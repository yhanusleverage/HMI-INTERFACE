#include "Screens.h"
#include "NavShell.h"
#include "RelayAliasConfig.h"
#include "RelayActuationLock.h"
#include "RelayCycleConfig.h"
#include "SlaveInventory.h"
#include "MasterLink.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

#include <cstdio>
#include <cstring>

/** Timer digital Atlas: ON min + OFF min, alterna hasta Stop. */

namespace {

constexpr lv_coord_t kListTopY = AppTheme::MENU_LIST_TOP;
constexpr lv_coord_t kRowW = LCD_H_RES - 24;
constexpr lv_coord_t kBtnH = AppTheme::TOUCH_MIN_H;

enum class Phase : uint8_t { Idle, On, Off };

lv_obj_t *listHost_ = nullptr;
lv_obj_t *lockLbl_ = nullptr;
lv_obj_t *phaseLbl_ = nullptr;
lv_obj_t *onValLbl_ = nullptr;
lv_obj_t *offValLbl_ = nullptr;
lv_obj_t *statusLbl_ = nullptr;
lv_obj_t *startBtn_ = nullptr;
lv_obj_t *stopBtn_ = nullptr;

char slotMac_[SlaveInventory::kMacLen] = {};
uint8_t slotRelay_ = 0;
bool slotOnline_ = false;

uint16_t onMin_ = 5;
uint16_t offMin_ = 5;
Phase phase_ = Phase::Idle;
unsigned long phaseStartMs_ = 0;
bool built_ = false;

void onStart(lv_event_t *);
void onStop(lv_event_t *);
void onOnMinus(lv_event_t *);
void onOnPlus(lv_event_t *);
void onOffMinus(lv_event_t *);
void onOffPlus(lv_event_t *);

void refreshDraftLabels();
void refreshUiState();
void updatePhaseDisplay();

void syncSlot() {
    const char *mac = NavShell::currentAtlasMac();
    slotRelay_ = NavShell::currentAtlasRelay();
    slotOnline_ = false;
    if (mac && mac[0]) {
        strncpy(slotMac_, mac, sizeof(slotMac_) - 1);
        slotMac_[sizeof(slotMac_) - 1] = '\0';
    } else {
        strncpy(slotMac_, RelayAliasConfig::kPlaceholderMac, sizeof(slotMac_) - 1);
    }
    const size_t nT = SlaveInventory::count();
    for (size_t i = 0; i < nT; ++i) {
        const SlaveInventory::Target *t = SlaveInventory::at(i);
        if (!SlaveInventory::isEspNow(t) || strcmp(t->mac, slotMac_) != 0) {
            continue;
        }
        slotOnline_ = t->online;
        if (slotRelay_ < SlaveInventory::kMaxRelays) {
            (void)t->relayOn[slotRelay_];
        }
        return;
    }
}

bool canSend() {
    return slotOnline_ && slotMac_[0] && strcmp(slotMac_, RelayAliasConfig::kPlaceholderMac) != 0 &&
           strcmp(slotMac_, "local") != 0;
}

void sendOn(int durationSec) {
    if (!canSend()) {
        return;
    }
    MasterLink::sendRelaySlave(slotMac_, slotRelay_, "on", durationSec);
}

void sendOff() {
    if (!canSend()) {
        return;
    }
    MasterLink::sendRelaySlave(slotMac_, slotRelay_, "off", 0);
}

void stopRun(bool userStop) {
    if (phase_ == Phase::Idle) {
        return;
    }
    if (userStop) {
        sendOff();
    }
    phase_ = Phase::Idle;
    RelayActuationLock::set(slotMac_, slotRelay_, RelayActuationLock::Owner::Idle);
    if (statusLbl_) {
        lv_label_set_text(statusLbl_,
                          userStop ? Strings::tr(Msg::StopSent) : Strings::tr(Msg::RelaysTimerDone));
        lv_obj_set_style_text_color(statusLbl_, AppTheme::text(), 0);
    }
    refreshUiState();
}

void updatePhaseDisplay() {
    if (!phaseLbl_) {
        return;
    }
    if (phase_ == Phase::Idle) {
        lv_label_set_text(phaseLbl_, "--:--");
        lv_obj_set_style_text_color(phaseLbl_, AppTheme::muted(), 0);
        return;
    }
    const unsigned long elapsedMs = millis() - phaseStartMs_;
    const uint32_t elapsedSec = static_cast<uint32_t>(elapsedMs / 1000UL);
    const uint32_t totalSec =
        (phase_ == Phase::On) ? static_cast<uint32_t>(onMin_) * 60U : static_cast<uint32_t>(offMin_) * 60U;
    uint32_t rem = (elapsedSec >= totalSec) ? 0U : totalSec - elapsedSec;
    const uint32_t mm = rem / 60U;
    const uint32_t ss = rem % 60U;
    char buf[16];
    if (phase_ == Phase::On) {
        snprintf(buf, sizeof(buf), Strings::tr(Msg::RelaysTimerPhaseOnFmt), mm, ss);
        lv_obj_set_style_text_color(phaseLbl_, AppTheme::accent(), 0);
    } else {
        snprintf(buf, sizeof(buf), Strings::tr(Msg::RelaysTimerPhaseOffFmt), mm, ss);
        lv_obj_set_style_text_color(phaseLbl_, AppTheme::muted(), 0);
    }
    lv_label_set_text(phaseLbl_, buf);
}

void refreshDraftLabels() {
    char buf[16];
    if (onValLbl_) {
        snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(onMin_));
        lv_label_set_text(onValLbl_, buf);
    }
    if (offValLbl_) {
        snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(offMin_));
        lv_label_set_text(offValLbl_, buf);
    }
    updatePhaseDisplay();
}

void refreshUiState() {
    const bool running = phase_ != Phase::Idle;
    const bool timerOther =
        RelayActuationLock::get(slotMac_, slotRelay_) == RelayActuationLock::Owner::Timer && !running;
    const bool cycleLock = RelayActuationLock::timerActive(slotMac_, slotRelay_);

    if (lockLbl_) {
        if (cycleLock && !running) {
            lv_label_set_text(lockLbl_, Strings::tr(Msg::RelaysLockTimerActive));
            lv_obj_clear_flag(lockLbl_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(lockLbl_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    const lv_opa_t editOpa = running ? LV_OPA_40 : LV_OPA_COVER;
    if (startBtn_) {
        lv_obj_set_style_opa(startBtn_, (running || !canSend()) ? LV_OPA_40 : LV_OPA_COVER, 0);
        if (running || !canSend()) {
            lv_obj_clear_flag(startBtn_, LV_OBJ_FLAG_CLICKABLE);
        } else {
            lv_obj_add_flag(startBtn_, LV_OBJ_FLAG_CLICKABLE);
        }
    }
    if (stopBtn_) {
        lv_obj_set_style_opa(stopBtn_, running ? LV_OPA_COVER : LV_OPA_40, 0);
        if (running) {
            lv_obj_add_flag(stopBtn_, LV_OBJ_FLAG_CLICKABLE);
        } else {
            lv_obj_clear_flag(stopBtn_, LV_OBJ_FLAG_CLICKABLE);
        }
    }
    (void)timerOther;
    (void)editOpa;
}

lv_obj_t *makePmRow(lv_obj_t *parent, lv_coord_t y, const char *title, lv_obj_t **valOut,
                    lv_event_cb_t onM, lv_event_cb_t onP) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, kRowW, kBtnH);
    lv_obj_set_pos(row, 12, y);
    UiKit::forceOpaqueBg(row, AppTheme::surface());
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, AppTheme::gridLine(), 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *t = lv_label_create(row);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_color(t, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t *v = lv_label_create(row);
    lv_obj_set_style_text_color(v, AppTheme::text(), 0);
    lv_obj_set_style_text_font(v, &lv_font_montserrat_20, 0);
    lv_obj_align(v, LV_ALIGN_CENTER, 0, 0);
    if (valOut) {
        *valOut = v;
    }

    lv_obj_t *m = UiKit::makeSecondaryButton(row, "-", 52, kBtnH, onM);
    lv_obj_align(m, LV_ALIGN_RIGHT_MID, -(8 + 52 + 4), 0);
    lv_obj_t *p = UiKit::makeSecondaryButton(row, "+", 52, kBtnH, onP);
    lv_obj_align(p, LV_ALIGN_RIGHT_MID, -8, 0);
    return row;
}

void buildDetail() {
    if (!listHost_) {
        return;
    }
    lv_obj_clean(listHost_);
    phaseLbl_ = onValLbl_ = offValLbl_ = statusLbl_ = startBtn_ = stopBtn_ = lockLbl_ = nullptr;
    built_ = false;

    lv_coord_t y = 2;
    lv_obj_t *hint = lv_label_create(listHost_);
    lv_label_set_text(hint, Strings::tr(Msg::RelaysTimerHint));
    lv_obj_set_style_text_color(hint, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_width(hint, kRowW);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(hint, 12, y);
    lv_obj_update_layout(hint);
    y += lv_obj_get_height(hint) + 6;

    lockLbl_ = lv_label_create(listHost_);
    lv_obj_set_style_text_color(lockLbl_, AppTheme::warn(), 0);
    lv_obj_set_style_text_font(lockLbl_, &lv_font_montserrat_14, 0);
    lv_obj_set_width(lockLbl_, kRowW);
    lv_obj_set_pos(lockLbl_, 12, y);
    lv_obj_add_flag(lockLbl_, LV_OBJ_FLAG_HIDDEN);
    y += 18;

    phaseLbl_ = lv_label_create(listHost_);
    lv_obj_set_style_text_font(phaseLbl_, &lv_font_montserrat_28, 0);
    lv_obj_set_width(phaseLbl_, LCD_H_RES);
    lv_obj_set_style_text_align(phaseLbl_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(phaseLbl_, 0, y);
    y += 34;

    makePmRow(listHost_, y, Strings::tr(Msg::RelaysTimerMin), &onValLbl_, onOnMinus, onOnPlus);
    y += kBtnH + 4;
    makePmRow(listHost_, y, Strings::tr(Msg::RelaysTimerOffMin), &offValLbl_, onOffMinus, onOffPlus);
    y += kBtnH + 8;

    const lv_coord_t halfW = (LCD_H_RES - 36) / 2;
    startBtn_ = UiKit::makePrimaryButton(listHost_, Strings::tr(Msg::RelaysTimerStart), onStart);
    lv_obj_set_size(startBtn_, halfW, kBtnH);
    lv_obj_set_pos(startBtn_, 12, y);
    stopBtn_ = UiKit::makeCautionButton(listHost_, Strings::tr(Msg::StopBtn), onStop);
    lv_obj_set_size(stopBtn_, halfW, kBtnH);
    lv_obj_set_pos(stopBtn_, 12 + halfW + 12, y);
    y += kBtnH + 6;

    statusLbl_ = lv_label_create(listHost_);
    lv_obj_set_width(statusLbl_, kRowW);
    lv_label_set_long_mode(statusLbl_, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(statusLbl_, 12, y);
    lv_label_set_text(statusLbl_, Strings::tr(Msg::ReadyStatus));
    lv_obj_set_style_text_color(statusLbl_, AppTheme::muted(), 0);

    built_ = true;
    refreshDraftLabels();
    refreshUiState();
}

void onBack(lv_event_t *) {
    stopRun(true);
    NavShell::back();
}

void onOnMinus(lv_event_t *) {
    if (phase_ != Phase::Idle || onMin_ <= 1) {
        return;
    }
    --onMin_;
    refreshDraftLabels();
}

void onOnPlus(lv_event_t *) {
    if (phase_ != Phase::Idle || onMin_ >= 240) {
        return;
    }
    ++onMin_;
    refreshDraftLabels();
}

void onOffMinus(lv_event_t *) {
    if (phase_ != Phase::Idle || offMin_ <= 1) {
        return;
    }
    --offMin_;
    refreshDraftLabels();
}

void onOffPlus(lv_event_t *) {
    if (phase_ != Phase::Idle || offMin_ >= 240) {
        return;
    }
    ++offMin_;
    refreshDraftLabels();
}

void onStart(lv_event_t *) {
    if (phase_ != Phase::Idle || !canSend()) {
        if (statusLbl_ && !canSend()) {
            lv_label_set_text(statusLbl_, Strings::tr(Msg::RelaysOffline));
            lv_obj_set_style_text_color(statusLbl_, AppTheme::warn(), 0);
        }
        return;
    }
    RelayActuationLock::set(slotMac_, slotRelay_, RelayActuationLock::Owner::Timer);
    const int sec = static_cast<int>(onMin_ * 60U);
    sendOn(sec);
    phase_ = Phase::On;
    phaseStartMs_ = millis();
    if (statusLbl_) {
        lv_label_set_text(statusLbl_, Strings::tr(Msg::RelaysTimerStart));
        lv_obj_set_style_text_color(statusLbl_, AppTheme::accent(), 0);
    }
    refreshUiState();
    updatePhaseDisplay();
}

void onStop(lv_event_t *) { stopRun(true); }

void tickPhase() {
    if (phase_ == Phase::Idle) {
        return;
    }
    const unsigned long elapsedMs = millis() - phaseStartMs_;
    const uint32_t elapsedSec = static_cast<uint32_t>(elapsedMs / 1000UL);
    if (phase_ == Phase::On) {
        const uint32_t onSec = static_cast<uint32_t>(onMin_) * 60U;
        if (elapsedSec >= onSec) {
            phase_ = Phase::Off;
            phaseStartMs_ = millis();
            sendOff();
        }
    } else if (phase_ == Phase::Off) {
        const uint32_t offSec = static_cast<uint32_t>(offMin_) * 60U;
        if (elapsedSec >= offSec) {
            phase_ = Phase::On;
            phaseStartMs_ = millis();
            sendOn(static_cast<int>(onMin_ * 60U));
        }
    }
    updatePhaseDisplay();
}

}  // namespace

lv_obj_t *Screens::createRelayTimer(lv_obj_t *parent) {
    listHost_ = nullptr;
    phase_ = Phase::Idle;
    onMin_ = 5;
    offMin_ = 5;
    built_ = false;

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root, Strings::tr(Msg::RelaysTimerTitle), onBack);

    listHost_ = lv_obj_create(root);
    lv_obj_remove_style_all(listHost_);
    lv_obj_set_size(listHost_, LCD_H_RES, LCD_V_RES - kListTopY);
    lv_obj_align(listHost_, LV_ALIGN_TOP_LEFT, 0, kListTopY);
    lv_obj_set_style_bg_opa(listHost_, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(listHost_, LV_OBJ_FLAG_SCROLLABLE);

    syncSlot();
    MasterLink::requestSlaves();
    buildDetail();
    return root;
}

void Screens::refreshRelayTimer(lv_obj_t *root) {
    (void)root;
    syncSlot();
    if (!built_) {
        return;
    }
    tickPhase();
    refreshUiState();
}
