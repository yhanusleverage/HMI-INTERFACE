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

/** ON/OFF inmediato de un relé Atlas (contexto NavShell). */

namespace {

lv_obj_t *onBtn_ = nullptr;
lv_obj_t *offBtn_ = nullptr;
lv_obj_t *statusLbl_ = nullptr;
lv_obj_t *lockLbl_ = nullptr;
bool relayOn_ = false;
bool online_ = false;

bool isPlaceholderMac(const char *mac) {
    return mac && strcmp(mac, RelayAliasConfig::kPlaceholderMac) == 0;
}

bool canSend() {
    const char *mac = NavShell::currentAtlasMac();
    if (!mac || !mac[0] || isPlaceholderMac(mac)) {
        return false;
    }
    if (strcmp(mac, "local") == 0) {
        return !SlaveInventory::isRelayLocked(mac, NavShell::currentAtlasRelay());
    }
    return online_ && !SlaveInventory::isRelayLocked(mac, NavShell::currentAtlasRelay());
}

void refreshLockBanner() {
    if (!lockLbl_) {
        return;
    }
    const char *mac = NavShell::currentAtlasMac();
    const uint8_t r = NavShell::currentAtlasRelay();
    bool show = false;
    const char *txt = nullptr;
    static char ruleBuf[48];
    if (mac && SlaveInventory::isRelayLocked(mac, r)) {
        const char *lab = SlaveInventory::relayLockLabel(mac, r);
        if (lab && lab[0]) {
            snprintf(ruleBuf, sizeof(ruleBuf), "%s: %s", Strings::tr(Msg::RelaysLockByRule), lab);
            txt = ruleBuf;
        } else {
            txt = Strings::tr(Msg::RelaysLockByRule);
        }
        show = true;
    } else if (mac && RelayActuationLock::timerActive(mac, r)) {
        txt = Strings::tr(Msg::RelaysLockTimerActive);
        show = true;
    } else if (mac) {
        const RelayCycleConfig::Entry *ce = RelayCycleConfig::get(mac, r);
        if (ce && ce->enabled) {
            txt = Strings::tr(Msg::RelaysLockCycleActive);
            show = true;
        }
    }
    if (show && txt) {
        lv_label_set_text(lockLbl_, txt);
        lv_obj_clear_flag(lockLbl_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(lockLbl_, LV_OBJ_FLAG_HIDDEN);
    }
}

void paintToggle() {
    if (!onBtn_ || !offBtn_) {
        return;
    }
    const bool on = relayOn_;
    lv_obj_set_style_bg_color(onBtn_, on ? AppTheme::accent() : AppTheme::surfaceAlt(), 0);
    lv_obj_set_style_bg_opa(onBtn_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(offBtn_, !on ? AppTheme::alarm() : AppTheme::surfaceAlt(), 0);
    lv_obj_set_style_bg_opa(offBtn_, LV_OPA_COVER, 0);
    lv_obj_t *onLbl = lv_obj_get_child(onBtn_, 0);
    lv_obj_t *offLbl = lv_obj_get_child(offBtn_, 0);
    if (onLbl) {
        lv_obj_set_style_text_color(onLbl, on ? AppTheme::bg() : AppTheme::text(), 0);
    }
    if (offLbl) {
        lv_obj_set_style_text_color(offLbl, !on ? AppTheme::text() : AppTheme::muted(), 0);
    }
    const lv_opa_t opa = canSend() ? LV_OPA_COVER : LV_OPA_50;
    lv_obj_set_style_opa(onBtn_, opa, 0);
    lv_obj_set_style_opa(offBtn_, opa, 0);
    if (canSend()) {
        lv_obj_add_flag(onBtn_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(offBtn_, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_clear_flag(onBtn_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(offBtn_, LV_OBJ_FLAG_CLICKABLE);
    }
    refreshLockBanner();
}

void syncFromInventory() {
    online_ = false;
    relayOn_ = false;
    const char *mac = NavShell::currentAtlasMac();
    const uint8_t r = NavShell::currentAtlasRelay();
    if (!mac || !mac[0] || isPlaceholderMac(mac)) {
        return;
    }
    if (strcmp(mac, "local") == 0) {
        online_ = true;
        const size_t ix = SlaveInventory::localIndex();
        const SlaveInventory::Target *t = (ix != SIZE_MAX) ? SlaveInventory::at(ix) : nullptr;
        if (t && r < SlaveInventory::kMaxRelays) {
            relayOn_ = t->relayOn[r];
        }
        return;
    }
    const size_t nT = SlaveInventory::count();
    for (size_t i = 0; i < nT; ++i) {
        const SlaveInventory::Target *t = SlaveInventory::at(i);
        if (!SlaveInventory::isEspNow(t) || strcmp(t->mac, mac) != 0) {
            continue;
        }
        online_ = t->online;
        if (r < SlaveInventory::kMaxRelays) {
            relayOn_ = t->relayOn[r];
        }
        return;
    }
}

void sendState(bool on) {
    if (!canSend()) {
        paintToggle();
        return;
    }
    const char *mac = NavShell::currentAtlasMac();
    const uint8_t r = NavShell::currentAtlasRelay();
    if (SlaveInventory::isRelayLocked(mac, r)) {
        if (statusLbl_) {
            lv_label_set_text(statusLbl_, Strings::tr(Msg::RelaysLockByRule));
            lv_obj_set_style_text_color(statusLbl_, AppTheme::warn(), 0);
        }
        return;
    }
    if (strcmp(mac, "local") == 0) {
        relayOn_ = on;
        paintToggle();
        MasterLink::sendRelayLocal(r, on ? "on" : "off", 0);
        if (statusLbl_) {
            lv_label_set_text(statusLbl_, Strings::tr(Msg::RelaysOnline));
            lv_obj_set_style_text_color(statusLbl_, AppTheme::text(), 0);
        }
        return;
    }
    if (RelayActuationLock::timerActive(mac, r)) {
        if (statusLbl_) {
            lv_label_set_text(statusLbl_, Strings::tr(Msg::RelaysLockTimerActive));
            lv_obj_set_style_text_color(statusLbl_, AppTheme::warn(), 0);
        }
        return;
    }
    RelayActuationLock::set(mac, r, RelayActuationLock::Owner::ManualPulse);
    relayOn_ = on;
    paintToggle();
    MasterLink::sendRelaySlave(mac, r, on ? "on" : "off", 0);
    if (statusLbl_) {
        const RelayCycleConfig::Entry *ce = RelayCycleConfig::get(mac, r);
        if (ce && ce->enabled) {
            lv_label_set_text(statusLbl_, Strings::tr(Msg::RelaysLockCycleActive));
            lv_obj_set_style_text_color(statusLbl_, AppTheme::warn(), 0);
        } else {
            lv_label_set_text(statusLbl_, Strings::tr(online_ ? Msg::RelaysOnline : Msg::RelaysOffline));
            lv_obj_set_style_text_color(statusLbl_, online_ ? AppTheme::text() : AppTheme::muted(), 0);
        }
    }
}

void onBack(lv_event_t *) { NavShell::back(); }
void onOn(lv_event_t *) { sendState(true); }
void onOff(lv_event_t *) { sendState(false); }

}  // namespace

lv_obj_t *Screens::createRelayOnOff(lv_obj_t *parent) {
    onBtn_ = nullptr;
    offBtn_ = nullptr;
    statusLbl_ = nullptr;
    lockLbl_ = nullptr;

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root, Strings::tr(Msg::RelaysOnOffTitle), onBack);

    lv_coord_t y = AppTheme::MENU_LIST_TOP + 2;

    lv_obj_t *hint = lv_label_create(root);
    lv_label_set_text(hint, Strings::tr(Msg::RelaysOnOffHint));
    lv_obj_set_style_text_color(hint, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_width(hint, LCD_H_RES - 24);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(hint, 12, y);
    lv_obj_update_layout(hint);
    y += lv_obj_get_height(hint) + 8;

    lockLbl_ = lv_label_create(root);
    lv_obj_set_style_text_color(lockLbl_, AppTheme::warn(), 0);
    lv_obj_set_style_text_font(lockLbl_, &lv_font_montserrat_14, 0);
    lv_obj_set_width(lockLbl_, LCD_H_RES - 24);
    lv_obj_set_pos(lockLbl_, 12, y);
    lv_obj_add_flag(lockLbl_, LV_OBJ_FLAG_HIDDEN);
    y += 20;

    const lv_coord_t btnW = (LCD_H_RES - 36) / 2;
    onBtn_ = UiKit::makeSecondaryButton(root, Strings::tr(Msg::OnLabel), btnW, AppTheme::BTN_PRIMARY_H,
                                        onOn);
    lv_obj_set_pos(onBtn_, 12, y);
    offBtn_ = UiKit::makeSecondaryButton(root, Strings::tr(Msg::OffLabel), btnW,
                                         AppTheme::BTN_PRIMARY_H, onOff);
    lv_obj_set_pos(offBtn_, 12 + btnW + 12, y);
    y += AppTheme::BTN_PRIMARY_H + 12;

    statusLbl_ = lv_label_create(root);
    lv_obj_set_pos(statusLbl_, 12, y);

    syncFromInventory();
    paintToggle();
    if (statusLbl_) {
        lv_label_set_text(statusLbl_,
                          Strings::tr(online_ ? Msg::RelaysOnline : Msg::RelaysOffline));
        lv_obj_set_style_text_color(statusLbl_, online_ ? AppTheme::text() : AppTheme::muted(), 0);
    }

    MasterLink::requestSlaves();
    return root;
}

void Screens::refreshRelayOnOff(lv_obj_t *root) {
    (void)root;
    syncFromInventory();
    paintToggle();
    if (statusLbl_) {
        lv_label_set_text(statusLbl_,
                          Strings::tr(online_ ? Msg::RelaysOnline : Msg::RelaysOffline));
        lv_obj_set_style_text_color(statusLbl_, online_ ? AppTheme::text() : AppTheme::muted(), 0);
    }
}

lv_obj_t *Screens::createRelayNames(lv_obj_t *parent) { return Screens::createRelayOnOff(parent); }

void Screens::refreshRelayNames(lv_obj_t *root) { Screens::refreshRelayOnOff(root); }
