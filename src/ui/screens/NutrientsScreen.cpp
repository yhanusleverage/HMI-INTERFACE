#include "Screens.h"
#include "NavShell.h"
#include "NutrientConfig.h"
#include "PumpConfig.h"
#include "MasterLink.h"
#include "AppStrings.h"
#include "DoseChannel.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr lv_coord_t kListTopY = AppTheme::MENU_LIST_TOP + 18;
constexpr lv_coord_t kKbH = AppTheme::KEYBOARD_H;
constexpr lv_coord_t kRowW = LCD_H_RES - 16;
constexpr lv_coord_t kPad = 4;
constexpr lv_coord_t kGap = 4;
constexpr lv_coord_t kBtnH = AppTheme::TOUCH_MIN_H;
constexpr lv_coord_t kPctW = 56;
constexpr lv_coord_t kChevronW = 28;
constexpr lv_coord_t kNameW = kRowW - 2 * kPad - 2 * kGap - kPctW - kChevronW;
constexpr lv_coord_t kRowH = kBtnH + 2 * kPad;
constexpr lv_coord_t kRowGap = 4;
constexpr lv_coord_t kRowStep = kRowH + kRowGap;

lv_obj_t *root_ = nullptr;
lv_obj_t *contentHost = nullptr;
lv_obj_t *totalLbl = nullptr;
lv_obj_t *emptyLbl = nullptr;
lv_obj_t *kb = nullptr;
lv_obj_t *activeTa = nullptr;
lv_obj_t *relayPanel = nullptr;
lv_obj_t *relayBtnLbl = nullptr;
lv_obj_t *mlTa = nullptr;
lv_obj_t *nameTa = nullptr;
lv_obj_t *pctDetailLbl = nullptr;

size_t editingIx_ = SIZE_MAX;
bool relayOpen_ = false;
bool kbIsNumber_ = false;
char draftName_[NUTRIENT_NAME_LEN] = {};
float draftMl_ = 1.0f;
uint8_t draftRelay_ = 1;

lv_obj_t *addBtn = nullptr;
lv_obj_t *pctLbls[NUTRIENT_MAX] = {};

/** Solo bombaN / pumpN — nunca concatenar con nombre de nutriente o pH. */
void relayDisplay(uint8_t relay1to6, char *buf, size_t n);

