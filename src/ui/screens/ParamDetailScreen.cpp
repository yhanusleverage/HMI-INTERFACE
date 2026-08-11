#include "Screens.h"
#include "NavShell.h"
#include "DataStore.h"
#include "MasterLink.h"
#include "NutrientConfig.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/HmiSemantics.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

#include <cstdio>
#include <cstdlib>

namespace {

ParamId detailId = ParamId::Ph;
ParamConfig draftCfg_{};
bool dirty_ = false;
bool editableSession_ = false;

lv_obj_t *root_ = nullptr;
lv_obj_t *valLbl = nullptr;
lv_obj_t *statusLbl = nullptr;
lv_obj_t *hintLbl = nullptr;
lv_obj_t *alvoValLbl = nullptr;
lv_obj_t *bandValLbl = nullptr;
lv_obj_t *roAlvoLbl = nullptr;
lv_obj_t *roBandLbl = nullptr;
lv_obj_t *editHost_ = nullptr;
lv_obj_t *kb_ = nullptr;
lv_obj_t *editBar_ = nullptr;
lv_obj_t *editTa_ = nullptr;
lv_obj_t *editTitle_ = nullptr;
lv_obj_t *saveBtn_ = nullptr;

enum class Adj : uint8_t { Alvo = 0, Band };

constexpr lv_coord_t kSaveReserve = AppTheme::BTN_PRIMARY_H + 12;

const char *paramKey(ParamId id) {
    switch (id) {
    case ParamId::Ph:
        return "ph";
    case ParamId::Ec:
        return "ec";
    case ParamId::TempAgua:
        return "temp_agua";
    case ParamId::Orp:
        return "orp";
    case ParamId::Do:
        return "do";
    default:
        return "ph";
    }
}

float stepFor(ParamId id) {
    switch (id) {
    case ParamId::Ph:
        return 0.1f;
    case ParamId::Ec:
        return 10.0f;
    case ParamId::TempAgua:
        return 0.5f;
    case ParamId::Orp:
        return 10.0f;
    case ParamId::Do:
        return 0.1f;
    default:
        return 0.1f;
    }
}

bool isIntParam(ParamId id) {
    return id == ParamId::Ec || id == ParamId::Orp;
}

bool allowsKeyboard(ParamId id) {
    return id == ParamId::Ec || id == ParamId::Ph || id == ParamId::Orp || id == ParamId::Do ||
           id == ParamId::TempAgua;
}

void formatNum(char *buf, size_t n, float v) {
    if (isIntParam(detailId)) {
        snprintf(buf, n, "%.0f", static_cast<double>(v));
    } else {
        snprintf(buf, n, "%.1f", static_cast<double>(v));
    }
}

float halfBand(const ParamConfig &cfg) {
    float h = (cfg.high - cfg.low) * 0.5f;
    if (h < 0.0f) {
        h = 0.0f;
    }
    return h;
}

void applyBandAroundSp(ParamConfig &cfg, float half) {
    const float minHalf = stepFor(detailId);
    if (half < minHalf) {
        half = minHalf;
    }
    cfg.low = cfg.setpoint - half;
    cfg.high = cfg.setpoint + half;
}

void syncEcPhBandsToNutrient(const ParamConfig &cfg) {
    if (detailId == ParamId::Ec) {
        NutrientConfig::setEcLow(cfg.low);
        NutrientConfig::setEcHigh(cfg.high);
    } else if (detailId == ParamId::Ph) {
        NutrientConfig::setPhLow(cfg.low);
        NutrientConfig::setPhHigh(cfg.high);
    }
}

lv_coord_t hostHeightNormal() {
    lv_coord_t h = LCD_V_RES - AppTheme::MENU_LIST_TOP - 8;
    if (editableSession_) {
        h -= kSaveReserve;
    }
    return h > 40 ? h : 40;
}

lv_coord_t hostHeightWithKb() {
    /* Save oculto con teclado: no restar kSaveReserve otra vez. */
    lv_coord_t h =
        LCD_V_RES - AppTheme::MENU_LIST_TOP - AppTheme::KEYBOARD_H - 40 - 8;
    return h > 40 ? h : 40;
}

void hideKb() {
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
    if (editHost_) {
        lv_obj_set_height(editHost_, hostHeightNormal());
    }
}

void loadDraft() {
    draftCfg_ = DataStore::instance().config(detailId);
    dirty_ = false;
}

void refreshAdjLabels() {
    const ParamConfig &cfg = editableSession_ ? draftCfg_ : DataStore::instance().config(detailId);
    char buf[32];
    if (alvoValLbl) {
        formatNum(buf, sizeof(buf), cfg.setpoint);
        lv_label_set_text(alvoValLbl, buf);
    }
    if (bandValLbl) {
        formatNum(buf, sizeof(buf), halfBand(cfg));
        lv_label_set_text(bandValLbl, buf);
    }
    if (roAlvoLbl) {
        char line[48];
        formatNum(buf, sizeof(buf), cfg.setpoint);
        snprintf(line, sizeof(line), "%s %s", Strings::tr(Msg::SpLabel), buf);
        lv_label_set_text(roAlvoLbl, line);
    }
    if (roBandLbl) {
        char half[16];
        formatNum(half, sizeof(half), halfBand(cfg));
        char line[64];
        snprintf(line, sizeof(line), "%s ±%s", Strings::tr(Msg::DeadbandLabel), half);
        lv_label_set_text(roBandLbl, line);
    }
}

void commitDraft() {
    if (!editableSession_) {
        return;
    }
    DataStore &store = DataStore::instance();
    store.setConfig(detailId, draftCfg_);
    MasterLink::sendSetpoint(paramKey(detailId), draftCfg_.setpoint);
    /* Bandas Alvo EC/pH viajan en loop_control (consumo 24h / deadband Master). */
    if (detailId == ParamId::Ec || detailId == ParamId::Ph) {
        MasterLink::sendLoopControl();
    }
    syncEcPhBandsToNutrient(draftCfg_);
    dirty_ = false;
}

void onBack(lv_event_t *) {
    hideKb();
    /* Atrás descarta el borrador (no persiste). */
    NavShell::back();
}

void onSave(lv_event_t *) {
    hideKb();
    commitDraft();
    NavShell::back();
}

void bumpAlvo(float delta) {
    if (!editableSession_) {
        return;
    }
    const float half = halfBand(draftCfg_);
    draftCfg_.setpoint += delta;
    applyBandAroundSp(draftCfg_, half);
    dirty_ = true;
    refreshAdjLabels();
}

void bumpBand(float delta) {
    if (!editableSession_) {
        return;
    }
    applyBandAroundSp(draftCfg_, halfBand(draftCfg_) + delta);
    dirty_ = true;
    refreshAdjLabels();
}

void setAlvoAbsolute(float v) {
    if (!editableSession_) {
        return;
    }
    const float half = halfBand(draftCfg_);
    draftCfg_.setpoint = v;
    applyBandAroundSp(draftCfg_, half);
    dirty_ = true;
    refreshAdjLabels();
}

void onAdjMinus(lv_event_t *e) {
    const Adj a = static_cast<Adj>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    const float s = stepFor(detailId);
    if (a == Adj::Alvo) {
        bumpAlvo(-s);
    } else {
        bumpBand(-s);
    }
}

void onAdjPlus(lv_event_t *e) {
    const Adj a = static_cast<Adj>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    const float s = stepFor(detailId);
    if (a == Adj::Alvo) {
        bumpAlvo(s);
    } else {
        bumpBand(s);
    }
}

void onKbDone(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY && editTa_) {
        const char *txt = lv_textarea_get_text(editTa_);
        if (txt && txt[0]) {
            char *end = nullptr;
            const float v = strtof(txt, &end);
            if (end != txt) {
                setAlvoAbsolute(v);
            }
        }
    }
    hideKb();
}

