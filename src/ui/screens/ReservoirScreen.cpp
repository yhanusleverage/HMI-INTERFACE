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

enum class Field : uint8_t {
    Volume = 0,
    Homo,
    DoseDelay,
    NutGap,
    PulseMl,
    PulseGap,
    Count
};

struct Draft {
    float volumeL = 50.0f;
    float homoSec = 60.0f;
    float dosingDelaySec = 60.0f;
    float nutGapSec = 3.0f;
    float pulseMl = 2.0f;
    float pulseGapSec = 2.0f;
    ReservoirConfig::DosingMode mode = ReservoirConfig::DosingMode::Batch;
};

lv_obj_t *root_ = nullptr;
lv_obj_t *host_ = nullptr;
lv_obj_t *recircPanel_ = nullptr;
lv_coord_t recircPanelH_ = 0;
lv_obj_t *kb_ = nullptr;
lv_obj_t *editBar_ = nullptr;
lv_obj_t *editTa_ = nullptr;
lv_obj_t *editTitle_ = nullptr;
lv_obj_t *saveBtn_ = nullptr;
Field editingField_ = Field::Count;
Draft draft_{};
bool wizard_ = false;

lv_obj_t *valLbls[static_cast<size_t>(Field::Count)] = {};
lv_obj_t *btnModeBatch = nullptr;
lv_obj_t *btnModeRecirc = nullptr;

