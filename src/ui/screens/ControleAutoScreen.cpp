#include "Screens.h"
#include "NavShell.h"
#include "ReservoirConfig.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

#include <cstdio>
#include <cstdlib>

namespace {

constexpr lv_coord_t kKbH = AppTheme::KEYBOARD_H;
constexpr lv_coord_t kEditBarH = 40;
constexpr lv_coord_t kSaveReserve = AppTheme::BTN_PRIMARY_H + 12;
/** Filas Cadeado/Armado (en host al desbloquear; sticky en root si Cadeado cerrado). */
constexpr lv_coord_t kCtrlRowH = AppTheme::TOUCH_MIN_H + 8;
constexpr lv_coord_t kArmBand = (kCtrlRowH + 4) * 2;

enum class Field : uint8_t { EcIv = 0, PhIv, MaxStepEc, MaxStepPh, Count };

struct Draft {
    float ecIv = 30.0f;
    float phIv = 30.0f;
    float maxStepEc = 50.0f;
    float maxStepPh = 50.0f;
    bool autoEc = false;
    bool autoPh = false;
    bool consumoDiario = false;
    bool consumoPh24h = false;
    bool armed = false;
    bool uiLocked = true;
};

lv_obj_t *root_ = nullptr;
lv_obj_t *lockRow_ = nullptr;
lv_obj_t *armRow_ = nullptr;
lv_obj_t *host_ = nullptr;
lv_obj_t *lockVeil_ = nullptr;
lv_obj_t *kb_ = nullptr;
lv_obj_t *editBar_ = nullptr;
lv_obj_t *editTa_ = nullptr;
lv_obj_t *editTitle_ = nullptr;
lv_obj_t *saveBtn_ = nullptr;
lv_obj_t *persistHint_ = nullptr;
Field editingField_ = Field::Count;
Draft draft_{};

lv_obj_t *valLbls[static_cast<size_t>(Field::Count)] = {};
lv_obj_t *btnEcOn = nullptr;
lv_obj_t *btnEcOff = nullptr;
lv_obj_t *btnPhOn = nullptr;
lv_obj_t *btnPhOff = nullptr;
lv_obj_t *btnConsOn = nullptr;
lv_obj_t *btnConsOff = nullptr;
lv_obj_t *btnConsPhOn = nullptr;
lv_obj_t *btnConsPhOff = nullptr;
lv_obj_t *lockSwitchBtn = nullptr;
lv_obj_t *lockSwitchLbl = nullptr;
lv_obj_t *armSwitchBtn = nullptr;
lv_obj_t *armSwitchLbl = nullptr;
lv_obj_t *armConfirm_ = nullptr;
lv_obj_t *armToast_ = nullptr;
lv_timer_t *armToastTimer_ = nullptr;

void hideNumericKb();
void refreshNums();
void refreshToggles();
void applyLockUi();
void hideArmConfirm();
void showArmConfirm();
void hideArmToast();
void showArmToast();
void commitArm(bool armed);

float clampf(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

float snapAggrPct(float raw) {
    float pct = (raw <= 1.0f) ? (raw * 100.0f) : raw;
    pct = clampf(pct, 0.0f, 100.0f);
    const int step = static_cast<int>((pct + 5.0f) / 10.0f) * 10;
    return static_cast<float>(clampf(static_cast<float>(step), 0.0f, 100.0f));
}

void loadDraft() {
    draft_.ecIv = ReservoirConfig::autoEcIntervalSec();
    draft_.phIv = ReservoirConfig::autoPhIntervalSec();
    draft_.maxStepEc = ReservoirConfig::maxStepEc();
    draft_.maxStepPh = ReservoirConfig::maxStepPh();
    draft_.autoEc = ReservoirConfig::autoEcEnabled();
    draft_.autoPh = ReservoirConfig::autoPhEnabled();
    draft_.consumoDiario = ReservoirConfig::consumoDiarioEnabled();
    draft_.consumoPh24h = ReservoirConfig::consumoPh24hEnabled();
    draft_.armed = ReservoirConfig::dosingArmed();
    draft_.uiLocked = ReservoirConfig::autoUiLocked();
}

void commitDraft() {
    ReservoirConfig::beginBatch();
    ReservoirConfig::setAutoEcIntervalSec(draft_.ecIv);
    ReservoirConfig::setAutoPhIntervalSec(draft_.phIv);
    ReservoirConfig::setMaxStepEc(draft_.maxStepEc);
    ReservoirConfig::setMaxStepPh(draft_.maxStepPh);
    ReservoirConfig::setAutoEcEnabled(draft_.autoEc);
    ReservoirConfig::setAutoPhEnabled(draft_.autoPh);
    ReservoirConfig::setConsumoDiarioEnabled(draft_.consumoDiario);
    ReservoirConfig::setConsumoPh24hEnabled(draft_.consumoPh24h);
    ReservoirConfig::setDosingArmed(draft_.armed);
    ReservoirConfig::setAutoUiLocked(draft_.uiLocked);
    ReservoirConfig::endBatch(true);
}

float fieldValue(Field f) {
    switch (f) {
    case Field::EcIv:
        return draft_.ecIv;
    case Field::PhIv:
        return draft_.phIv;
    case Field::MaxStepEc:
        return draft_.maxStepEc;
    case Field::MaxStepPh:
        return draft_.maxStepPh;
    default:
        return 0.0f;
    }
}

const char *fieldFmt(Field f) {
    (void)f;
    return "%.0f";
}

const char *fieldTitle(Field f) {
    switch (f) {
    case Field::EcIv:
        return Strings::tr(Msg::ReservoirAutoEcIv);
    case Field::PhIv:
        return Strings::tr(Msg::ReservoirAutoPhIv);
    case Field::MaxStepEc:
        return Strings::tr(Msg::ReservoirMaxStepEc);
    case Field::MaxStepPh:
        return Strings::tr(Msg::ReservoirMaxStepPh);
    default:
        return "";
    }
}

void applyField(Field f, float v) {
    switch (f) {
    case Field::EcIv:
        draft_.ecIv = clampf(v, 5.0f, 3600.0f);
        break;
    case Field::PhIv:
        draft_.phIv = clampf(v, 5.0f, 3600.0f);
        break;
    case Field::MaxStepEc:
        draft_.maxStepEc = snapAggrPct(v);
        break;
    case Field::MaxStepPh:
        draft_.maxStepPh = snapAggrPct(v);
        break;
    default:
        break;
    }
}

lv_coord_t hostTop() { return AppTheme::MENU_LIST_TOP; }

lv_coord_t stickyCtrlY0() { return AppTheme::MENU_LIST_TOP + 2; }

lv_coord_t veilTop() {
    /* Con Cadeado cerrado, cortina bajo las filas sticky Cadeado/Armado. */
    return draft_.uiLocked ? (AppTheme::MENU_LIST_TOP + kArmBand) : hostTop();
}

lv_coord_t hostHeight(bool keyboardOpen) {
    lv_coord_t h = LCD_V_RES - hostTop();
    if (!keyboardOpen) {
        h -= kSaveReserve;
    } else {
        h -= (kKbH + kEditBarH);
    }
    return h > 40 ? h : 40;
}

void placeCtrlRowsInHost() {
    if (!host_ || !lockRow_ || !armRow_) {
        return;
    }
    if (lv_obj_get_parent(lockRow_) != host_) {
        lv_obj_set_parent(lockRow_, host_);
    }
    if (lv_obj_get_parent(armRow_) != host_) {
        lv_obj_set_parent(armRow_, host_);
    }
    lv_obj_align(lockRow_, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_align(armRow_, LV_ALIGN_TOP_MID, 0, 4 + kCtrlRowH + 4);
    lv_obj_move_to_index(lockRow_, 0);
    lv_obj_move_to_index(armRow_, 1);
}

void stickCtrlRowsToRoot() {
    if (!root_ || !lockRow_ || !armRow_) {
        return;
    }
    const lv_coord_t y0 = stickyCtrlY0();
    if (lv_obj_get_parent(lockRow_) != root_) {
        lv_obj_set_parent(lockRow_, root_);
    }
    if (lv_obj_get_parent(armRow_) != root_) {
        lv_obj_set_parent(armRow_, root_);
    }
    lv_obj_align(lockRow_, LV_ALIGN_TOP_MID, 0, y0);
    lv_obj_align(armRow_, LV_ALIGN_TOP_MID, 0, y0 + kCtrlRowH + 4);
    lv_obj_move_foreground(lockRow_);
    lv_obj_move_foreground(armRow_);
}

void onBack(lv_event_t *) {
    hideNumericKb();
    hideArmConfirm();
    hideArmToast();
    NavShell::back();
}

void onSave(lv_event_t *) {
    if (draft_.uiLocked) {
        return;
    }
    hideNumericKb();
    commitDraft();
    NavShell::back();
}

void hideNumericKb() {
    if (kb_) {
        lv_keyboard_set_textarea(kb_, nullptr);
        lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    }
    if (editBar_) {
        lv_obj_add_flag(editBar_, LV_OBJ_FLAG_HIDDEN);
    }
    if (saveBtn_ && !draft_.uiLocked) {
        lv_obj_clear_flag(saveBtn_, LV_OBJ_FLAG_HIDDEN);
    }
    editingField_ = Field::Count;
    if (host_) {
        lv_obj_set_height(host_, hostHeight(false));
    }
}

void showNumericKb(Field f) {
    if (draft_.uiLocked) {
        return;
    }
    editingField_ = f;
    if (!kb_ || !editBar_ || !editTa_ || !editTitle_) {
        return;
    }
    lv_label_set_text(editTitle_, fieldTitle(f));
    char buf[24];
    snprintf(buf, sizeof(buf), fieldFmt(f), static_cast<double>(fieldValue(f)));
    lv_textarea_set_text(editTa_, buf);
    if (saveBtn_) {
        lv_obj_add_flag(saveBtn_, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_height(host_, hostHeight(true));
    lv_obj_clear_flag(editBar_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(kb_, editTa_);
    lv_obj_move_foreground(editBar_);
    lv_obj_move_foreground(kb_);
}

void onKbDone(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_READY && code != LV_EVENT_CANCEL) {
        return;
    }
    if (code == LV_EVENT_READY && editTa_ && editingField_ != Field::Count) {
        const char *txt = lv_textarea_get_text(editTa_);
        if (txt && txt[0]) {
            applyField(editingField_, static_cast<float>(atof(txt)));
        }
    }
    hideNumericKb();
    refreshNums();
}

void refreshNums() {
    char buf[24];
    for (size_t i = 0; i < static_cast<size_t>(Field::Count); ++i) {
        if (!valLbls[i]) {
            continue;
        }
        const Field f = static_cast<Field>(i);
        if (f == Field::MaxStepEc || f == Field::MaxStepPh) {
            snprintf(buf, sizeof(buf), "%.0f%%", static_cast<double>(fieldValue(f)));
        } else {
            snprintf(buf, sizeof(buf), fieldFmt(f), static_cast<double>(fieldValue(f)));
        }
        lv_label_set_text(valLbls[i], buf);
    }
}

void paintToggle(lv_obj_t *onBtn, lv_obj_t *offBtn, bool on) {
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

void paintLock() {
    if (!lockSwitchBtn || !lockSwitchLbl) {
        return;
    }
    const bool locked = draft_.uiLocked;
    const char *txt =
        locked ? Strings::tr(Msg::AutoLockBlocked) : Strings::tr(Msg::AutoLockReleased);
    lv_label_set_text(lockSwitchLbl, txt);
    lv_obj_set_style_bg_color(lockSwitchBtn, locked ? AppTheme::surfaceAlt() : AppTheme::accent(),
                              0);
    lv_obj_set_style_border_color(lockSwitchBtn,
                                  locked ? AppTheme::gridLine() : AppTheme::accent(), 0);
    lv_obj_set_style_text_color(lockSwitchLbl, locked ? AppTheme::muted() : AppTheme::bg(), 0);
}

void paintArm() {
    if (!armSwitchBtn || !armSwitchLbl) {
        return;
    }
    const bool armed = draft_.armed;
    const char *txt = armed ? Strings::tr(Msg::DosingActive) : Strings::tr(Msg::DosingInactive);
    lv_label_set_text(armSwitchLbl, txt);
    lv_obj_set_style_bg_color(armSwitchBtn, armed ? AppTheme::accent() : AppTheme::surfaceAlt(), 0);
    lv_obj_set_style_border_color(armSwitchBtn, armed ? AppTheme::accent() : AppTheme::gridLine(), 0);
    lv_obj_set_style_text_color(armSwitchLbl, armed ? AppTheme::bg() : AppTheme::muted(), 0);
}

/** Cadeado cerrado: cortina sobre host+Save; Cadeado/Armado sticky tocables en root. */
void applyLockUi() {
    const bool locked = draft_.uiLocked;
    if (locked) {
        hideNumericKb();
        stickCtrlRowsToRoot();
        if (host_) {
            lv_obj_add_state(host_, LV_STATE_DISABLED);
            lv_obj_clear_flag(host_, LV_OBJ_FLAG_SCROLLABLE);
        }
        if (saveBtn_) {
            lv_obj_add_state(saveBtn_, LV_STATE_DISABLED);
            lv_obj_clear_flag(saveBtn_, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(saveBtn_, LV_OBJ_FLAG_HIDDEN);
        }
        if (lockVeil_) {
            lv_obj_set_size(lockVeil_, LCD_H_RES, LCD_V_RES - veilTop());
            lv_obj_align(lockVeil_, LV_ALIGN_TOP_LEFT, 0, veilTop());
            lv_obj_clear_flag(lockVeil_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(lockVeil_);
        }
        if (lockRow_) {
            lv_obj_move_foreground(lockRow_);
        }
        if (armRow_) {
            lv_obj_move_foreground(armRow_);
        }
    } else {
        if (lockVeil_) {
            lv_obj_add_flag(lockVeil_, LV_OBJ_FLAG_HIDDEN);
        }
        placeCtrlRowsInHost();
        if (host_) {
            lv_obj_clear_state(host_, LV_STATE_DISABLED);
            lv_obj_add_flag(host_, LV_OBJ_FLAG_SCROLLABLE);
        }
        if (saveBtn_) {
            lv_obj_clear_state(saveBtn_, LV_STATE_DISABLED);
            lv_obj_add_flag(saveBtn_, LV_OBJ_FLAG_CLICKABLE);
            if (editingField_ == Field::Count) {
                lv_obj_clear_flag(saveBtn_, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

void refreshToggles() {
    paintToggle(btnEcOn, btnEcOff, draft_.autoEc);
    paintToggle(btnPhOn, btnPhOff, draft_.autoPh);
    paintToggle(btnConsOn, btnConsOff, draft_.consumoDiario);
    paintToggle(btnConsPhOn, btnConsPhOff, draft_.consumoPh24h);
    paintLock();
    paintArm();
    applyLockUi();
}

void onMinus(lv_event_t *e) {
    if (draft_.uiLocked) {
        return;
    }
    const Field f = static_cast<Field>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    switch (f) {
    case Field::EcIv:
        applyField(f, draft_.ecIv - 5.0f);
        break;
    case Field::PhIv:
        applyField(f, draft_.phIv - 5.0f);
        break;
    case Field::MaxStepEc:
        applyField(f, draft_.maxStepEc - 10.0f);
        break;
    case Field::MaxStepPh:
        applyField(f, draft_.maxStepPh - 10.0f);
        break;
    default:
        break;
    }
    refreshNums();
}

void onPlus(lv_event_t *e) {
    if (draft_.uiLocked) {
        return;
    }
    const Field f = static_cast<Field>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    switch (f) {
    case Field::EcIv:
        applyField(f, draft_.ecIv + 5.0f);
        break;
    case Field::PhIv:
        applyField(f, draft_.phIv + 5.0f);
        break;
    case Field::MaxStepEc:
        applyField(f, draft_.maxStepEc + 10.0f);
        break;
    case Field::MaxStepPh:
        applyField(f, draft_.maxStepPh + 10.0f);
        break;
    default:
        break;
    }
    refreshNums();
}

void onEcOn(lv_event_t *) {
    if (draft_.uiLocked) {
        return;
    }
    draft_.autoEc = true;
    ReservoirConfig::setAutoEcEnabled(true);
    refreshToggles();
}
void onEcOff(lv_event_t *) {
    if (draft_.uiLocked) {
        return;
    }
    draft_.autoEc = false;
    ReservoirConfig::setAutoEcEnabled(false);
    refreshToggles();
}
void onPhOn(lv_event_t *) {
    if (draft_.uiLocked) {
        return;
    }
    draft_.autoPh = true;
    ReservoirConfig::setAutoPhEnabled(true);
    refreshToggles();
}
void onPhOff(lv_event_t *) {
    if (draft_.uiLocked) {
        return;
    }
    draft_.autoPh = false;
    ReservoirConfig::setAutoPhEnabled(false);
    refreshToggles();
}
void onConsOn(lv_event_t *) {
    if (draft_.uiLocked) {
        return;
    }
    draft_.consumoDiario = true;
    ReservoirConfig::setConsumoDiarioEnabled(true);
    refreshToggles();
}
void onConsOff(lv_event_t *) {
    if (draft_.uiLocked) {
        return;
    }
    draft_.consumoDiario = false;
    ReservoirConfig::setConsumoDiarioEnabled(false);
    refreshToggles();
}
void onConsPhOn(lv_event_t *) {
    if (draft_.uiLocked) {
        return;
    }
    draft_.consumoPh24h = true;
    ReservoirConfig::setConsumoPh24hEnabled(true);
    refreshToggles();
}
void onConsPhOff(lv_event_t *) {
    if (draft_.uiLocked) {
        return;
    }
    draft_.consumoPh24h = false;
    ReservoirConfig::setConsumoPh24hEnabled(false);
    refreshToggles();
}
void hideArmConfirm() {
    if (armConfirm_) {
        lv_obj_add_flag(armConfirm_, LV_OBJ_FLAG_HIDDEN);
    }
}

void hideArmToast() {
    if (armToastTimer_) {
        lv_timer_del(armToastTimer_);
        armToastTimer_ = nullptr;
    }
    if (armToast_) {
        lv_obj_add_flag(armToast_, LV_OBJ_FLAG_HIDDEN);
    }
}

void onArmToastTimer(lv_timer_t *t) {
    (void)t;
    hideArmToast();
}

void showArmToast() {
    if (!armToast_) {
        return;
    }
    hideArmToast();
    lv_obj_clear_flag(armToast_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(armToast_);
    armToastTimer_ = lv_timer_create(onArmToastTimer, 1800, nullptr);
    lv_timer_set_repeat_count(armToastTimer_, 1);
}

void commitArm(bool armed) {
    draft_.armed = armed;
    ReservoirConfig::setDosingArmed(armed);
    refreshToggles();
}

void onArmConfirmOk(lv_event_t *) {
    hideArmConfirm();
    commitArm(true);
    showArmToast();
}

void onArmConfirmCancel(lv_event_t *) {
    hideArmConfirm();
}

void showArmConfirm() {
    if (!armConfirm_) {
        return;
    }
    hideArmToast();
    lv_obj_clear_flag(armConfirm_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(armConfirm_);
}

void onArmSwitch(lv_event_t *) {
    if (draft_.armed) {
        commitArm(false);
        hideArmConfirm();
        hideArmToast();
        return;
    }
    showArmConfirm();
}

void onLockSwitch(lv_event_t *) {
    draft_.uiLocked = !draft_.uiLocked;
    ReservoirConfig::setAutoUiLocked(draft_.uiLocked);
    refreshToggles();
}

void onValTap(lv_event_t *e) {
    if (draft_.uiLocked) {
        return;
    }
    const Field f = static_cast<Field>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    if (f == Field::MaxStepEc || f == Field::MaxStepPh) {
        return;
    }
    showNumericKb(f);
}

lv_obj_t *makeSeg(lv_obj_t *parent, const char *txt, lv_event_cb_t cb) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, 72, AppTheme::TOUCH_MIN_H);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    UiKit::applyPressStyle(btn, AppTheme::surfaceAlt(), AppTheme::surface(), AppTheme::gridLine(),
                           AppTheme::accent());
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, txt);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, AppTheme::text(), 0);
    lv_obj_set_width(lbl, 68);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_obj_center(lbl);
    return btn;
}

void sizeSeg(lv_obj_t *btn, lv_coord_t w) {
    if (!btn) {
        return;
    }
    lv_obj_set_size(btn, w, AppTheme::TOUCH_MIN_H);
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    if (lbl) {
        lv_obj_set_width(lbl, w - 8);
    }
}

void makeNumRow(lv_obj_t *parent, const char *name, Field f, lv_coord_t y, bool withKb) {
    const void *ud = reinterpret_cast<void *>(static_cast<uintptr_t>(f));
    const lv_coord_t rowH = AppTheme::TOUCH_MIN_H + 8;
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LCD_H_RES - 16, rowH);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y);
    UiKit::forceOpaqueBg(row, AppTheme::surface());
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, AppTheme::gridLine(), 0);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, name);
    lv_obj_set_style_text_color(lbl, AppTheme::text(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_width(lbl, 200);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t *minus = UiKit::makeSecondaryButton(row, "-", 44, AppTheme::TOUCH_MIN_H, onMinus);
    lv_obj_remove_event_cb(minus, onMinus);
    lv_obj_add_event_cb(minus, onMinus, LV_EVENT_CLICKED, const_cast<void *>(ud));
    lv_obj_align(minus, LV_ALIGN_RIGHT_MID, -118, 0);

    lv_obj_t *valBtn = lv_btn_create(row);
    lv_obj_remove_style_all(valBtn);
    lv_obj_set_size(valBtn, 72, AppTheme::TOUCH_MIN_H);
    lv_obj_set_style_bg_opa(valBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(valBtn, 0, 0);
    lv_obj_set_style_bg_color(valBtn, AppTheme::surfaceAlt(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(valBtn, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(valBtn, 1, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(valBtn, AppTheme::accent(), LV_STATE_PRESSED);
    if (withKb) {
        lv_obj_add_flag(valBtn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(valBtn, onValTap, LV_EVENT_CLICKED, const_cast<void *>(ud));
    } else {
        lv_obj_clear_flag(valBtn, LV_OBJ_FLAG_CLICKABLE);
    }
    lv_obj_align(valBtn, LV_ALIGN_RIGHT_MID, -50, 0);

    lv_obj_t *val = lv_label_create(valBtn);
    lv_label_set_text(val, "--");
    lv_obj_set_style_text_color(val, AppTheme::accent(), 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_20, 0);
    lv_obj_set_width(val, 72);
    lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(val);
    valLbls[static_cast<size_t>(f)] = val;

    lv_obj_t *plus = UiKit::makeSecondaryButton(row, "+", 44, AppTheme::TOUCH_MIN_H, onPlus);
    lv_obj_remove_event_cb(plus, onPlus);
    lv_obj_add_event_cb(plus, onPlus, LV_EVENT_CLICKED, const_cast<void *>(ud));
    lv_obj_align(plus, LV_ALIGN_RIGHT_MID, -8, 0);
}

void makeToggleRow(lv_obj_t *parent, const char *name, lv_coord_t y, lv_obj_t **onOut,
                   lv_obj_t **offOut, lv_event_cb_t onCb, lv_event_cb_t offCb) {
    const lv_coord_t rowH = AppTheme::TOUCH_MIN_H + 8;
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LCD_H_RES - 16, rowH);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y);
    UiKit::forceOpaqueBg(row, AppTheme::surface());
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, AppTheme::gridLine(), 0);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, name);
    lv_obj_set_style_text_color(lbl, AppTheme::text(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 10, 0);

    /* OFF izquierda · ON derecha — mismo orden que Armado / Central. */
    *offOut = makeSeg(row, Strings::tr(Msg::OffLabel), offCb);
    lv_obj_align(*offOut, LV_ALIGN_RIGHT_MID, -84, 0);
    *onOut = makeSeg(row, Strings::tr(Msg::OnLabel), onCb);
    lv_obj_align(*onOut, LV_ALIGN_RIGHT_MID, -8, 0);
}

}  // namespace

lv_obj_t *Screens::createControleAuto(lv_obj_t *parent) {
    for (size_t i = 0; i < static_cast<size_t>(Field::Count); ++i) {
        valLbls[i] = nullptr;
    }
    btnEcOn = btnEcOff = btnPhOn = btnPhOff = nullptr;
    btnConsOn = btnConsOff = nullptr;
    btnConsPhOn = btnConsPhOff = nullptr;
    lockSwitchBtn = lockSwitchLbl = nullptr;
    armSwitchBtn = armSwitchLbl = nullptr;
    armConfirm_ = nullptr;
    armToast_ = nullptr;
    armToastTimer_ = nullptr;
    root_ = nullptr;
    lockRow_ = nullptr;
    armRow_ = nullptr;
    host_ = nullptr;
    lockVeil_ = nullptr;
    kb_ = nullptr;
    editBar_ = nullptr;
    editTa_ = nullptr;
    editTitle_ = nullptr;
    saveBtn_ = nullptr;
    persistHint_ = nullptr;
    editingField_ = Field::Count;
    loadDraft();

    root_ = lv_obj_create(parent);
    UiKit::styleScreen(root_);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    UiKit::styleHeader(root_, Strings::tr(Msg::ControleAuto), onBack);

    auto makeCtrlRow = [](lv_obj_t *parent, lv_coord_t y, const char *title, lv_event_cb_t onTap,
                          lv_obj_t **btnOut, lv_obj_t **lblOut) -> lv_obj_t * {
        lv_obj_t *row = lv_obj_create(parent);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LCD_H_RES - 16, kCtrlRowH);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y);
        UiKit::forceOpaqueBg(row, AppTheme::surface());
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, AppTheme::gridLine(), 0);

        lv_obj_t *lbl = lv_label_create(row);
        lv_label_set_text(lbl, title);
        lv_obj_set_style_text_color(lbl, AppTheme::text(), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
        lv_obj_set_width(lbl, 140);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 10, 0);

        lv_obj_t *btn = lv_btn_create(row);
        lv_obj_remove_style_all(btn);
        lv_obj_set_size(btn, 120, AppTheme::TOUCH_MIN_H);
        lv_obj_set_style_radius(btn, 0, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        UiKit::applyPressStyle(btn, AppTheme::surfaceAlt(), AppTheme::surface(), AppTheme::gridLine(),
                               AppTheme::accent());
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(btn, onTap, LV_EVENT_CLICKED, nullptr);
        lv_obj_align(btn, LV_ALIGN_RIGHT_MID, -8, 0);

        lv_obj_t *swLbl = lv_label_create(btn);
        lv_obj_set_style_text_font(swLbl, &lv_font_montserrat_14, 0);
        lv_obj_set_width(swLbl, 112);
        lv_obj_set_style_text_align(swLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(swLbl, LV_LABEL_LONG_CLIP);
        lv_obj_center(swLbl);

        *btnOut = btn;
        *lblOut = swLbl;
        return row;
    };

    host_ = lv_obj_create(root_);
    lv_obj_remove_style_all(host_);
    lv_obj_set_size(host_, LCD_H_RES, hostHeight(false));
    lv_obj_align(host_, LV_ALIGN_TOP_LEFT, 0, hostTop());
    lv_obj_set_style_bg_opa(host_, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(host_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(host_, LV_DIR_VER);
    lv_obj_set_style_pad_bottom(host_, 12, 0);

    lockRow_ = makeCtrlRow(host_, 4, Strings::tr(Msg::AutoUiLockLabel), onLockSwitch, &lockSwitchBtn,
                           &lockSwitchLbl);
    armRow_ = makeCtrlRow(host_, 4 + kCtrlRowH + 4, Strings::tr(Msg::DosingArm), onArmSwitch,
                          &armSwitchBtn, &armSwitchLbl);

    lv_coord_t y = 4 + kArmBand + 4;
    const lv_coord_t step = AppTheme::TOUCH_MIN_H + 12;

    makeNumRow(host_, Strings::tr(Msg::ReservoirAutoEcIv), Field::EcIv, y, true);
    y += step;
    makeToggleRow(host_, Strings::tr(Msg::ReservoirAutoEc), y, &btnEcOn, &btnEcOff, onEcOn, onEcOff);
    y += step;
    makeToggleRow(host_, Strings::tr(Msg::ConsumoDiario), y, &btnConsOn, &btnConsOff, onConsOn,
                  onConsOff);
    y += step;
    makeNumRow(host_, Strings::tr(Msg::ReservoirAutoPhIv), Field::PhIv, y, true);
    y += step;
    makeToggleRow(host_, Strings::tr(Msg::ReservoirAutoPh), y, &btnPhOn, &btnPhOff, onPhOn, onPhOff);
    y += step;
    makeToggleRow(host_, Strings::tr(Msg::ConsumoPh24h), y, &btnConsPhOn, &btnConsPhOff, onConsPhOn,
                  onConsPhOff);
    y += step;
    makeNumRow(host_, Strings::tr(Msg::ReservoirMaxStepEc), Field::MaxStepEc, y, false);
    y += step;
    makeNumRow(host_, Strings::tr(Msg::ReservoirMaxStepPh), Field::MaxStepPh, y, false);
    y += step + 4;

    persistHint_ = lv_label_create(host_);
    lv_label_set_text(persistHint_, Strings::tr(Msg::AutoPersistHint));
    UiKit::styleHint(persistHint_);
    lv_obj_set_width(persistHint_, LCD_H_RES - 24);
    lv_label_set_long_mode(persistHint_, LV_LABEL_LONG_WRAP);
    lv_obj_align(persistHint_, LV_ALIGN_TOP_MID, 0, y);

    saveBtn_ = UiKit::makePrimaryButton(root_, Strings::tr(Msg::NutSave), onSave);
    lv_obj_set_size(saveBtn_, LCD_H_RES - 16, AppTheme::BTN_PRIMARY_H);
    lv_obj_align(saveBtn_, LV_ALIGN_BOTTOM_MID, 0, -6);

    /* Cortina: bajo Cadeado/Armado sticky cuando cerrado. */
    lockVeil_ = lv_obj_create(root_);
    lv_obj_remove_style_all(lockVeil_);
    lv_obj_set_size(lockVeil_, LCD_H_RES, LCD_V_RES - veilTop());
    lv_obj_align(lockVeil_, LV_ALIGN_TOP_LEFT, 0, veilTop());
    lv_obj_set_style_bg_color(lockVeil_, AppTheme::bg(), 0);
    lv_obj_set_style_bg_opa(lockVeil_, LV_OPA_70, 0);
    lv_obj_add_flag(lockVeil_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(lockVeil_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(lockVeil_, LV_OBJ_FLAG_HIDDEN);

    editBar_ = lv_obj_create(root_);
    lv_obj_remove_style_all(editBar_);
    lv_obj_set_size(editBar_, LCD_H_RES, kEditBarH);
    lv_obj_align(editBar_, LV_ALIGN_BOTTOM_MID, 0, -kKbH);
    UiKit::forceOpaqueBg(editBar_, AppTheme::surface());
    lv_obj_set_style_border_width(editBar_, 1, 0);
    lv_obj_set_style_border_color(editBar_, AppTheme::gridLine(), 0);
    lv_obj_add_flag(editBar_, LV_OBJ_FLAG_HIDDEN);

    editTitle_ = lv_label_create(editBar_);
    lv_obj_set_style_text_color(editTitle_, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(editTitle_, &lv_font_montserrat_14, 0);
    lv_obj_align(editTitle_, LV_ALIGN_LEFT_MID, 8, 0);

    editTa_ = lv_textarea_create(editBar_);
    lv_textarea_set_one_line(editTa_, true);
    lv_textarea_set_accepted_chars(editTa_, "0123456789.+-");
    lv_textarea_set_max_length(editTa_, 12);
    lv_obj_set_size(editTa_, 160, 32);
    lv_obj_align(editTa_, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_set_style_bg_color(editTa_, AppTheme::bg(), 0);
    lv_obj_set_style_bg_opa(editTa_, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(editTa_, AppTheme::text(), 0);
    lv_obj_set_style_border_width(editTa_, 1, 0);
    lv_obj_set_style_border_color(editTa_, AppTheme::accent(), 0);
    lv_obj_set_style_radius(editTa_, 0, 0);
    lv_obj_set_style_pad_all(editTa_, 4, 0);

    kb_ = lv_keyboard_create(root_);
    lv_obj_set_size(kb_, LCD_H_RES, kKbH);
    lv_obj_align(kb_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    UiKit::attachNumericKeyboard(kb_, editTa_, onKbDone);

    /* Confirmación Armado → ACTIVO: velo + panel (ISA: acción irreversible de proceso). */
    armConfirm_ = lv_obj_create(root_);
    lv_obj_remove_style_all(armConfirm_);
    lv_obj_set_size(armConfirm_, LCD_H_RES, LCD_V_RES);
    lv_obj_set_pos(armConfirm_, 0, 0);
    lv_obj_set_style_bg_color(armConfirm_, AppTheme::bg(), 0);
    lv_obj_set_style_bg_opa(armConfirm_, LV_OPA_80, 0);
    lv_obj_set_style_border_width(armConfirm_, 0, 0);
    lv_obj_set_style_pad_all(armConfirm_, 0, 0);
    lv_obj_clear_flag(armConfirm_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(armConfirm_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(armConfirm_, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *panel = lv_obj_create(armConfirm_);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, LCD_H_RES - 28, 196);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    UiKit::forceOpaqueBg(panel, AppTheme::surface());
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, AppTheme::gridLine(), 0);
    lv_obj_set_style_radius(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *bar = lv_obj_create(panel);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, LCD_H_RES - 28, 4);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    UiKit::forceOpaqueBg(bar, AppTheme::warn());
    lv_obj_set_style_border_width(bar, 0, 0);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, Strings::tr(Msg::ArmConfirmTitle));
    lv_obj_set_style_text_color(title, AppTheme::text(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_width(title, LCD_H_RES - 56);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    lv_obj_t *body = lv_label_create(panel);
    lv_label_set_text(body, Strings::tr(Msg::ArmConfirmBody));
    lv_obj_set_style_text_color(body, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(body, &lv_font_montserrat_14, 0);
    lv_obj_set_width(body, LCD_H_RES - 56);
    lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, 56);

    const lv_coord_t btnW = (LCD_H_RES - 28 - 36) / 2;
    lv_obj_t *cancelBtn =
        UiKit::makeSecondaryButton(panel, Strings::tr(Msg::ArmConfirmCancel), btnW,
                                   AppTheme::BTN_PRIMARY_H, onArmConfirmCancel);
    lv_obj_align(cancelBtn, LV_ALIGN_BOTTOM_LEFT, 12, -12);

    lv_obj_t *okBtn = UiKit::makeCautionButton(panel, Strings::tr(Msg::ArmConfirmOk), onArmConfirmOk);
    lv_obj_set_size(okBtn, btnW, AppTheme::BTN_PRIMARY_H);
    lv_obj_align(okBtn, LV_ALIGN_BOTTOM_RIGHT, -12, -12);

    /* Toast breve tras confirmar (feedback calmado, no celebración). */
    armToast_ = lv_obj_create(root_);
    lv_obj_remove_style_all(armToast_);
    lv_obj_set_size(armToast_, LCD_H_RES - 40, 44);
    lv_obj_align(armToast_, LV_ALIGN_BOTTOM_MID, 0, -(AppTheme::BTN_PRIMARY_H + 20));
    UiKit::forceOpaqueBg(armToast_, AppTheme::surfaceAlt());
    lv_obj_set_style_border_width(armToast_, 1, 0);
    lv_obj_set_style_border_color(armToast_, AppTheme::accent(), 0);
    lv_obj_set_style_radius(armToast_, 0, 0);
    lv_obj_clear_flag(armToast_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(armToast_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(armToast_, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *toastBar = lv_obj_create(armToast_);
    lv_obj_remove_style_all(toastBar);
    lv_obj_set_size(toastBar, 4, 44);
    lv_obj_align(toastBar, LV_ALIGN_LEFT_MID, 0, 0);
    UiKit::forceOpaqueBg(toastBar, AppTheme::accent());
    lv_obj_set_style_border_width(toastBar, 0, 0);

    lv_obj_t *toastLbl = lv_label_create(armToast_);
    lv_label_set_text(toastLbl, Strings::tr(Msg::ArmToastActive));
    lv_obj_set_style_text_color(toastLbl, AppTheme::text(), 0);
    lv_obj_set_style_text_font(toastLbl, &lv_font_montserrat_16, 0);
    lv_obj_set_width(toastLbl, LCD_H_RES - 72);
    lv_obj_set_style_text_align(toastLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(toastLbl, LV_LABEL_LONG_CLIP);
    lv_obj_align(toastLbl, LV_ALIGN_CENTER, 4, 0);

    refreshNums();
    refreshToggles();
    return root_;
}

void Screens::refreshControleAuto(lv_obj_t *root) {
    if (!root_ || root != root_) {
        return;
    }
    /* Flags en vivo = sistema (Central / UART); números siguen en borrador hasta Save. */
    draft_.armed = ReservoirConfig::dosingArmed();
    draft_.uiLocked = ReservoirConfig::autoUiLocked();
    draft_.autoEc = ReservoirConfig::autoEcEnabled();
    draft_.autoPh = ReservoirConfig::autoPhEnabled();
    draft_.consumoDiario = ReservoirConfig::consumoDiarioEnabled();
    draft_.consumoPh24h = ReservoirConfig::consumoPh24hEnabled();
    refreshToggles();
}