void onOpenAlvoKb(lv_event_t *) {
    if (!allowsKeyboard(detailId) || !kb_ || !editBar_ || !editTa_) {
        return;
    }
    char buf[32];
    formatNum(buf, sizeof(buf), draftCfg_.setpoint);
    lv_textarea_set_text(editTa_, buf);
    if (editTitle_) {
        lv_label_set_text(editTitle_, Strings::tr(Msg::SpLabel));
    }
    lv_keyboard_set_textarea(kb_, editTa_);
    lv_obj_clear_flag(editBar_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    if (saveBtn_) {
        lv_obj_add_flag(saveBtn_, LV_OBJ_FLAG_HIDDEN);
    }
    if (editHost_) {
        lv_obj_set_height(editHost_, hostHeightWithKb());
    }
    lv_obj_move_foreground(editBar_);
    lv_obj_move_foreground(kb_);
}

lv_coord_t makeAdjRow(lv_obj_t *parent, lv_coord_t y, const char *name, Adj field, lv_obj_t **valOut,
                      bool tapValueOpensKb) {
    const void *ud = reinterpret_cast<void *>(static_cast<uintptr_t>(field));
    const lv_coord_t rowH = AppTheme::TOUCH_MIN_H + 4;
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
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_width(lbl, 110);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t *minus =
        UiKit::makeSecondaryButton(row, "-", 44, AppTheme::TOUCH_MIN_H, onAdjMinus);
    lv_obj_remove_event_cb(minus, onAdjMinus);
    lv_obj_add_event_cb(minus, onAdjMinus, LV_EVENT_CLICKED, const_cast<void *>(ud));
    lv_obj_align(minus, LV_ALIGN_RIGHT_MID, -118, 0);

    lv_obj_t *valBtn = lv_btn_create(row);
    lv_obj_remove_style_all(valBtn);
    lv_obj_set_size(valBtn, 72, AppTheme::TOUCH_MIN_H);
    lv_obj_align(valBtn, LV_ALIGN_RIGHT_MID, -54, 0);
    lv_obj_set_style_bg_opa(valBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(valBtn, 0, 0);
    lv_obj_set_style_bg_color(valBtn, AppTheme::surfaceAlt(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(valBtn, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(valBtn, 1, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(valBtn, AppTheme::accent(), LV_STATE_PRESSED);
    if (tapValueOpensKb) {
        lv_obj_add_flag(valBtn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(valBtn, onOpenAlvoKb, LV_EVENT_CLICKED, nullptr);
    } else {
        lv_obj_clear_flag(valBtn, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_t *val = lv_label_create(valBtn);
    lv_label_set_text(val, "--");
    lv_obj_set_style_text_color(val, AppTheme::accent(), 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_20, 0);
    lv_obj_set_width(val, 72);
    lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(val);
    *valOut = val;

    lv_obj_t *plus = UiKit::makeSecondaryButton(row, "+", 44, AppTheme::TOUCH_MIN_H, onAdjPlus);
    lv_obj_remove_event_cb(plus, onAdjPlus);
    lv_obj_add_event_cb(plus, onAdjPlus, LV_EVENT_CLICKED, const_cast<void *>(ud));
    lv_obj_align(plus, LV_ALIGN_RIGHT_MID, -8, 0);

    return y + rowH + 4;
}

}  // namespace

lv_obj_t *Screens::createParamDetail(lv_obj_t *parent, ParamId id) {
    detailId = id;
    root_ = nullptr;
    valLbl = nullptr;
    statusLbl = nullptr;
    hintLbl = nullptr;
    alvoValLbl = bandValLbl = nullptr;
    roAlvoLbl = roBandLbl = nullptr;
    editHost_ = nullptr;
    kb_ = nullptr;
    editBar_ = nullptr;
    editTa_ = nullptr;
    editTitle_ = nullptr;
    saveBtn_ = nullptr;
    dirty_ = false;

    DataStore &store = DataStore::instance();
    editableSession_ = NavShell::paramEditable();
    if (editableSession_) {
        loadDraft();
    }

    root_ = lv_obj_create(parent);
    UiKit::styleScreen(root_);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root_, store.name(id), onBack);

    editHost_ = lv_obj_create(root_);
    lv_obj_remove_style_all(editHost_);
    lv_obj_set_size(editHost_, LCD_H_RES, hostHeightNormal());
    lv_obj_set_pos(editHost_, 0, AppTheme::MENU_LIST_TOP);
    lv_obj_set_style_bg_opa(editHost_, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(editHost_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(editHost_, LV_DIR_VER);

    valLbl = lv_label_create(editHost_);
    lv_label_set_text(valLbl, "--");
    lv_obj_set_style_text_color(valLbl, AppTheme::text(), 0);
    lv_obj_set_style_text_font(valLbl, &lv_font_montserrat_28, 0);
    lv_obj_align(valLbl, LV_ALIGN_TOP_MID, 0, 0);

    statusLbl = lv_label_create(editHost_);
    lv_label_set_text(statusLbl, Strings::tr(Msg::StatusOk));
    lv_obj_set_style_text_color(statusLbl, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(statusLbl, &lv_font_montserrat_14, 0);
    lv_obj_align(statusLbl, LV_ALIGN_TOP_MID, 0, 32);

    lv_coord_t y = 56;
    if (editableSession_) {
        y = makeAdjRow(editHost_, y, Strings::tr(Msg::SpLabel), Adj::Alvo, &alvoValLbl,
                       allowsKeyboard(detailId));
        y = makeAdjRow(editHost_, y, Strings::tr(Msg::DeadbandLabel), Adj::Band, &bandValLbl,
                       false);

        saveBtn_ = UiKit::makePrimaryButton(root_, Strings::tr(Msg::NutSave), onSave);
        lv_obj_set_size(saveBtn_, LCD_H_RES - 16, AppTheme::BTN_PRIMARY_H);
        lv_obj_align(saveBtn_, LV_ALIGN_BOTTOM_MID, 0, -6);

        /* Teclado solo para Alvo (tap en valor). */
        editBar_ = lv_obj_create(root_);
        lv_obj_remove_style_all(editBar_);
        lv_obj_set_size(editBar_, LCD_H_RES, 40);
        lv_obj_align(editBar_, LV_ALIGN_BOTTOM_MID, 0, -AppTheme::KEYBOARD_H);
        UiKit::forceOpaqueBg(editBar_, AppTheme::surface());
        lv_obj_set_style_border_width(editBar_, 1, 0);
        lv_obj_set_style_border_color(editBar_, AppTheme::gridLine(), 0);
        lv_obj_add_flag(editBar_, LV_OBJ_FLAG_HIDDEN);

        editTitle_ = lv_label_create(editBar_);
        lv_label_set_text(editTitle_, Strings::tr(Msg::SpLabel));
        lv_obj_set_style_text_color(editTitle_, AppTheme::muted(), 0);
        lv_obj_set_style_text_font(editTitle_, &lv_font_montserrat_14, 0);
        lv_obj_align(editTitle_, LV_ALIGN_LEFT_MID, 8, 0);

        editTa_ = lv_textarea_create(editBar_);
        lv_textarea_set_one_line(editTa_, true);
        lv_textarea_set_accepted_chars(editTa_, "0123456789.");
        lv_textarea_set_max_length(editTa_, 12);
        lv_obj_set_size(editTa_, 160, 32);
        lv_obj_align(editTa_, LV_ALIGN_RIGHT_MID, -8, 0);
        lv_obj_set_style_bg_color(editTa_, AppTheme::surfaceAlt(), 0);
        lv_obj_set_style_bg_opa(editTa_, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(editTa_, AppTheme::text(), 0);
        lv_obj_set_style_border_width(editTa_, 1, 0);
        lv_obj_set_style_border_color(editTa_, AppTheme::gridLine(), 0);

        kb_ = lv_keyboard_create(root_);
        lv_obj_set_size(kb_, LCD_H_RES, AppTheme::KEYBOARD_H);
        lv_obj_align(kb_, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);
        UiKit::attachNumericKeyboard(kb_, editTa_, onKbDone);
    } else {
        roAlvoLbl = lv_label_create(editHost_);
        lv_obj_set_style_text_color(roAlvoLbl, AppTheme::muted(), 0);
        lv_obj_set_style_text_font(roAlvoLbl, &lv_font_montserrat_14, 0);
        lv_obj_align(roAlvoLbl, LV_ALIGN_TOP_LEFT, 24, y);

        roBandLbl = lv_label_create(editHost_);
        lv_obj_set_style_text_color(roBandLbl, AppTheme::muted(), 0);
        lv_obj_set_style_text_font(roBandLbl, &lv_font_montserrat_14, 0);
        lv_obj_align(roBandLbl, LV_ALIGN_TOP_RIGHT, -24, y);
        y += 28;

        hintLbl = lv_label_create(editHost_);
        lv_label_set_text(hintLbl, Strings::tr(Msg::ReadOnlyHint));
        UiKit::styleHint(hintLbl);
        lv_obj_set_width(hintLbl, LCD_H_RES - 32);
        lv_label_set_long_mode(hintLbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(hintLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(hintLbl, LV_ALIGN_TOP_MID, 0, y);
    }

    /* Calibracion solo en Sensors → Calibrate (no en Niveles). */

    refreshAdjLabels();
    return root_;
}

void Screens::refreshParamDetail(lv_obj_t *root) {
    (void)root;
    DataStore &store = DataStore::instance();
    const ParamStatus st = store.status(detailId);
    char buf[48];
    const float v = store.value(detailId);
    if (isIntParam(detailId)) {
        snprintf(buf, sizeof(buf), "%.0f %s", static_cast<double>(v), store.unit(detailId));
    } else {
        snprintf(buf, sizeof(buf), "%.1f %s", static_cast<double>(v), store.unit(detailId));
    }
    if (valLbl) {
        lv_label_set_text(valLbl, buf);
    }

    if (statusLbl) {
        lv_label_set_text(statusLbl, HmiSemantics::statusText(st));
        lv_obj_set_style_text_color(statusLbl, HmiSemantics::statusLabel(st), 0);
    }

    /* Editable: Alvo/Banda vienen del borrador; no pisar con store. */
    if (!editableSession_) {
        refreshAdjLabels();
    }
}
