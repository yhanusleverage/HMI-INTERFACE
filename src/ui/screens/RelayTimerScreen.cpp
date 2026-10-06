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

/** Timer Atlas: la cuenta atrás es local. El esclavo ejecuta el cycle hasta Stop. */

namespace {

constexpr lv_coord_t kListTopY = AppTheme::MENU_LIST_TOP;
constexpr lv_coord_t kRowW = LCD_H_RES - 24;
constexpr lv_coord_t kBtnH = AppTheme::TOUCH_MIN_H;

enum class Phase : uint8_t { Idle, On, Off };

lv_obj_t *listHost_ = nullptr;
lv_obj_t *lockLbl_ = nullptr;
lv_obj_t *phaseLbl_ = nullptr;
lv_obj_t *onHBtn_ = nullptr;
lv_obj_t *onMBtn_ = nullptr;
lv_obj_t *offHBtn_ = nullptr;
lv_obj_t *offMBtn_ = nullptr;
lv_obj_t *statusLbl_ = nullptr;
lv_obj_t *startBtn_ = nullptr;
lv_obj_t *stopBtn_ = nullptr;

char slotMac_[SlaveInventory::kMacLen] = {};
uint8_t slotRelay_ = 0;
bool slotOnline_ = false;

uint8_t onH_ = 0;
uint8_t onM_ = 5;
uint8_t offH_ = 0;
uint8_t offM_ = 5;
bool onEditMin_ = true;
bool offEditMin_ = true;
Phase phase_ = Phase::Idle;
unsigned long phaseStartMs_ = 0;
bool built_ = false;

void onStart(lv_event_t *);
void onStop(lv_event_t *);
void onOnH(lv_event_t *);
void onOnM(lv_event_t *);
void onOffH(lv_event_t *);
void onOffM(lv_event_t *);
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

int phaseSec(uint8_t hours, uint8_t mins) {
    const int sec = static_cast<int>(hours) * 60 + static_cast<int>(mins);
    return (sec > 0 ? sec : 1) * 60;
}

void sendCycle(bool start) {
    if (!canSend()) {
        return;
    }
    if (!start) {
        MasterLink::sendRelaySlave(slotMac_, slotRelay_, "cycle_stop", 0, 0, "cycle_stop");
        return;
    }
    MasterLink::sendRelaySlave(slotMac_, slotRelay_, "cycle", phaseSec(onH_, onM_), phaseSec(offH_, offM_),
                               "cycle");
}

void stopRun(bool userStop) {
    if (phase_ == Phase::Idle) {
        return;
    }
    if (userStop) {
        sendCycle(false);
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
    const uint32_t totalSec = (phase_ == Phase::On) ? (static_cast<uint32_t>(onH_) * 60U + onM_) * 60U
                                                    : (static_cast<uint32_t>(offH_) * 60U + offM_) * 60U;
    uint32_t rem = (elapsedSec >= totalSec) ? 0U : totalSec - elapsedSec;
    const uint32_t hh = rem / 3600U;
    const uint32_t mm = (rem % 3600U) / 60U;
    char buf[24];
    if (phase_ == Phase::On) {
        snprintf(buf, sizeof(buf), "ON %02u:%02u", static_cast<unsigned>(hh), static_cast<unsigned>(mm));
        lv_obj_set_style_text_color(phaseLbl_, AppTheme::accent(), 0);
    } else {
        snprintf(buf, sizeof(buf), "OFF %02u:%02u", static_cast<unsigned>(hh), static_cast<unsigned>(mm));
        lv_obj_set_style_text_color(phaseLbl_, AppTheme::muted(), 0);
    }
    lv_label_set_text(phaseLbl_, buf);
}

void paintHm(lv_obj_t *btn, unsigned value, bool selected) {
    if (!btn) {
        return;
    }
    char buf[4];
    snprintf(buf, sizeof(buf), "%02u", value);
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    if (lbl) {
        lv_label_set_text(lbl, buf);
        lv_obj_set_style_text_color(lbl, selected ? AppTheme::bg() : AppTheme::text(), 0);
    }
    lv_obj_set_style_bg_color(btn, selected ? AppTheme::accent() : AppTheme::surfaceAlt(), 0);
}

void refreshDraftLabels() {
    paintHm(onHBtn_, onH_, !onEditMin_);
    paintHm(onMBtn_, onM_, onEditMin_);
    paintHm(offHBtn_, offH_, !offEditMin_);
    paintHm(offMBtn_, offM_, offEditMin_);
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

lv_obj_t *makeHmRow(lv_obj_t *parent, lv_coord_t y, const char *title, lv_obj_t **hBtn, lv_obj_t **mBtn,
                    lv_event_cb_t onH, lv_event_cb_t onM, lv_event_cb_t onMinus, lv_event_cb_t onPlus) {
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

    lv_obj_t *hb = UiKit::makeSecondaryButton(row, "00", 56, kBtnH, onH);
    lv_obj_align(hb, LV_ALIGN_LEFT_MID, 78, 0);
    lv_obj_t *colon = lv_label_create(row);
    lv_label_set_text(colon, ":");
    lv_obj_set_style_text_color(colon, AppTheme::text(), 0);
    lv_obj_set_style_text_font(colon, &lv_font_montserrat_20, 0);
    lv_obj_align(colon, LV_ALIGN_LEFT_MID, 138, 0);
    lv_obj_t *mb = UiKit::makeSecondaryButton(row, "00", 56, kBtnH, onM);
    lv_obj_align(mb, LV_ALIGN_LEFT_MID, 150, 0);
    if (hBtn) {
        *hBtn = hb;
    }
    if (mBtn) {
        *mBtn = mb;
    }

    lv_obj_t *minus = UiKit::makeSecondaryButton(row, "-", 52, kBtnH, onMinus);
    lv_obj_align(minus, LV_ALIGN_RIGHT_MID, -(8 + 52 + 4), 0);
    lv_obj_t *plus = UiKit::makeSecondaryButton(row, "+", 52, kBtnH, onPlus);
    lv_obj_align(plus, LV_ALIGN_RIGHT_MID, -8, 0);
    return row;
}

void buildDetail() {
    if (!listHost_) {
        return;
    }
    lv_obj_clean(listHost_);
    phaseLbl_ = statusLbl_ = startBtn_ = stopBtn_ = lockLbl_ = nullptr;
    onHBtn_ = onMBtn_ = offHBtn_ = offMBtn_ = nullptr;
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

    makeHmRow(listHost_, y, Strings::tr(Msg::OnLabel), &onHBtn_, &onMBtn_, onOnH, onOnM, onOnMinus,
              onOnPlus);
    y += kBtnH + 4;
    makeHmRow(listHost_, y, Strings::tr(Msg::OffLabel), &offHBtn_, &offMBtn_, onOffH, onOffM, onOffMinus,
              onOffPlus);
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

void stepHm(uint8_t &h, uint8_t &m, bool editMin, int dir) {
    if (editMin) {
        int v = static_cast<int>(m) + dir;
        if (v < 0) {
            v = 0;
        }
        if (v > 59) {
            v = 59;
        }
        m = static_cast<uint8_t>(v);
    } else {
        int v = static_cast<int>(h) + dir;
        if (v < 0) {
            v = 0;
        }
        if (v > 23) {
            v = 23;
        }
        h = static_cast<uint8_t>(v);
    }
    if (h == 0 && m == 0) {
        m = 1;
    }
}

void onOnH(lv_event_t *) {
    if (phase_ != Phase::Idle) {
        return;
    }
    onEditMin_ = false;
    refreshDraftLabels();
}
void onOnM(lv_event_t *) {
    if (phase_ != Phase::Idle) {
        return;
    }
    onEditMin_ = true;
    refreshDraftLabels();
}
void onOffH(lv_event_t *) {
    if (phase_ != Phase::Idle) {
        return;
    }
    offEditMin_ = false;
    refreshDraftLabels();
}
void onOffM(lv_event_t *) {
    if (phase_ != Phase::Idle) {
        return;
    }
    offEditMin_ = true;
    refreshDraftLabels();
}

void onOnMinus(lv_event_t *) {
    if (phase_ != Phase::Idle) {
        return;
    }
    stepHm(onH_, onM_, onEditMin_, -1);
    refreshDraftLabels();
}

void onOnPlus(lv_event_t *) {
    if (phase_ != Phase::Idle) {
        return;
    }
    stepHm(onH_, onM_, onEditMin_, 1);
    refreshDraftLabels();
}

void onOffMinus(lv_event_t *) {
    if (phase_ != Phase::Idle) {
        return;
    }
    stepHm(offH_, offM_, offEditMin_, -1);
    refreshDraftLabels();
}

void onOffPlus(lv_event_t *) {
    if (phase_ != Phase::Idle) {
        return;
    }
    stepHm(offH_, offM_, offEditMin_, 1);
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
    sendCycle(true);
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
        const uint32_t onSec = (static_cast<uint32_t>(onH_) * 60U + onM_) * 60U;
        if (elapsedSec >= onSec) {
            phase_ = Phase::Off;
            phaseStartMs_ = millis();
        }
    } else if (phase_ == Phase::Off) {
        const uint32_t offSec = (static_cast<uint32_t>(offH_) * 60U + offM_) * 60U;
        if (elapsedSec >= offSec) {
            phase_ = Phase::On;
            phaseStartMs_ = millis();
        }
    }
    updatePhaseDisplay();
}

}  // namespace

lv_obj_t *Screens::createRelayTimer(lv_obj_t *parent) {
    listHost_ = nullptr;
    phase_ = Phase::Idle;
    onH_ = 0;
    onM_ = 5;
    offH_ = 0;
    offM_ = 5;
    onEditMin_ = true;
    offEditMin_ = true;
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
