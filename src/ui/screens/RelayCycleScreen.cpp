#include "Screens.h"
#include "NavShell.h"
#include "RelayAliasConfig.h"
#include "RelayCycleConfig.h"
#include "RelayActuationLock.h"
#include "SlaveInventory.h"
#include "MasterLink.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

#include <cstdio>
#include <cstring>

/** Ciclo Atlas: ON/OFF horas, 24h auto. Layout compacto 480×320. */

namespace {

constexpr lv_coord_t kListTopY = AppTheme::MENU_LIST_TOP;
constexpr lv_coord_t kRowW = LCD_H_RES - 24;
constexpr lv_coord_t kBtnH = AppTheme::TOUCH_MIN_H;
constexpr lv_coord_t kSegW = 72;
constexpr lv_coord_t kPad = 4;
constexpr lv_coord_t kGap = 4;
constexpr lv_coord_t kRowH = kBtnH;

lv_obj_t *listHost_ = nullptr;
lv_obj_t *lockLbl_ = nullptr;
lv_obj_t *onValLbl_ = nullptr;
lv_obj_t *offValLbl_ = nullptr;
lv_obj_t *enOnBtn_ = nullptr;
lv_obj_t *enOffBtn_ = nullptr;
lv_obj_t *saveBtn_ = nullptr;

size_t detailIx_ = SIZE_MAX;
size_t slotCount_ = 0;
char slotMac_[SlaveInventory::kMacLen] = {};
uint8_t slotRelay_ = 0;
bool draftEn_ = false;
uint8_t draftOnH_ = 12;
uint8_t draftOffH_ = 12;
bool built_ = false;

void onEnOn(lv_event_t *);
void onEnOff(lv_event_t *);
void onOnMinus(lv_event_t *);
void onOnPlus(lv_event_t *);
void onOffMinus(lv_event_t *);
void onOffPlus(lv_event_t *);
void onSave(lv_event_t *);

size_t countEspNow() {
    size_t n = 0;
    const size_t nT = SlaveInventory::count();
    for (size_t i = 0; i < nT; ++i) {
        if (SlaveInventory::isEspNow(SlaveInventory::at(i))) {
            ++n;
        }
    }
    return n;
}

void syncSlotFromNav() {
    const char *mac = NavShell::currentAtlasMac();
    slotRelay_ = NavShell::currentAtlasRelay();
    if (mac && mac[0]) {
        strncpy(slotMac_, mac, sizeof(slotMac_) - 1);
        slotMac_[sizeof(slotMac_) - 1] = '\0';
    } else if (countEspNow() == 0) {
        strncpy(slotMac_, RelayAliasConfig::kPlaceholderMac, sizeof(slotMac_) - 1);
    }
    slotCount_ = 1;
    detailIx_ = 0;
}

void loadDraft() {
    draftEn_ = false;
    draftOnH_ = 12;
    draftOffH_ = 12;
    const RelayCycleConfig::Entry *e = RelayCycleConfig::get(slotMac_, slotRelay_);
    if (!e) {
        return;
    }
    draftEn_ = e->enabled;
    draftOnH_ = e->onHours;
    draftOffH_ = e->offHours;
}

void paintOnOffPair(lv_obj_t *onBtn, lv_obj_t *offBtn, bool on) {
    if (!onBtn || !offBtn) {
        return;
    }
    lv_obj_set_style_bg_color(onBtn, on ? AppTheme::accent() : AppTheme::surfaceAlt(), 0);
    lv_obj_set_style_bg_color(offBtn, !on ? AppTheme::alarm() : AppTheme::surfaceAlt(), 0);
    lv_obj_t *onLbl = lv_obj_get_child(onBtn, 0);
    lv_obj_t *offLbl = lv_obj_get_child(offBtn, 0);
    if (onLbl) {
        lv_obj_set_style_text_color(onLbl, on ? AppTheme::bg() : AppTheme::text(), 0);
    }
    if (offLbl) {
        lv_obj_set_style_text_color(offLbl, !on ? AppTheme::text() : AppTheme::muted(), 0);
    }
}

lv_obj_t *makeSeg(lv_obj_t *parent, const char *txt, lv_event_cb_t cb) {
    return UiKit::makeSecondaryButton(parent, txt, kSegW, kBtnH, cb);
}

void refreshValues() {
    char buf[12];
    if (onValLbl_) {
        snprintf(buf, sizeof(buf), "%u h", static_cast<unsigned>(draftOnH_));
        lv_label_set_text(onValLbl_, buf);
    }
    if (offValLbl_) {
        snprintf(buf, sizeof(buf), "%u h", static_cast<unsigned>(draftOffH_));
        lv_label_set_text(offValLbl_, buf);
    }
    paintOnOffPair(enOnBtn_, enOffBtn_, draftEn_);

    const bool timerLock = RelayActuationLock::timerActive(slotMac_, slotRelay_);
    if (lockLbl_) {
        if (timerLock) {
            lv_label_set_text(lockLbl_, Strings::tr(Msg::RelaysLockTimerActive));
            lv_obj_clear_flag(lockLbl_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(lockLbl_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (saveBtn_) {
        const lv_opa_t opa = timerLock ? LV_OPA_40 : LV_OPA_COVER;
        lv_obj_set_style_opa(saveBtn_, opa, 0);
        if (timerLock) {
            lv_obj_clear_flag(saveBtn_, LV_OBJ_FLAG_CLICKABLE);
        } else {
            lv_obj_add_flag(saveBtn_, LV_OBJ_FLAG_CLICKABLE);
        }
    }
}

lv_obj_t *makePmRow(lv_obj_t *parent, lv_coord_t y, const char *title, lv_obj_t **valOut,
                    lv_event_cb_t onM, lv_event_cb_t onP) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, kRowW, kRowH);
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
    lockLbl_ = onValLbl_ = offValLbl_ = enOnBtn_ = enOffBtn_ = saveBtn_ = nullptr;
    built_ = false;

    lv_coord_t y = 2;
    lv_obj_t *hint = lv_label_create(listHost_);
    lv_label_set_text(hint, Strings::tr(Msg::RelaysCycleHint));
    lv_obj_set_style_text_color(hint, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_width(hint, kRowW);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(hint, 12, y);
    lv_obj_update_layout(hint);
    y += lv_obj_get_height(hint) + 6;

    lockLbl_ = lv_label_create(listHost_);
    lv_label_set_text(lockLbl_, Strings::tr(Msg::RelaysLockTimerActive));
    lv_obj_set_style_text_color(lockLbl_, AppTheme::warn(), 0);
    lv_obj_set_style_text_font(lockLbl_, &lv_font_montserrat_14, 0);
    lv_obj_set_width(lockLbl_, kRowW);
    lv_obj_set_pos(lockLbl_, 12, y);
    lv_obj_add_flag(lockLbl_, LV_OBJ_FLAG_HIDDEN);
    y += 20;

    lv_obj_t *enRow = lv_obj_create(listHost_);
    lv_obj_remove_style_all(enRow);
    lv_obj_set_size(enRow, kRowW, kRowH);
    lv_obj_set_pos(enRow, 12, y);
    UiKit::forceOpaqueBg(enRow, AppTheme::surface());
    lv_obj_set_style_border_width(enRow, 1, 0);
    lv_obj_set_style_border_color(enRow, AppTheme::gridLine(), 0);
    lv_obj_clear_flag(enRow, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *enLbl = lv_label_create(enRow);
    lv_label_set_text(enLbl, Strings::tr(Msg::RelaysCycleEnable));
    lv_obj_set_style_text_color(enLbl, AppTheme::text(), 0);
    lv_obj_set_style_text_font(enLbl, &lv_font_montserrat_14, 0);
    lv_obj_align(enLbl, LV_ALIGN_LEFT_MID, 8, 0);
    enOnBtn_ = makeSeg(enRow, Strings::tr(Msg::OnLabel), onEnOn);
    lv_obj_align(enOnBtn_, LV_ALIGN_RIGHT_MID, -(kPad + kSegW + kGap), 0);
    enOffBtn_ = makeSeg(enRow, Strings::tr(Msg::OffLabel), onEnOff);
    lv_obj_align(enOffBtn_, LV_ALIGN_RIGHT_MID, -kPad, 0);
    y += kRowH + kGap;

    makePmRow(listHost_, y, Strings::tr(Msg::RelaysOnHours), &onValLbl_, onOnMinus, onOnPlus);
    y += kRowH + kGap;
    makePmRow(listHost_, y, Strings::tr(Msg::RelaysOffHours), &offValLbl_, onOffMinus, onOffPlus);
    y += kRowH + 8;

    saveBtn_ = UiKit::makePrimaryButton(listHost_, Strings::tr(Msg::NutSave), onSave);
    lv_obj_set_size(saveBtn_, kRowW, kBtnH);
    lv_obj_set_pos(saveBtn_, 12, y);

    built_ = true;
    refreshValues();
}

void onBack(lv_event_t *) { NavShell::back(); }

void onEnOn(lv_event_t *) {
    draftEn_ = true;
    refreshValues();
}
void onEnOff(lv_event_t *) {
    draftEn_ = false;
    refreshValues();
}

void onOnMinus(lv_event_t *) {
    if (draftOnH_ > 1) {
        --draftOnH_;
        refreshValues();
    }
}
void onOnPlus(lv_event_t *) {
    if (draftOnH_ < 23) {
        ++draftOnH_;
        refreshValues();
    }
}
void onOffMinus(lv_event_t *) {
    if (draftOffH_ > 1) {
        --draftOffH_;
        refreshValues();
    }
}
void onOffPlus(lv_event_t *) {
    if (draftOffH_ < 23) {
        ++draftOffH_;
        refreshValues();
    }
}

void onSave(lv_event_t *) {
    if (RelayActuationLock::timerActive(slotMac_, slotRelay_)) {
        return;
    }
    RelayCycleConfig::setHours(slotMac_, slotRelay_, draftOnH_, draftOffH_);
    RelayCycleConfig::setEnabled(slotMac_, slotRelay_, draftEn_);
    NavShell::back();
}

}  // namespace

lv_obj_t *Screens::createRelayCycle(lv_obj_t *parent) {
    listHost_ = nullptr;
    detailIx_ = SIZE_MAX;
    slotCount_ = 0;
    built_ = false;

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root, Strings::tr(Msg::RelaysCycleTitle), onBack);

    listHost_ = lv_obj_create(root);
    lv_obj_remove_style_all(listHost_);
    lv_obj_set_size(listHost_, LCD_H_RES, LCD_V_RES - kListTopY);
    lv_obj_align(listHost_, LV_ALIGN_TOP_LEFT, 0, kListTopY);
    lv_obj_set_style_bg_opa(listHost_, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(listHost_, LV_OBJ_FLAG_SCROLLABLE);

    syncSlotFromNav();
    loadDraft();
    MasterLink::requestSlaves();
    buildDetail();
    return root;
}

void Screens::refreshRelayCycle(lv_obj_t *root) {
    (void)root;
    if (built_) {
        refreshValues();
    }
}