void hideKeyboard() {
    if (kb) {
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
    activeTa = nullptr;
    kbIsNumber_ = false;
}

void bringChromeForward() {
    if (addBtn) {
        lv_obj_move_foreground(addBtn);
    }
    if (kb && !lv_obj_has_flag(kb, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_move_foreground(kb);
    }
}

void setContentHeight(bool keyboardOpen) {
    if (!contentHost) {
        return;
    }
    const lv_coord_t top = AppTheme::MENU_LIST_TOP + 20;
    const lv_coord_t h = keyboardOpen ? (LCD_V_RES - top - kKbH) : (LCD_V_RES - top);
    lv_obj_set_height(contentHost, h > 40 ? h : 40);
}

void paintUi();
void refreshTotal();
void refreshEmpty();

void loadDraft(size_t ix) {
    draftName_[0] = '\0';
    draftMl_ = 1.0f;
    draftRelay_ = 1;
    if (ix >= NutrientConfig::listCount()) {
        return;
    }
    strncpy(draftName_, NutrientConfig::listName(ix), NUTRIENT_NAME_LEN - 1);
    draftName_[NUTRIENT_NAME_LEN - 1] = '\0';
    draftMl_ = NutrientConfig::listMlPerL(ix);
    draftRelay_ = NutrientConfig::listRelayNumber(ix);
}

void pullDraftFromUi() {
    if (nameTa) {
        strncpy(draftName_, lv_textarea_get_text(nameTa), NUTRIENT_NAME_LEN - 1);
        draftName_[NUTRIENT_NAME_LEN - 1] = '\0';
    }
    if (mlTa) {
        const char *txt = lv_textarea_get_text(mlTa);
        if (txt && txt[0]) {
            char *end = nullptr;
            const float v = strtof(txt, &end);
            if (end != txt) {
                draftMl_ = v;
            }
        }
    }
}

/** Etiqueta de bomba en Nutrientes / picker: alias espejado o bombaN (nunca ambos). */
void relayDisplay(uint8_t relay1to6, char *buf, size_t n) {
    if (!buf || n == 0) {
        return;
    }
    if (relay1to6 < 1) {
        relay1to6 = 1;
    }
    if (relay1to6 > PUMP_RELAY_COUNT) {
        relay1to6 = PUMP_RELAY_COUNT;
    }
    PumpConfig::formatTitle(doseFromRelayNumber(relay1to6), buf, n);
}

void refreshListPcts() {
    const size_t n = NutrientConfig::listCount();
    for (size_t i = 0; i < n; ++i) {
        if (!pctLbls[i]) {
            continue;
        }
        char pctBuf[16];
        snprintf(pctBuf, sizeof(pctBuf), "%.0f%%", NutrientConfig::listProportionPct(i));
        lv_label_set_text(pctLbls[i], pctBuf);
    }
    refreshTotal();
    refreshEmpty();
}

void onNavBack(lv_event_t *) {
    hideKeyboard();
    if (editingIx_ != SIZE_MAX) {
        editingIx_ = SIZE_MAX;
        relayOpen_ = false;
        paintUi();
        return;
    }
    NavShell::back();
}

void refreshTotal() {
    if (!totalLbl) {
        return;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), Strings::tr(Msg::NutTotalFmt), NutrientConfig::totalMlPerL());
    lv_label_set_text(totalLbl, buf);
}

void refreshEmpty() {
    if (!emptyLbl) {
        return;
    }
    if (editingIx_ != SIZE_MAX) {
        lv_obj_add_flag(emptyLbl, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (NutrientConfig::listCount() == 0) {
        lv_obj_clear_flag(emptyLbl, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(emptyLbl, Strings::tr(Msg::NutEmptyHint));
    } else {
        lv_obj_add_flag(emptyLbl, LV_OBJ_FLAG_HIDDEN);
    }
}

void applyMlFromTa() {
    /* Solo actualiza draft — persistencia en Guardar. */
    if (!mlTa) {
        return;
    }
    const char *txt = lv_textarea_get_text(mlTa);
    if (!txt || txt[0] == '\0') {
        return;
    }
    char *end = nullptr;
    float v = strtof(txt, &end);
    if (end == txt) {
        return;
    }
    if (v < 0.0f) {
        v = 0.0f;
    }
    if (v > 100.0f) {
        v = 100.0f;
    }
    draftMl_ = static_cast<float>(static_cast<int>(v * 10.0f + 0.5f)) / 10.0f;
}

void applyNameFromTa() {
    if (!nameTa) {
        return;
    }
    strncpy(draftName_, lv_textarea_get_text(nameTa), NUTRIENT_NAME_LEN - 1);
    draftName_[NUTRIENT_NAME_LEN - 1] = '\0';
}

void showKeyboard(lv_obj_t *ta, bool numberMode) {
    activeTa = ta;
    kbIsNumber_ = numberMode;
    if (!kb) {
        return;
    }
    lv_keyboard_set_mode(kb, numberMode ? LV_KEYBOARD_MODE_NUMBER : LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(kb);
    setContentHeight(true);
    lv_obj_scroll_to_view(ta, LV_ANIM_OFF);
}

void onTaFocused(lv_event_t *e) {
    lv_obj_t *ta = lv_event_get_target(e);
    const bool num = (ta == mlTa);
    showKeyboard(ta, num);
}

void onTaDefocus(lv_event_t *e) {
    lv_obj_t *ta = lv_event_get_target(e);
    if (ta == nameTa) {
        applyNameFromTa();
    } else if (ta == mlTa) {
        applyMlFromTa();
        if (pctDetailLbl && editingIx_ < NutrientConfig::listCount()) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.0f%%", NutrientConfig::listProportionPct(editingIx_));
            lv_label_set_text(pctDetailLbl, buf);
        }
        refreshTotal();
    }
}

void onKbReady(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_READY && code != LV_EVENT_CANCEL) {
        return;
    }
    if (code == LV_EVENT_READY) {
        if (activeTa == nameTa) {
            applyNameFromTa();
        } else if (activeTa == mlTa) {
            applyMlFromTa();
            if (mlTa) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%.1f", draftMl_);
                lv_textarea_set_text(mlTa, buf);
            }
        }
    }
    hideKeyboard();
    setContentHeight(false);
}

void onOpenDetail(lv_event_t *e) {
    hideKeyboard();
    editingIx_ = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
    loadDraft(editingIx_);
    relayOpen_ = false;
    MasterLink::requestSlaves();
    paintUi();
}

void onAdd(lv_event_t *) {
    hideKeyboard();
    if (!NutrientConfig::listAdd(nullptr, 1.0f)) {
        return;
    }
    editingIx_ = NutrientConfig::listCount() - 1;
    loadDraft(editingIx_);
    relayOpen_ = false;
    paintUi();
}

void onRemove(lv_event_t *) {
    hideKeyboard();
    if (editingIx_ >= NutrientConfig::listCount()) {
        return;
    }
    NutrientConfig::listRemove(editingIx_);
    editingIx_ = SIZE_MAX;
    relayOpen_ = false;
    paintUi();
}

void onSave(lv_event_t *) {
    hideKeyboard();
    if (editingIx_ >= NutrientConfig::listCount()) {
        return;
    }
    pullDraftFromUi();
    applyNameFromTa();
    applyMlFromTa();
    NutrientConfig::listSetName(editingIx_, draftName_);
    NutrientConfig::listSetMlPerL(editingIx_, draftMl_);
    NutrientConfig::listSetRelayNumber(editingIx_, draftRelay_);
    NutrientConfig::save();
    NutrientConfig::syncToMaster();
    editingIx_ = SIZE_MAX;
    relayOpen_ = false;
    paintUi();
}

void onMinusMl(lv_event_t *) {
    pullDraftFromUi();
    applyMlFromTa();
    draftMl_ -= 0.1f;
    if (draftMl_ < 0.0f) {
        draftMl_ = 0.0f;
    }
    draftMl_ = static_cast<float>(static_cast<int>(draftMl_ * 10.0f + 0.5f)) / 10.0f;
    if (mlTa) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", draftMl_);
        lv_textarea_set_text(mlTa, buf);
    }
}

void onPlusMl(lv_event_t *) {
    pullDraftFromUi();
    applyMlFromTa();
    draftMl_ += 0.1f;
    if (draftMl_ > 100.0f) {
        draftMl_ = 100.0f;
    }
    draftMl_ = static_cast<float>(static_cast<int>(draftMl_ * 10.0f + 0.5f)) / 10.0f;
    if (mlTa) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", draftMl_);
        lv_textarea_set_text(mlTa, buf);
    }
}