float clampf(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

void loadDraft() {
    draft_.volumeL = ReservoirConfig::volumeL();
    draft_.homoSec = ReservoirConfig::homoSec();
    draft_.dosingDelaySec = ReservoirConfig::dosingDelaySec();
    draft_.nutGapSec = ReservoirConfig::nutrientGapSec();
    draft_.pulseMl = ReservoirConfig::pulseMl();
    draft_.pulseGapSec = ReservoirConfig::pulseGapSec();
    draft_.mode = ReservoirConfig::dosingMode();
}

void commitDraft() {
    ReservoirConfig::beginBatch();
    ReservoirConfig::setVolumeL(draft_.volumeL);
    ReservoirConfig::setHomoSec(draft_.homoSec);
    ReservoirConfig::setDosingDelaySec(draft_.dosingDelaySec);
    ReservoirConfig::setNutrientGapSec(draft_.nutGapSec);
    ReservoirConfig::setPulseMl(draft_.pulseMl);
    ReservoirConfig::setPulseGapSec(draft_.pulseGapSec);
    ReservoirConfig::setDosingMode(draft_.mode);
    ReservoirConfig::endBatch(true);
}

float fieldValue(Field f) {
    switch (f) {
    case Field::Volume:
        return draft_.volumeL;
    case Field::Homo:
        return draft_.homoSec;
    case Field::DoseDelay:
        return draft_.dosingDelaySec;
    case Field::NutGap:
        return draft_.nutGapSec;
    case Field::PulseMl:
        return draft_.pulseMl;
    case Field::PulseGap:
        return draft_.pulseGapSec;
    default:
        return 0.0f;
    }
}

const char *fieldFmt(Field f) {
    switch (f) {
    case Field::PulseMl:
        return "%.1f";
    default:
        return "%.0f";
    }
}

const char *fieldTitle(Field f) {
    switch (f) {
    case Field::Volume:
        return Strings::tr(Msg::ReservoirVolumeL);
    case Field::Homo:
        return Strings::tr(Msg::ReservoirHomoSec);
    case Field::DoseDelay:
        return Strings::tr(Msg::ReservoirDosingDelay);
    case Field::NutGap:
        return Strings::tr(Msg::ReservoirNutGapSec);
    case Field::PulseMl:
        return Strings::tr(Msg::ReservoirPulseMl);
    case Field::PulseGap:
        return Strings::tr(Msg::ReservoirPulseGapSec);
    default:
        return "";
    }
}

void applyField(Field f, float v) {
    switch (f) {
    case Field::Volume:
        draft_.volumeL = clampf(v, 1.0f, 10000.0f);
        break;
    case Field::Homo:
        draft_.homoSec = clampf(v, 0.0f, 3600.0f);
        break;
    case Field::DoseDelay:
        draft_.dosingDelaySec = clampf(v, 0.0f, 600.0f);
        break;
    case Field::NutGap:
        draft_.nutGapSec = clampf(v, 0.0f, 600.0f);
        break;
    case Field::PulseMl: {
        float p = clampf(v, 0.1f, 50.0f);
        p = static_cast<float>(static_cast<int>(p * 10.0f + 0.5f)) / 10.0f;
        draft_.pulseMl = p;
        break;
    }
    case Field::PulseGap:
        draft_.pulseGapSec = clampf(v, 0.0f, 120.0f);
        break;
    default:
        break;
    }
}

void setHostHeight(bool keyboardOpen) {
    if (!host_) {
        return;
    }
    const lv_coord_t top = AppTheme::MENU_LIST_TOP;
    lv_coord_t h = LCD_V_RES - top;
    if (!wizard_ && !keyboardOpen) {
        h -= kSaveReserve;
    }
    if (keyboardOpen) {
        h -= (kKbH + kEditBarH);
    }
    lv_obj_set_height(host_, h > 40 ? h : 40);
}

void hideNumericKb() {
    editingField_ = Field::Count;
    if (kb_) {
        lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(kb_, nullptr);
    }
    if (editBar_) {
        lv_obj_add_flag(editBar_, LV_OBJ_FLAG_HIDDEN);
    }
    if (saveBtn_) {
        lv_obj_clear_flag(saveBtn_, LV_OBJ_FLAG_HIDDEN);
    }
    setHostHeight(false);
}

void showNumericKb(Field f) {
    if (!kb_ || !editTa_ || !editBar_) {
        return;
    }
    editingField_ = f;
    char buf[24];
    snprintf(buf, sizeof(buf), fieldFmt(f), static_cast<double>(fieldValue(f)));
    if (editTitle_) {
        lv_label_set_text(editTitle_, fieldTitle(f));
    }
    lv_textarea_set_text(editTa_, buf);
    if (saveBtn_) {
        lv_obj_add_flag(saveBtn_, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(editBar_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(kb_, editTa_);
    lv_obj_move_foreground(editBar_);
    lv_obj_move_foreground(kb_);
    setHostHeight(true);
    lv_obj_add_state(editTa_, LV_STATE_FOCUSED);
}

void onBack(lv_event_t *) {
    if (editingField_ != Field::Count) {
        hideNumericKb();
        return;
    }
    if (NavShell::inWizard()) {
        return;
    }
    NavShell::back();
}

void onSave(lv_event_t *) {
    hideNumericKb();
    commitDraft();
    NavShell::back();
}

void onContinue(lv_event_t *) {
    hideNumericKb();
    commitDraft();
    NavShell::wizardContinue();
}
void onSkip(lv_event_t *) {
    hideNumericKb();
    NavShell::wizardContinue();
}

void refreshNums() {
    char buf[24];
    auto setf = [&](Field f, const char *fmt, double v) {
        if (!valLbls[static_cast<size_t>(f)]) {
            return;
        }
        snprintf(buf, sizeof(buf), fmt, v);
        lv_label_set_text(valLbls[static_cast<size_t>(f)], buf);
    };
    setf(Field::Volume, "%.0f", draft_.volumeL);
    setf(Field::Homo, "%.0f", draft_.homoSec);
    setf(Field::DoseDelay, "%.0f", draft_.dosingDelaySec);
    setf(Field::NutGap, "%.0f", draft_.nutGapSec);
    setf(Field::PulseMl, "%.1f", draft_.pulseMl);
    setf(Field::PulseGap, "%.0f", draft_.pulseGapSec);
}

void paintMode() {
    const bool batch = draft_.mode == ReservoirConfig::DosingMode::Batch;
    if (btnModeBatch && btnModeRecirc) {
        lv_obj_set_style_bg_color(btnModeBatch, batch ? AppTheme::accent() : AppTheme::surfaceAlt(),
                                  0);
        lv_obj_set_style_border_color(btnModeBatch,
                                      batch ? AppTheme::accent() : AppTheme::gridLine(), 0);
        lv_obj_set_style_bg_color(btnModeRecirc, !batch ? AppTheme::accent() : AppTheme::surfaceAlt(),
                                  0);
        lv_obj_set_style_border_color(btnModeRecirc,
                                      !batch ? AppTheme::accent() : AppTheme::gridLine(), 0);
        lv_obj_t *bLbl = lv_obj_get_child(btnModeBatch, 0);
        lv_obj_t *rLbl = lv_obj_get_child(btnModeRecirc, 0);
        if (bLbl) {
            lv_obj_set_style_text_color(bLbl, batch ? AppTheme::bg() : AppTheme::text(), 0);
        }
        if (rLbl) {
            lv_obj_set_style_text_color(rLbl, !batch ? AppTheme::bg() : AppTheme::text(), 0);
        }
    }
}

void applyModeVisibility() {
    const bool recirc = draft_.mode == ReservoirConfig::DosingMode::Recirculating;
    if (recircPanel_) {
        if (recirc) {
            lv_obj_set_height(recircPanel_, recircPanelH_);
            lv_obj_clear_flag(recircPanel_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(recircPanel_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_height(recircPanel_, 0);
        }
    }
    paintMode();
    refreshNums();
}

void onMinus(lv_event_t *e) {
    const Field f = static_cast<Field>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    switch (f) {
    case Field::Volume:
        applyField(f, draft_.volumeL - 5.0f);
        break;
    case Field::Homo:
        applyField(f, draft_.homoSec - 5.0f);
        break;
    case Field::DoseDelay:
        applyField(f, draft_.dosingDelaySec - 5.0f);
        break;
    case Field::NutGap:
        applyField(f, draft_.nutGapSec - 1.0f);
        break;
    case Field::PulseMl:
        applyField(f, draft_.pulseMl - 0.1f);
        break;
    case Field::PulseGap:
        applyField(f, draft_.pulseGapSec - 1.0f);
        break;
    default:
        break;
    }
    refreshNums();
}

void onPlus(lv_event_t *e) {
    const Field f = static_cast<Field>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    switch (f) {
    case Field::Volume:
        applyField(f, draft_.volumeL + 5.0f);
        break;
    case Field::Homo:
        applyField(f, draft_.homoSec + 5.0f);
        break;
    case Field::DoseDelay:
        applyField(f, draft_.dosingDelaySec + 5.0f);
        break;
    case Field::NutGap:
        applyField(f, draft_.nutGapSec + 1.0f);
        break;
    case Field::PulseMl:
        applyField(f, draft_.pulseMl + 0.1f);
        break;
    case Field::PulseGap:
        applyField(f, draft_.pulseGapSec + 1.0f);
        break;
    default:
        break;
    }
    refreshNums();
}

void onModeBatch(lv_event_t *) {
    hideNumericKb();
    draft_.mode = ReservoirConfig::DosingMode::Batch;
    applyModeVisibility();
}
void onModeRecirc(lv_event_t *) {
    hideNumericKb();
    draft_.mode = ReservoirConfig::DosingMode::Recirculating;
    applyModeVisibility();
}

void onValTap(lv_event_t *e) {
    const Field f = static_cast<Field>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    showNumericKb(f);
}

void onKbDone(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_READY && code != LV_EVENT_CANCEL) {
        return;
    }
    if (code == LV_EVENT_READY && editTa_ && editingField_ != Field::Count) {
        const char *txt = lv_textarea_get_text(editTa_);
        if (txt && txt[0]) {
            char *end = nullptr;
            const float v = strtof(txt, &end);
            if (end != txt) {
                applyField(editingField_, v);
            }
        }
        refreshNums();
    }
    hideNumericKb();
}

lv_obj_t *makeSeg(lv_obj_t *parent, const char *txt, lv_event_cb_t cb) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, 72, AppTheme::TOUCH_MIN_H);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    UiKit::applyPressStyle(btn, AppTheme::surfaceAlt(), AppTheme::surface(), AppTheme::gridLine(),
                           AppTheme::accent());
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, txt);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, AppTheme::text(), 0);
    lv_obj_center(lbl);
    return btn;
}

lv_obj_t *makeNumRow(lv_obj_t *parent, const char *name, Field f, lv_coord_t y) {
    const void *ud = reinterpret_cast<void *>(static_cast<uintptr_t>(f));
    const lv_coord_t rowH = AppTheme::TOUCH_MIN_H + 8;
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LCD_H_RES - 16, rowH);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y);
    UiKit::forceOpaqueBg(row, AppTheme::surface());
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, AppTheme::gridLine(), 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

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
    lv_obj_set_size(valBtn, 64, AppTheme::TOUCH_MIN_H);
    lv_obj_set_style_bg_opa(valBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(valBtn, 0, 0);
    lv_obj_set_style_bg_color(valBtn, AppTheme::surfaceAlt(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(valBtn, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(valBtn, 1, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(valBtn, AppTheme::accent(), LV_STATE_PRESSED);
    lv_obj_add_flag(valBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(valBtn, onValTap, LV_EVENT_CLICKED, const_cast<void *>(ud));
    lv_obj_align(valBtn, LV_ALIGN_RIGHT_MID, -54, 0);

    lv_obj_t *val = lv_label_create(valBtn);
    lv_label_set_text(val, "--");
    lv_obj_set_style_text_color(val, AppTheme::accent(), 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_20, 0);
    lv_obj_set_width(val, 64);
    lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(val);
    valLbls[static_cast<size_t>(f)] = val;

    lv_obj_t *plus = UiKit::makeSecondaryButton(row, "+", 44, AppTheme::TOUCH_MIN_H, onPlus);
    lv_obj_remove_event_cb(plus, onPlus);
    lv_obj_add_event_cb(plus, onPlus, LV_EVENT_CLICKED, const_cast<void *>(ud));
    lv_obj_align(plus, LV_ALIGN_RIGHT_MID, -8, 0);
    return row;
}

}  // namespace

lv_obj_t *Screens::createReservoir(lv_obj_t *parent) {
    for (size_t i = 0; i < static_cast<size_t>(Field::Count); ++i) {
        valLbls[i] = nullptr;
    }
    btnModeBatch = btnModeRecirc = nullptr;
    root_ = nullptr;
    host_ = nullptr;
    recircPanel_ = nullptr;
    recircPanelH_ = 0;
    kb_ = nullptr;
    editBar_ = nullptr;
    editTa_ = nullptr;
    editTitle_ = nullptr;
    saveBtn_ = nullptr;
    editingField_ = Field::Count;
    wizard_ = NavShell::inWizard();
    loadDraft();

    root_ = lv_obj_create(parent);
    UiKit::styleScreen(root_);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    if (wizard_) {
        lv_obj_t *hdr = lv_obj_create(root_);
        lv_obj_remove_style_all(hdr);
        lv_obj_set_size(hdr, LCD_H_RES, AppTheme::HEADER_H);
        lv_obj_align(hdr, LV_ALIGN_TOP_LEFT, 0, 0);
        UiKit::forceOpaqueBg(hdr, AppTheme::bg());
        lv_obj_t *title = lv_label_create(hdr);
        lv_label_set_text(title, Strings::tr(Msg::ReservoirControlTitle));
        lv_obj_set_style_text_color(title, AppTheme::text(), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
        lv_obj_align(title, LV_ALIGN_LEFT_MID, 12, 0);
        lv_obj_t *cont = UiKit::makePrimaryButton(hdr, Strings::tr(Msg::Continue), onContinue);
        lv_obj_set_size(cont, 100, 32);
        lv_obj_align(cont, LV_ALIGN_RIGHT_MID, -8, 0);
        lv_obj_t *sk = UiKit::makeSecondaryButton(hdr, Strings::tr(Msg::Skip), 70, 32, onSkip);
        lv_obj_align(sk, LV_ALIGN_RIGHT_MID, -116, 0);
    } else {
        UiKit::styleHeader(root_, Strings::tr(Msg::ReservoirControlTitle), onBack);
    }

    const lv_coord_t top = AppTheme::MENU_LIST_TOP;
    host_ = lv_obj_create(root_);
    lv_obj_remove_style_all(host_);
    lv_obj_set_size(host_, LCD_H_RES, 40);
    lv_obj_align(host_, LV_ALIGN_TOP_LEFT, 0, top);
    lv_obj_set_style_bg_opa(host_, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(host_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(host_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(host_, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_bottom(host_, 12, 0);
    setHostHeight(false);

    lv_coord_t y = 4;
    const lv_coord_t step = AppTheme::TOUCH_MIN_H + 12;

    lv_obj_t *hint = lv_label_create(host_);
    lv_label_set_text(hint, wizard_ ? Strings::tr(Msg::ReservoirWizardHint)
                                    : Strings::tr(Msg::ReservoirLoopHint));
    lv_obj_set_style_text_color(hint, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_width(hint, LCD_H_RES - 24);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(hint, 12, y);
    lv_obj_update_layout(hint);
    y += lv_obj_get_height(hint) + 8;

    {
        lv_obj_t *row = lv_obj_create(host_);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LCD_H_RES - 16, AppTheme::TOUCH_MIN_H + 8);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, y);
        UiKit::forceOpaqueBg(row, AppTheme::surface());
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, AppTheme::gridLine(), 0);
        btnModeBatch = makeSeg(row, Strings::tr(Msg::ReservoirModeBatch), onModeBatch);
        lv_obj_set_size(btnModeBatch, 100, AppTheme::TOUCH_MIN_H);
        lv_obj_align(btnModeBatch, LV_ALIGN_LEFT_MID, 10, 0);
        btnModeRecirc = makeSeg(row, Strings::tr(Msg::ReservoirModeRecirc), onModeRecirc);
        lv_obj_set_size(btnModeRecirc, 100, AppTheme::TOUCH_MIN_H);
        lv_obj_align(btnModeRecirc, LV_ALIGN_LEFT_MID, 120, 0);
    }
    y += step;

    makeNumRow(host_, Strings::tr(Msg::ReservoirVolumeL), Field::Volume, y);
    y += step;
    makeNumRow(host_, Strings::tr(Msg::ReservoirHomoSec), Field::Homo, y);
    y += step;
    makeNumRow(host_, Strings::tr(Msg::ReservoirDosingDelay), Field::DoseDelay, y);
    y += step;

    recircPanelH_ = 3 * step + 8;
    recircPanel_ = lv_obj_create(host_);
    lv_obj_remove_style_all(recircPanel_);
    lv_obj_set_size(recircPanel_, LCD_H_RES, recircPanelH_);
    lv_obj_set_pos(recircPanel_, 0, y);
    lv_obj_set_style_bg_opa(recircPanel_, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(recircPanel_, LV_OBJ_FLAG_SCROLLABLE);

    lv_coord_t py = 0;
    makeNumRow(recircPanel_, Strings::tr(Msg::ReservoirNutGapSec), Field::NutGap, py);
    py += step;
    makeNumRow(recircPanel_, Strings::tr(Msg::ReservoirPulseMl), Field::PulseMl, py);
    py += step;
    makeNumRow(recircPanel_, Strings::tr(Msg::ReservoirPulseGapSec), Field::PulseGap, py);

    applyModeVisibility();

    if (!wizard_) {
        saveBtn_ = UiKit::makePrimaryButton(root_, Strings::tr(Msg::NutSave), onSave);
        lv_obj_set_size(saveBtn_, LCD_H_RES - 16, AppTheme::BTN_PRIMARY_H);
        lv_obj_align(saveBtn_, LV_ALIGN_BOTTOM_MID, 0, -6);
    }

    editBar_ = lv_obj_create(root_);
    lv_obj_remove_style_all(editBar_);
    lv_obj_set_size(editBar_, LCD_H_RES, kEditBarH);
    lv_obj_align(editBar_, LV_ALIGN_BOTTOM_MID, 0, -kKbH);
    UiKit::forceOpaqueBg(editBar_, AppTheme::surface());
    lv_obj_set_style_border_width(editBar_, 1, 0);
    lv_obj_set_style_border_color(editBar_, AppTheme::gridLine(), 0);
    lv_obj_add_flag(editBar_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(editBar_, LV_OBJ_FLAG_SCROLLABLE);

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

    refreshNums();
    return root_;
}

void Screens::refreshReservoir(lv_obj_t *root) {
    (void)root;
    /* Borrador local: no pisar con store mientras la sesión está abierta. */
}