void onToggleRelayMenu(lv_event_t *) {
    hideKeyboard();
    pullDraftFromUi();
    setContentHeight(false);
    relayOpen_ = !relayOpen_;
    paintUi();
}

void onPickRelay(lv_event_t *e) {
    const uint8_t r = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    if (NutrientConfig::phUpRelay() == r || NutrientConfig::phDownRelay() == r) {
        return;
    }
    if (editingIx_ != SIZE_MAX && NutrientConfig::relaySharedByNutrient(r, editingIx_)) {
        return;
    }
    draftRelay_ = r;
    relayOpen_ = false;
    paintUi();
}

void paintList() {
    lv_obj_clean(contentHost);
    nameTa = nullptr;
    mlTa = nullptr;
    relayPanel = nullptr;
    relayBtnLbl = nullptr;
    pctDetailLbl = nullptr;
    for (size_t i = 0; i < NUTRIENT_MAX; ++i) {
        pctLbls[i] = nullptr;
    }
    refreshTotal();
    refreshEmpty();

    lv_coord_t y = 0;

    const size_t n = NutrientConfig::listCount();
    for (size_t i = 0; i < n; ++i) {
        const void *ud = reinterpret_cast<void *>(static_cast<uintptr_t>(i));

        /* lv_btn: hit-target estable (no rebuild en refresh). */
        lv_obj_t *row = lv_btn_create(contentHost);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, kRowW, kRowH);
        lv_obj_set_pos(row, 8, y);
        UiKit::forceOpaqueBg(row, AppTheme::surface());
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, AppTheme::gridLine(), 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        UiKit::applyPressStyle(row, AppTheme::surface(), AppTheme::surfaceAlt(), AppTheme::gridLine(),
                               AppTheme::accent());
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, onOpenDetail, LV_EVENT_CLICKED, const_cast<void *>(ud));

        lv_obj_t *name = lv_label_create(row);
        lv_label_set_text(name, NutrientConfig::listName(i));
        lv_obj_set_style_text_color(name, AppTheme::text(), 0);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_20, 0);
        lv_obj_set_width(name, kNameW - 72);
        lv_label_set_long_mode(name, LV_LABEL_LONG_CLIP);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, kPad + 4, 0);
        lv_obj_clear_flag(name, LV_OBJ_FLAG_CLICKABLE);

        char pumpBuf[12];
        dosePumpTag(NutrientConfig::listRelayNumber(i), pumpBuf, sizeof(pumpBuf));
        lv_obj_t *pumpLbl = lv_label_create(row);
        lv_label_set_text(pumpLbl, pumpBuf);
        lv_obj_set_style_text_color(pumpLbl, AppTheme::muted(), 0);
        lv_obj_set_style_text_font(pumpLbl, &lv_font_montserrat_14, 0);
        lv_obj_set_width(pumpLbl, 68);
        lv_obj_set_style_text_align(pumpLbl, LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_long_mode(pumpLbl, LV_LABEL_LONG_CLIP);
        lv_obj_align(pumpLbl, LV_ALIGN_RIGHT_MID, -(kPctW + kChevronW + kGap + 4), 0);
        lv_obj_clear_flag(pumpLbl, LV_OBJ_FLAG_CLICKABLE);
        if (NutrientConfig::relaySharedByNutrient(NutrientConfig::listRelayNumber(i), i) ||
            NutrientConfig::phUpRelay() == NutrientConfig::listRelayNumber(i) ||
            NutrientConfig::phDownRelay() == NutrientConfig::listRelayNumber(i)) {
            lv_obj_set_style_text_color(pumpLbl, AppTheme::alarm(), 0);
        }

        char pctBuf[16];
        snprintf(pctBuf, sizeof(pctBuf), "%.0f%%", NutrientConfig::listProportionPct(i));
        lv_obj_t *pct = lv_label_create(row);
        lv_label_set_text(pct, pctBuf);
        lv_obj_set_style_text_color(pct, AppTheme::accent(), 0);
        lv_obj_set_style_text_font(pct, &lv_font_montserrat_16, 0);
        lv_obj_set_width(pct, kPctW);
        lv_obj_set_style_text_align(pct, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(pct, LV_ALIGN_RIGHT_MID, -(kChevronW + kGap), 0);
        lv_obj_clear_flag(pct, LV_OBJ_FLAG_CLICKABLE);
        pctLbls[i] = pct;

        lv_obj_t *chev = lv_label_create(row);
        lv_label_set_text(chev, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(chev, AppTheme::muted(), 0);
        lv_obj_set_style_text_font(chev, &lv_font_montserrat_16, 0);
        lv_obj_align(chev, LV_ALIGN_RIGHT_MID, -kPad, 0);
        lv_obj_clear_flag(chev, LV_OBJ_FLAG_CLICKABLE);

        y += kRowStep;
    }
    lv_obj_set_style_pad_bottom(contentHost, 8, 0);
    bringChromeForward();
}

void paintDetail() {
    lv_obj_clean(contentHost);
    nameTa = nullptr;
    mlTa = nullptr;
    relayPanel = nullptr;
    relayBtnLbl = nullptr;
    pctDetailLbl = nullptr;
    refreshEmpty();

    if (editingIx_ >= NutrientConfig::listCount()) {
        editingIx_ = SIZE_MAX;
        paintList();
        return;
    }

    const size_t ix = editingIx_;
    lv_coord_t y = 0;
    const lv_coord_t step = AppTheme::TOUCH_MIN_H + 8;

    /* Nombre */
    lv_obj_t *nameRow = lv_obj_create(contentHost);
    lv_obj_remove_style_all(nameRow);
    lv_obj_set_size(nameRow, kRowW, AppTheme::TOUCH_MIN_H + 8);
    lv_obj_set_pos(nameRow, 8, y);
    UiKit::forceOpaqueBg(nameRow, AppTheme::surface());
    lv_obj_set_style_border_width(nameRow, 1, 0);
    lv_obj_set_style_border_color(nameRow, AppTheme::gridLine(), 0);
    lv_obj_clear_flag(nameRow, LV_OBJ_FLAG_SCROLLABLE);

    nameTa = lv_textarea_create(nameRow);
    lv_textarea_set_one_line(nameTa, true);
    lv_textarea_set_max_length(nameTa, NUTRIENT_NAME_LEN - 1);
    lv_textarea_set_text(nameTa, draftName_);
    lv_textarea_set_placeholder_text(nameTa, Strings::tr(Msg::PumpNameHint));
    lv_obj_set_size(nameTa, kRowW - 80, kBtnH);
    lv_obj_align(nameTa, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_set_style_bg_color(nameTa, AppTheme::surfaceAlt(), 0);
    lv_obj_set_style_bg_opa(nameTa, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(nameTa, AppTheme::text(), 0);
    lv_obj_set_style_text_font(nameTa, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(nameTa, &lv_font_montserrat_16, LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_text_color(nameTa, AppTheme::muted(), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_border_width(nameTa, 1, 0);
    lv_obj_set_style_border_color(nameTa, AppTheme::gridLine(), 0);
    lv_obj_set_style_pad_all(nameTa, 4, 0);
    lv_obj_clear_flag(nameTa, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(nameTa, onTaFocused, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(nameTa, onTaDefocus, LV_EVENT_DEFOCUSED, nullptr);

    char pctBuf[16];
    snprintf(pctBuf, sizeof(pctBuf), "%.0f%%", NutrientConfig::listProportionPct(ix));
    pctDetailLbl = lv_label_create(nameRow);
    lv_label_set_text(pctDetailLbl, pctBuf);
    lv_obj_set_style_text_color(pctDetailLbl, AppTheme::accent(), 0);
    lv_obj_set_style_text_font(pctDetailLbl, &lv_font_montserrat_16, 0);
    lv_obj_align(pctDetailLbl, LV_ALIGN_RIGHT_MID, -8, 0);
    y += step;

    /* Bomba — botón + menú; etiqueta siempre bombaN (asignación visible). */
    char rlab[40];
    relayDisplay(draftRelay_, rlab, sizeof(rlab));

    lv_obj_t *relayHdr = lv_obj_create(contentHost);
    lv_obj_remove_style_all(relayHdr);
    lv_obj_set_size(relayHdr, kRowW, AppTheme::TOUCH_MIN_H + 4);
    lv_obj_set_pos(relayHdr, 8, y);
    UiKit::forceOpaqueBg(relayHdr, AppTheme::surface());
    lv_obj_set_style_border_width(relayHdr, 1, 0);
    lv_obj_set_style_border_color(relayHdr, AppTheme::gridLine(), 0);
    lv_obj_clear_flag(relayHdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *rTitle = lv_label_create(relayHdr);
    lv_label_set_text(rTitle, Strings::tr(Msg::RulesRelay));
    lv_obj_set_style_text_color(rTitle, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(rTitle, &lv_font_montserrat_14, 0);
    lv_obj_align(rTitle, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t *rBtn = UiKit::makeSecondaryButton(relayHdr, rlab, 220, AppTheme::TOUCH_MIN_H,
                                                onToggleRelayMenu);
    lv_obj_align(rBtn, LV_ALIGN_RIGHT_MID, -8, 0);
    relayBtnLbl = lv_obj_get_child(rBtn, 0);
    if (relayBtnLbl) {
        char arrow[48];
        snprintf(arrow, sizeof(arrow), "%s %s", rlab, relayOpen_ ? LV_SYMBOL_UP : LV_SYMBOL_DOWN);
        lv_label_set_text(relayBtnLbl, arrow);
    }
    const bool draftShared = NutrientConfig::relaySharedByNutrient(draftRelay_, ix) ||
                             NutrientConfig::phUpRelay() == draftRelay_ ||
                             NutrientConfig::phDownRelay() == draftRelay_;
    if (draftShared) {
        lv_obj_set_style_border_color(rBtn, AppTheme::alarm(), 0);
        lv_obj_set_style_border_width(rBtn, 2, 0);
        if (relayBtnLbl) {
            lv_obj_set_style_text_color(relayBtnLbl, AppTheme::alarm(), 0);
        }
    }
    y += step;

    if (relayOpen_) {
        const lv_coord_t panelH = static_cast<lv_coord_t>(PUMP_RELAY_COUNT) * (AppTheme::TOUCH_MIN_H + 4);
        relayPanel = lv_obj_create(contentHost);
        lv_obj_remove_style_all(relayPanel);
        lv_obj_set_size(relayPanel, kRowW, panelH);
        lv_obj_set_pos(relayPanel, 8, y);
        UiKit::forceOpaqueBg(relayPanel, AppTheme::surfaceAlt());
        lv_obj_set_style_border_width(relayPanel, 1, 0);
        lv_obj_set_style_border_color(relayPanel, AppTheme::gridLine(), 0);
        lv_obj_clear_flag(relayPanel, LV_OBJ_FLAG_SCROLLABLE);

        lv_coord_t py = 2;
        for (uint8_t r = 1; r <= PUMP_RELAY_COUNT; ++r) {
            char lab[48];
            relayDisplay(r, lab, sizeof(lab));
            const bool sel = (draftRelay_ == r);
            const bool sharedNut = NutrientConfig::relaySharedByNutrient(r, ix);
            const bool sharedPh =
                (NutrientConfig::phUpRelay() == r || NutrientConfig::phDownRelay() == r);

            lv_obj_t *opt = UiKit::makeSecondaryButton(relayPanel, lab, kRowW - 16,
                                                       AppTheme::TOUCH_MIN_H, onPickRelay);
            lv_obj_remove_event_cb(opt, onPickRelay);
            lv_obj_set_pos(opt, 8, py);

            lv_obj_t *optLbl = lv_obj_get_child(opt, 0);
            const bool blocked = sharedPh || sharedNut;
            if (sel) {
                UiKit::applySelectionStyle(opt, true, false);
            } else if (blocked) {
                UiKit::applySelectionStyle(opt, false, true);
                lv_obj_set_style_border_color(opt, AppTheme::alarm(), 0);
                lv_obj_set_style_border_width(opt, 2, 0);
                if (optLbl) {
                    lv_obj_set_style_text_color(optLbl, AppTheme::alarm(), 0);
                }
            } else {
                UiKit::applySelectionStyle(opt, false, false);
            }
            if (!blocked || sel) {
                lv_obj_add_event_cb(opt, onPickRelay, LV_EVENT_CLICKED,
                                    reinterpret_cast<void *>(static_cast<uintptr_t>(r)));
            }
            py += AppTheme::TOUCH_MIN_H + 4;
        }
        y += panelH + 4;
    }

    /* ml/L: − | textarea numérico | + */
    lv_obj_t *mlRow = lv_obj_create(contentHost);
    lv_obj_remove_style_all(mlRow);
    lv_obj_set_size(mlRow, kRowW, AppTheme::TOUCH_MIN_H + 4);
    lv_obj_set_pos(mlRow, 8, y);
    UiKit::forceOpaqueBg(mlRow, AppTheme::surface());
    lv_obj_set_style_border_width(mlRow, 1, 0);
    lv_obj_set_style_border_color(mlRow, AppTheme::gridLine(), 0);
    lv_obj_clear_flag(mlRow, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *mlTitle = lv_label_create(mlRow);
    lv_label_set_text(mlTitle, "ml/L");
    lv_obj_set_style_text_color(mlTitle, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(mlTitle, &lv_font_montserrat_14, 0);
    lv_obj_align(mlTitle, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t *minus = UiKit::makeSecondaryButton(mlRow, "-", 40, AppTheme::TOUCH_MIN_H, onMinusMl);
    lv_obj_align(minus, LV_ALIGN_RIGHT_MID, -156, 0);

    mlTa = lv_textarea_create(mlRow);
    lv_textarea_set_one_line(mlTa, true);
    lv_textarea_set_accepted_chars(mlTa, "0123456789.");
    lv_textarea_set_max_length(mlTa, 8);
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", draftMl_);
        lv_textarea_set_text(mlTa, buf);
    }
    lv_obj_set_size(mlTa, 72, kBtnH);
    lv_obj_align(mlTa, LV_ALIGN_RIGHT_MID, -76, 0);
    lv_obj_set_style_bg_color(mlTa, AppTheme::surfaceAlt(), 0);
    lv_obj_set_style_bg_opa(mlTa, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(mlTa, AppTheme::text(), 0);
    lv_obj_set_style_text_align(mlTa, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_border_width(mlTa, 1, 0);
    lv_obj_set_style_border_color(mlTa, AppTheme::gridLine(), 0);
    lv_obj_set_style_pad_all(mlTa, 4, 0);
    lv_obj_clear_flag(mlTa, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(mlTa, onTaFocused, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(mlTa, onTaDefocus, LV_EVENT_DEFOCUSED, nullptr);

    lv_obj_t *plus = UiKit::makeSecondaryButton(mlRow, "+", 40, AppTheme::TOUCH_MIN_H, onPlusMl);
    lv_obj_align(plus, LV_ALIGN_RIGHT_MID, -8, 0);
    y += step + 8;

    lv_obj_t *actBar = lv_obj_create(contentHost);
    lv_obj_remove_style_all(actBar);
    lv_obj_set_size(actBar, kRowW, AppTheme::BTN_PRIMARY_H);
    lv_obj_set_pos(actBar, 8, y);
    lv_obj_set_style_bg_opa(actBar, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(actBar, LV_OBJ_FLAG_SCROLLABLE);

    const lv_coord_t half = (kRowW - 8) / 2;
    lv_obj_t *del = UiKit::makeCautionButton(actBar, Strings::tr(Msg::RulesDel), onRemove);
    lv_obj_set_size(del, half, AppTheme::BTN_PRIMARY_H);
    lv_obj_align(del, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *save = UiKit::makePrimaryButton(actBar, Strings::tr(Msg::NutSave), onSave);
    lv_obj_set_size(save, half, AppTheme::BTN_PRIMARY_H);
    lv_obj_align(save, LV_ALIGN_RIGHT_MID, 0, 0);

    refreshTotal();
    bringChromeForward();
}

void paintUi() {
    if (!contentHost) {
        return;
    }
    hideKeyboard();
    setContentHeight(false);
    if (editingIx_ == SIZE_MAX) {
        paintList();
    } else {
        paintDetail();
    }
}

}  // namespace

lv_obj_t *Screens::createNutrients(lv_obj_t *parent) {
    root_ = nullptr;
    contentHost = nullptr;
    totalLbl = nullptr;
    emptyLbl = nullptr;
    kb = nullptr;
    activeTa = nullptr;
    relayPanel = nullptr;
    relayBtnLbl = nullptr;
    mlTa = nullptr;
    nameTa = nullptr;
    pctDetailLbl = nullptr;
    addBtn = nullptr;
    editingIx_ = SIZE_MAX;
    relayOpen_ = false;
    for (size_t i = 0; i < NUTRIENT_MAX; ++i) {
        pctLbls[i] = nullptr;
    }

    root_ = lv_obj_create(parent);
    UiKit::styleScreen(root_);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root_, Strings::tr(Msg::DoseHubRecipe), onNavBack, 60);

    /* + grande y por encima del título / lista */
    addBtn = UiKit::makeSecondaryButton(root_, "+", 52, AppTheme::BACK_H, onAdd);
    lv_obj_align(addBtn, LV_ALIGN_TOP_RIGHT, -4, AppTheme::PAD - 4);
    lv_obj_t *addLbl = lv_obj_get_child(addBtn, 0);
    if (addLbl) {
        lv_obj_set_style_text_font(addLbl, &lv_font_montserrat_28, 0);
    }

    totalLbl = lv_label_create(root_);
    lv_obj_set_style_text_color(totalLbl, AppTheme::accent(), 0);
    lv_obj_set_style_text_font(totalLbl, &lv_font_montserrat_12, 0);
    lv_obj_align(totalLbl, LV_ALIGN_TOP_LEFT, 12, AppTheme::MENU_LIST_TOP);
    lv_obj_clear_flag(totalLbl, LV_OBJ_FLAG_CLICKABLE);

    emptyLbl = lv_label_create(root_);
    lv_obj_set_style_text_color(emptyLbl, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(emptyLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_width(emptyLbl, LCD_H_RES - 48);
    lv_label_set_long_mode(emptyLbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(emptyLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(emptyLbl, LV_ALIGN_CENTER, 0, 20);
    lv_obj_clear_flag(emptyLbl, LV_OBJ_FLAG_CLICKABLE);

    contentHost = lv_obj_create(root_);
    lv_obj_remove_style_all(contentHost);
    lv_obj_set_width(contentHost, LCD_H_RES);
    lv_obj_align(contentHost, LV_ALIGN_TOP_LEFT, 0, AppTheme::MENU_LIST_TOP + 20);
    lv_obj_set_style_bg_opa(contentHost, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(contentHost, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(contentHost, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(contentHost, LV_SCROLLBAR_MODE_AUTO);
    setContentHeight(false);

    kb = lv_keyboard_create(root_);
    lv_obj_set_size(kb, LCD_H_RES, kKbH);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    UiKit::styleDarkKeyboard(kb);
    lv_obj_add_event_cb(kb, onKbReady, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(kb, onKbReady, LV_EVENT_CANCEL, nullptr);

    paintUi();
    bringChromeForward();
    return root_;
}

void Screens::refreshNutrients(lv_obj_t *root) {
    (void)root;
    if (!contentHost || activeTa) {
        return;
    }
    /* No reconstruir lista/detalle en cada tick — eso anulaba los clics. */
    if (editingIx_ == SIZE_MAX) {
        refreshListPcts();
    } else {
        refreshTotal();
        if (pctDetailLbl && editingIx_ < NutrientConfig::listCount()) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.0f%%", NutrientConfig::listProportionPct(editingIx_));
            lv_label_set_text(pctDetailLbl, buf);
        }
    }
}
