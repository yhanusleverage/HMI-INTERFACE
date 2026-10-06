#include "Screens.h"
#include "NavShell.h"
#include "RelayAliasConfig.h"
#include "SlaveInventory.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

#include <cstdio>
#include <cstring>

/**
 * Hub Reglas — solo dibujo.
 * Lista en RAM de la pantalla. Los nombres de valvula salen del alias Atlas si existe.
 * No NVS de reglas, no motor, no UART.
 */

namespace {

constexpr size_t kMaxDemo = 6;
constexpr uint8_t kLevelCount = 4;
constexpr size_t kNameLen = 20;

const char *const kLevelBtn[kLevelCount] = {"Vacio", "Bajo", "Medio", "Alto"};
const char *const kLevelWord[kLevelCount] = {"vacio", "bajo", "medio", "alto"};

struct DemoRule {
    bool used;
    bool two;
    uint8_t level;
    uint8_t level2;
    uint8_t r1;
    uint8_t r2;
    bool startOn;
    bool endOn;
    bool startOn2;
    bool endOn2;
    char name[kNameLen];
    char phase[16];
};

DemoRule rules_[kMaxDemo];
bool seeded_ = false;
bool editing_ = false;
int editIx_ = -1;

bool draftTwo_ = false;
uint8_t draftLevel_ = 0;
uint8_t draftLevel2_ = 3;
uint8_t draftR1_ = 1;
uint8_t draftR2_ = 2;
bool draftStartOn_ = true;
bool draftEndOn_ = false;
bool draftStartOn2_ = true;
bool draftEndOn2_ = false;

lv_obj_t *titleLbl_ = nullptr;
lv_obj_t *plusBtn_ = nullptr;
lv_obj_t *levelLine_ = nullptr;
lv_obj_t *listHost_ = nullptr;
lv_obj_t *editHost_ = nullptr;
lv_obj_t *nameTa_ = nullptr;
lv_obj_t *kb_ = nullptr;
lv_obj_t *errLbl_ = nullptr;
lv_obj_t *saveBtn_ = nullptr;
lv_obj_t *secondBlock_ = nullptr;
lv_obj_t *addSecondBtn_ = nullptr;
lv_coord_t ySecond_ = 0;
lv_coord_t secondH_ = 0;
lv_obj_t *root_ = nullptr;
lv_obj_t *menuPanel_ = nullptr;
lv_obj_t *levelFieldBtn_[2] = {};
lv_obj_t *startFieldBtn_[2] = {};
lv_obj_t *endFieldBtn_[2] = {};
lv_obj_t *levelFieldLbl_[2] = {};
lv_obj_t *startFieldLbl_[2] = {};
lv_obj_t *endFieldLbl_[2] = {};
lv_obj_t *levelChip_[2][kLevelCount] = {};

enum class OpenMenu : uint8_t {
    None = 0,
    Level1,
    Start1,
    End1,
    Level2,
    Start2,
    End2
};
OpenMenu openMenu_ = OpenMenu::None;

void seedOnce() {
    if (seeded_) {
        return;
    }
    seeded_ = true;
    memset(rules_, 0, sizeof(rules_));
    rules_[0].used = true;
    rules_[0].two = true;
    rules_[0].level = 2;
    rules_[0].level2 = 3;
    rules_[0].r1 = 1;
    rules_[0].r2 = 2;
    rules_[0].startOn = true;
    rules_[0].endOn = false;
    rules_[0].startOn2 = true;
    rules_[0].endOn2 = false;
    strncpy(rules_[0].name, "partial fill", sizeof(rules_[0].name) - 1);
    strncpy(rules_[0].phase, "Listo", sizeof(rules_[0].phase) - 1);
    rules_[1].used = true;
    rules_[1].two = false;
    rules_[1].level = 0;
    rules_[1].r1 = 1;
    rules_[1].startOn = true;
    rules_[1].endOn = false;
    strncpy(rules_[1].name, "Dreno", sizeof(rules_[1].name) - 1);
    strncpy(rules_[1].phase, "Drenando", sizeof(rules_[1].phase) - 1);
}

const char *nivelWord(uint8_t level) {
    if (level >= kLevelCount) {
        return kLevelWord[0];
    }
    return kLevelWord[level];
}

void valveCaption(uint8_t relay1to8, char *buf, size_t n) {
    if (!buf || n == 0) {
        return;
    }
    buf[0] = '\0';
    if (relay1to8 < 1 || relay1to8 > 8) {
        return;
    }
    const uint8_t idx = static_cast<uint8_t>(relay1to8 - 1);
    char mac[SlaveInventory::kMacLen] = {};
    const size_t ix = SlaveInventory::firstEspNowIndex();
    if (ix != SIZE_MAX) {
        const SlaveInventory::Target *t = SlaveInventory::at(ix);
        if (t && t->mac[0]) {
            strncpy(mac, t->mac, sizeof(mac) - 1);
        }
    }
    const char *alias = mac[0] ? RelayAliasConfig::getName(mac, idx) : nullptr;
    if (!alias || alias[0] == '\0') {
        alias = RelayAliasConfig::getName(RelayAliasConfig::kPlaceholderMac, idx);
    }
    if (alias && alias[0] != '\0') {
        snprintf(buf, n, "%s", alias);
        return;
    }
    RelayAliasConfig::placeholder(buf, n, idx);
}

bool phaseRunning(const char *phase) {
    return phase && (strcmp(phase, "Drenando") == 0 || strcmp(phase, "Llenando") == 0);
}

void statePhrase(uint8_t relay, bool startOn, bool endOn, uint8_t level, char *buf, size_t n) {
    char name[24];
    valveCaption(relay, name, sizeof(name));
    const char *arrive = endOn ? "abre" : "cierra";
    if (startOn && !endOn) {
        snprintf(buf, n, "%s abierto hasta %s, al llegar cierra", name, nivelWord(level));
    } else if (!startOn && endOn) {
        snprintf(buf, n, "%s cerrado hasta %s, al llegar abre", name, nivelWord(level));
    } else {
        snprintf(buf, n, "%s %s hasta %s, al llegar %s", name, startOn ? "abierto" : "cerrado",
                 nivelWord(level), arrive);
    }
}

void formatSummary(const DemoRule &r, char *buf, size_t n) {
    char a[72];
    statePhrase(r.r1 ? r.r1 : 1, r.startOn, r.endOn, r.level, a, sizeof(a));
    if (!r.two) {
        snprintf(buf, n, "%s", a);
        return;
    }
    char b[72];
    statePhrase(r.r2 ? r.r2 : 1, r.startOn2, r.endOn2, r.level2, b, sizeof(b));
    snprintf(buf, n, "%s; luego %s", a, b);
}

void paintChoice(lv_obj_t *btn, bool on) {
    if (!btn) {
        return;
    }
    UiKit::applySelectionStyle(btn, on, false);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
}

void setFieldText(lv_obj_t *lbl, const char *text) {
    if (lbl) {
        lv_label_set_text(lbl, text ? text : "");
    }
}

void refreshFields() {
    char name[24];
    valveCaption(draftR1_ ? draftR1_ : 1, name, sizeof(name));
    setFieldText(startFieldLbl_[0], name);
    setFieldText(endFieldLbl_[0], draftEndOn_ ? "abre" : "cierra");
    valveCaption(draftR2_ ? draftR2_ : 2, name, sizeof(name));
    setFieldText(startFieldLbl_[1], name);
    setFieldText(endFieldLbl_[1], draftEndOn2_ ? "abre" : "cierra");
    const uint8_t sel[2] = {draftLevel_, draftLevel2_};
    for (uint8_t step = 0; step < 2; ++step) {
        for (uint8_t i = 0; i < kLevelCount; ++i) {
            paintChoice(levelChip_[step][i], i == sel[step]);
        }
        paintChoice(endFieldBtn_[step], true);
    }
}

void closeMenu() {
    openMenu_ = OpenMenu::None;
    if (menuPanel_) {
        lv_obj_add_flag(menuPanel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clean(menuPanel_);
    }
}

void clearErr() {
    if (!errLbl_) {
        return;
    }
    lv_label_set_text(errLbl_, "");
    lv_obj_add_flag(errLbl_, LV_OBJ_FLAG_HIDDEN);
}

void showErr(const char *msg) {
    if (!errLbl_) {
        return;
    }
    lv_label_set_text(errLbl_, msg);
    lv_obj_clear_flag(errLbl_, LV_OBJ_FLAG_HIDDEN);
}

void hideKb() {
    if (kb_) {
        lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    }
    if (nameTa_) {
        lv_obj_clear_state(nameTa_, LV_STATE_FOCUSED);
    }
}

void showList();
void showEdit(int ix);

void onBack(lv_event_t *) {
    closeMenu();
    hideKb();
    if (editing_) {
        showList();
        return;
    }
    NavShell::back();
}

void onRow(lv_event_t *e) {
    const int ix = static_cast<int>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    showEdit(ix);
}

void onNew(lv_event_t *) { showEdit(-1); }

void onPickLevel(lv_event_t *e) {
    const uint8_t level =
        static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    const bool second = openMenu_ == OpenMenu::Level2;
    if (second) {
        draftLevel2_ = level < kLevelCount ? level : 0;
    } else {
        draftLevel_ = level < kLevelCount ? level : 0;
    }
    refreshFields();
    closeMenu();
    clearErr();
}

void onPickState(lv_event_t *e) {
    const uintptr_t code = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
    const OpenMenu which = openMenu_;
    if (which == OpenMenu::End1 || which == OpenMenu::End2) {
        const bool on = code != 0;
        if (which == OpenMenu::End2) {
            draftEndOn2_ = on;
        } else {
            draftEndOn_ = on;
        }
    } else {
        const uint8_t relay = static_cast<uint8_t>(code);
        const uint8_t r = relay >= 1 && relay <= 8 ? relay : 1;
        if (which == OpenMenu::Start2) {
            draftR2_ = r;
            draftStartOn2_ = true;
        } else {
            draftR1_ = r;
            draftStartOn_ = true;
        }
    }
    refreshFields();
    closeMenu();
    clearErr();
}

void onIgnore(lv_event_t *) {}

void onNameFocus(lv_event_t *) {
    closeMenu();
    if (!kb_ || !nameTa_) {
        return;
    }
    lv_keyboard_set_textarea(kb_, nameTa_);
    lv_obj_clear_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(kb_);
}

void onKbDone(lv_event_t *) { hideKb(); }

void placeSave() {
    const lv_coord_t y = draftTwo_ ? (ySecond_ + secondH_ + 8) : (ySecond_ + 8);
    if (errLbl_) {
        lv_obj_set_pos(errLbl_, 12, y);
    }
    if (saveBtn_) {
        lv_obj_set_pos(saveBtn_, 20, y + 28);
    }
}

void applySecondVisible() {
    if (secondBlock_) {
        if (draftTwo_) {
            lv_obj_clear_flag(secondBlock_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(secondBlock_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (addSecondBtn_) {
        if (draftTwo_) {
            lv_obj_add_flag(addSecondBtn_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(addSecondBtn_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    placeSave();
}

void onAddSecond(lv_event_t *) {
    closeMenu();
    draftTwo_ = true;
    applySecondVisible();
    refreshFields();
    clearErr();
}

void onDropSecond(lv_event_t *) {
    closeMenu();
    draftTwo_ = false;
    applySecondVisible();
    clearErr();
}

void copyName(char *dst, size_t n, const char *src) {
    if (!dst || n == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    while (*src == ' ') {
        ++src;
    }
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
    size_t len = strlen(dst);
    while (len > 0 && dst[len - 1] == ' ') {
        dst[--len] = '\0';
    }
}

void onSave(lv_event_t *) {
    char name[kNameLen];
    copyName(name, sizeof(name), nameTa_ ? lv_textarea_get_text(nameTa_) : "");
    if (name[0] == '\0') {
        showErr("Escribe el nombre de la regla.");
        return;
    }
    int ix = editIx_;
    if (ix < 0) {
        ix = -1;
        for (size_t i = 0; i < kMaxDemo; ++i) {
            if (!rules_[i].used) {
                ix = static_cast<int>(i);
                break;
            }
        }
        if (ix < 0) {
            showErr("La lista ya tiene 6 reglas.");
            return;
        }
        strncpy(rules_[ix].phase, "Listo", sizeof(rules_[ix].phase) - 1);
    }
    rules_[ix].used = true;
    rules_[ix].two = draftTwo_;
    rules_[ix].level = draftLevel_;
    rules_[ix].level2 = draftLevel2_;
    rules_[ix].r1 = draftR1_;
    rules_[ix].r2 = draftTwo_ ? draftR2_ : 0;
    rules_[ix].startOn = true;
    rules_[ix].endOn = draftEndOn_;
    rules_[ix].startOn2 = true;
    rules_[ix].endOn2 = draftEndOn2_;
    strncpy(rules_[ix].name, name, sizeof(rules_[ix].name) - 1);
    rules_[ix].name[sizeof(rules_[ix].name) - 1] = '\0';
    if (!rules_[ix].phase[0]) {
        strncpy(rules_[ix].phase, "Listo", sizeof(rules_[ix].phase) - 1);
    }
    hideKb();
    showList();
}

lv_obj_t *makeRuleRow(lv_obj_t *parent, const DemoRule &r, void *userData) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_width(btn, LCD_H_RES - 24);
    lv_obj_set_height(btn, AppTheme::HUB_ROW_H);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    UiKit::applyPressStyle(btn, AppTheme::surface(), AppTheme::surfaceAlt(), AppTheme::gridLine(),
                           AppTheme::accent());
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, onRow, LV_EVENT_CLICKED, userData);

    lv_obj_t *t = lv_label_create(btn);
    lv_label_set_text(t, r.name[0] ? r.name : "Regla");
    lv_obj_set_style_text_color(t, AppTheme::text(), 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_set_width(t, LCD_H_RES - 24 - 120);
    lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 14, 8);

    lv_obj_t *ph = lv_label_create(btn);
    lv_label_set_text(ph, r.phase);
    lv_obj_set_style_text_color(ph, phaseRunning(r.phase) ? AppTheme::accentText() : AppTheme::muted(),
                                0);
    lv_obj_set_style_text_font(ph, &lv_font_montserrat_14, 0);
    lv_obj_align(ph, LV_ALIGN_TOP_RIGHT, -12, 12);

    char summary[140];
    formatSummary(r, summary, sizeof(summary));
    lv_obj_t *h = lv_label_create(btn);
    lv_label_set_text(h, summary);
    lv_obj_set_style_text_color(h, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(h, &lv_font_montserrat_16, 0);
    lv_obj_set_width(h, LCD_H_RES - 24 - 28);
    lv_label_set_long_mode(h, LV_LABEL_LONG_DOT);
    lv_obj_align(h, LV_ALIGN_BOTTOM_LEFT, 14, -8);
    return btn;
}

void buildList() {
    if (!listHost_) {
        return;
    }
    lv_obj_clean(listHost_);
    lv_coord_t y = 0;
    const lv_coord_t step = AppTheme::HUB_ROW_H + 4;
    bool any = false;
    for (size_t i = 0; i < kMaxDemo; ++i) {
        if (!rules_[i].used) {
            continue;
        }
        any = true;
        lv_obj_t *row = makeRuleRow(listHost_, rules_[i],
                                    reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
        lv_obj_set_pos(row, 12, y);
        y += step;
    }
    if (!any) {
        lv_obj_t *empty = lv_label_create(listHost_);
        lv_label_set_text(empty, "No hay reglas. Pulsa + y ponle un nombre.");
        lv_obj_set_style_text_color(empty, AppTheme::muted(), 0);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_16, 0);
        lv_obj_set_pos(empty, 12, 8);
    }
}

void showList() {
    editing_ = false;
    closeMenu();
    hideKb();
    if (titleLbl_) {
        lv_label_set_text(titleLbl_, "Reglas");
    }
    if (plusBtn_) {
        lv_obj_clear_flag(plusBtn_, LV_OBJ_FLAG_HIDDEN);
    }
    if (levelLine_) {
        lv_obj_clear_flag(levelLine_, LV_OBJ_FLAG_HIDDEN);
    }
    if (listHost_) {
        lv_obj_clear_flag(listHost_, LV_OBJ_FLAG_HIDDEN);
    }
    if (editHost_) {
        lv_obj_add_flag(editHost_, LV_OBJ_FLAG_HIDDEN);
    }
    buildList();
}

void showEdit(int ix) {
    editing_ = true;
    editIx_ = ix;
    clearErr();
    hideKb();
    if (ix >= 0 && rules_[ix].used) {
        draftTwo_ = rules_[ix].two;
        draftLevel_ = rules_[ix].level < kLevelCount ? rules_[ix].level : 0;
        draftLevel2_ = rules_[ix].level2 < kLevelCount ? rules_[ix].level2 : 3;
        draftR1_ = rules_[ix].r1 ? rules_[ix].r1 : 1;
        draftR2_ = rules_[ix].r2 ? rules_[ix].r2 : 2;
        draftStartOn_ = rules_[ix].startOn;
        draftEndOn_ = rules_[ix].endOn;
        draftStartOn2_ = rules_[ix].startOn2;
        draftEndOn2_ = rules_[ix].endOn2;
        if (nameTa_) {
            lv_textarea_set_text(nameTa_, rules_[ix].name);
        }
        if (titleLbl_) {
            lv_label_set_text(titleLbl_, "Editar");
        }
    } else {
        draftTwo_ = false;
        draftLevel_ = 0;
        draftLevel2_ = 3;
        draftR1_ = 1;
        draftR2_ = 2;
        draftStartOn_ = true;
        draftEndOn_ = false;
        draftStartOn2_ = true;
        draftEndOn2_ = false;
        if (nameTa_) {
            lv_textarea_set_text(nameTa_, "");
        }
        if (titleLbl_) {
            lv_label_set_text(titleLbl_, "Nueva regla");
        }
    }
    refreshFields();
    applySecondVisible();
    if (plusBtn_) {
        lv_obj_add_flag(plusBtn_, LV_OBJ_FLAG_HIDDEN);
    }
    if (levelLine_) {
        lv_obj_add_flag(levelLine_, LV_OBJ_FLAG_HIDDEN);
    }
    if (listHost_) {
        lv_obj_add_flag(listHost_, LV_OBJ_FLAG_HIDDEN);
    }
    if (editHost_) {
        lv_obj_clear_flag(editHost_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_scroll_to_y(editHost_, 0, LV_ANIM_OFF);
    }
}

void clipBtnLabel(lv_obj_t *btn, lv_coord_t w) {
    if (!btn || lv_obj_get_child_cnt(btn) == 0) {
        return;
    }
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    if (!lbl) {
        return;
    }
    lv_obj_set_width(lbl, w > 12 ? w - 12 : w);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);
}

void fillMenu(OpenMenu kind, uint8_t selectedRelay, bool selectedOn) {
    if (!menuPanel_) {
        return;
    }
    lv_obj_clean(menuPanel_);
    const bool levels = kind == OpenMenu::Level1 || kind == OpenMenu::Level2;
    const bool ending = kind == OpenMenu::End1 || kind == OpenMenu::End2;
    const lv_coord_t rowH = AppTheme::TOUCH_MIN_H;
    const lv_coord_t rowGap = 4;
    const lv_coord_t innerW = lv_obj_get_width(menuPanel_) - 8;
    const uint8_t count = levels ? kLevelCount : (ending ? 2 : 8);
    for (uint8_t i = 0; i < count; ++i) {
        char lab[40];
        uintptr_t code = i;
        bool selected = false;
        if (levels) {
            snprintf(lab, sizeof(lab), "%s", kLevelBtn[i]);
            selected = selectedRelay == i;
        } else if (ending) {
            const bool on = i == 0;
            snprintf(lab, sizeof(lab), "%s", on ? "abre" : "cierra");
            code = on ? 1u : 0u;
            selected = on == selectedOn;
        } else {
            const uint8_t relay = static_cast<uint8_t>(i + 1);
            valveCaption(relay, lab, sizeof(lab));
            code = relay;
            selected = relay == selectedRelay;
        }
        lv_obj_t *btn = UiKit::makeSecondaryButton(menuPanel_, lab, innerW > 40 ? innerW : 40, rowH,
                                                   onIgnore);
        lv_obj_set_pos(btn, 4, 4 + static_cast<lv_coord_t>(i) * (rowH + rowGap));
        lv_obj_add_event_cb(btn, levels ? onPickLevel : onPickState, LV_EVENT_CLICKED,
                            reinterpret_cast<void *>(code));
        clipBtnLabel(btn, innerW > 40 ? innerW : 40);
        paintChoice(btn, selected);
    }
}

void openMenu(OpenMenu kind) {
    if (!menuPanel_ || !root_) {
        return;
    }
    if (openMenu_ == kind) {
        closeMenu();
        return;
    }
    hideKb();
    lv_obj_t *anchor = nullptr;
    bool levels = false;
    uint8_t selectedRelay = 0;
    bool selectedOn = true;
    switch (kind) {
    case OpenMenu::Level1:
        anchor = levelFieldBtn_[0];
        levels = true;
        selectedRelay = draftLevel_;
        break;
    case OpenMenu::Start1:
        anchor = startFieldBtn_[0];
        selectedRelay = draftR1_;
        selectedOn = draftStartOn_;
        break;
    case OpenMenu::End1:
        anchor = endFieldBtn_[0];
        selectedRelay = draftR1_;
        selectedOn = draftEndOn_;
        break;
    case OpenMenu::Level2:
        anchor = levelFieldBtn_[1];
        levels = true;
        selectedRelay = draftLevel2_;
        break;
    case OpenMenu::Start2:
        anchor = startFieldBtn_[1];
        selectedRelay = draftR2_ ? draftR2_ : 2;
        selectedOn = draftStartOn2_;
        break;
    case OpenMenu::End2:
        anchor = endFieldBtn_[1];
        selectedRelay = draftR2_ ? draftR2_ : 2;
        selectedOn = draftEndOn2_;
        break;
    case OpenMenu::None:
        closeMenu();
        return;
    }
    if (!anchor) {
        return;
    }
    openMenu_ = kind;
    const bool ending = kind == OpenMenu::End1 || kind == OpenMenu::End2;
    const lv_coord_t menuW = ending ? 160 : (levels ? 180 : 280);
    const lv_coord_t rowH = AppTheme::TOUCH_MIN_H;
    const uint8_t visible = ending ? 2 : 4;
    const lv_coord_t menuH = 8 + visible * (rowH + 4);
    lv_obj_set_size(menuPanel_, menuW, menuH);
    fillMenu(kind, selectedRelay, selectedOn);

    lv_area_t fa;
    lv_area_t ra;
    lv_obj_get_coords(anchor, &fa);
    lv_obj_get_coords(root_, &ra);
    lv_coord_t x = fa.x1 - ra.x1;
    lv_coord_t y = fa.y2 - ra.y1 + 2;
    if (x + menuW > LCD_H_RES - 8) {
        x = LCD_H_RES - 8 - menuW;
    }
    if (x < 8) {
        x = 8;
    }
    if (y + menuH > LCD_V_RES - 8) {
        y = fa.y1 - ra.y1 - menuH - 2;
    }
    if (y < AppTheme::MENU_LIST_TOP) {
        y = AppTheme::MENU_LIST_TOP;
    }
    lv_obj_set_pos(menuPanel_, x, y);
    lv_obj_clear_flag(menuPanel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(menuPanel_);
    uint8_t selIx = 0;
    if (levels) {
        selIx = selectedRelay;
    } else if (ending) {
        selIx = selectedOn ? 0 : 1;
    } else {
        selIx = selectedRelay >= 1 ? static_cast<uint8_t>(selectedRelay - 1) : 0;
    }
    if (selIx >= visible) {
        lv_obj_scroll_to_y(menuPanel_, static_cast<lv_coord_t>(selIx) * (rowH + 4), LV_ANIM_OFF);
    } else {
        lv_obj_scroll_to_y(menuPanel_, 0, LV_ANIM_OFF);
    }
}

void onField(lv_event_t *e) {
    const auto kind = static_cast<OpenMenu>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    openMenu(kind);
}

void onLevelChip(lv_event_t *e) {
    const uintptr_t code = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
    const uint8_t step = static_cast<uint8_t>((code >> 4) & 1u);
    const uint8_t level = static_cast<uint8_t>(code & 0x0Fu);
    if (step) {
        draftLevel2_ = level < kLevelCount ? level : 0;
    } else {
        draftLevel_ = level < kLevelCount ? level : 0;
    }
    refreshFields();
    closeMenu();
    clearErr();
}

void onToggleEnd(lv_event_t *e) {
    const uint8_t step =
        static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    if (step) {
        draftEndOn2_ = !draftEndOn2_;
    } else {
        draftEndOn_ = !draftEndOn_;
    }
    refreshFields();
    closeMenu();
    clearErr();
}

void makePick(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, OpenMenu kind,
              lv_obj_t **btnOut, lv_obj_t **lblOut) {
    lv_obj_t *btn = UiKit::makeSecondaryButton(parent, " ", w, AppTheme::TOUCH_MIN_H, onIgnore);
    lv_obj_set_pos(btn, x, y);
    lv_obj_add_event_cb(btn, onField, LV_EVENT_CLICKED,
                        reinterpret_cast<void *>(static_cast<uintptr_t>(kind)));
    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    if (lbl) {
        lv_obj_set_width(lbl, w > 22 ? w - 22 : w);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 6, 0);
    }
    lv_obj_t *mark = lv_label_create(btn);
    lv_label_set_text(mark, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_color(mark, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(mark, &lv_font_montserrat_14, 0);
    lv_obj_align(mark, LV_ALIGN_RIGHT_MID, -4, 0);
    if (btnOut) {
        *btnOut = btn;
    }
    if (lblOut) {
        *lblOut = lbl;
    }
}

void placeWord(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y) {
    lv_obj_t *lab = lv_label_create(parent);
    lv_label_set_text(lab, text);
    lv_obj_set_style_text_color(lab, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(lab, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lab, x, y + 14);
}

void paintStepBadge(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, uint8_t step) {
    const bool first = step == 0;
    lv_obj_t *badge = lv_obj_create(parent);
    lv_obj_remove_style_all(badge);
    lv_obj_set_size(badge, 28, 28);
    lv_obj_set_pos(badge, x, y + 8);
    lv_obj_set_style_radius(badge, 14, 0);
    lv_obj_set_style_bg_color(badge, first ? AppTheme::accent() : AppTheme::surfaceAlt(), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *num = lv_label_create(badge);
    lv_label_set_text(num, first ? "1" : "2");
    lv_obj_set_style_text_color(num, first ? AppTheme::bg() : AppTheme::text(), 0);
    lv_obj_set_style_text_font(num, &lv_font_montserrat_16, 0);
    lv_obj_center(num);
}

lv_coord_t placeStepFields(lv_obj_t *parent, lv_coord_t y, uint8_t step) {
    const bool second = step != 0;
    const lv_coord_t gap = 4;
    const lv_coord_t left = 8;
    const lv_coord_t h = AppTheme::TOUCH_MIN_H;
    const OpenMenu valveKind = second ? OpenMenu::Start2 : OpenMenu::Start1;
    paintStepBadge(parent, left, y, step);
    placeWord(parent, "Abrir", left + 28 + gap, y);
    lv_coord_t x = left + 28 + gap + 44 + gap;
    const lv_coord_t valveW = LCD_H_RES - 8 - x;
    makePick(parent, x, y, valveW > 80 ? valveW : 80, valveKind, &startFieldBtn_[step],
             &startFieldLbl_[step]);

    const lv_coord_t y2 = y + h + 6;
    placeWord(parent, "hasta", left, y2);
    x = left + 46;
    const lv_coord_t luegoW = 46;
    const lv_coord_t endW = 78;
    const lv_coord_t chipsSpan = LCD_H_RES - 8 - x - gap - luegoW - gap - endW;
    const lv_coord_t chipW = (chipsSpan - 3 * gap) / kLevelCount;
    for (uint8_t i = 0; i < kLevelCount; ++i) {
        lv_obj_t *chip = UiKit::makeSecondaryButton(parent, kLevelBtn[i], chipW > 48 ? chipW : 48, h,
                                                    onIgnore);
        lv_obj_set_pos(chip, x, y2);
        lv_obj_add_event_cb(chip, onLevelChip, LV_EVENT_CLICKED,
                            reinterpret_cast<void *>(static_cast<uintptr_t>((step << 4) | i)));
        levelChip_[step][i] = chip;
        x += (chipW > 48 ? chipW : 48) + gap;
    }
    placeWord(parent, "luego", x, y2);
    x += luegoW + gap;
    lv_obj_t *endBtn = UiKit::makeSecondaryButton(parent, "cierra", endW, h, onIgnore);
    lv_obj_set_pos(endBtn, x, y2);
    lv_obj_add_event_cb(endBtn, onToggleEnd, LV_EVENT_CLICKED,
                        reinterpret_cast<void *>(static_cast<uintptr_t>(step)));
    endFieldBtn_[step] = endBtn;
    endFieldLbl_[step] = lv_obj_get_child(endBtn, 0);
    return h + 6 + h;
}

}  // namespace

lv_obj_t *Screens::createRulesHub(lv_obj_t *parent) {
    seedOnce();
    editing_ = false;

    lv_obj_t *root = lv_obj_create(parent);
    root_ = root;
    UiKit::styleScreen(root);
    titleLbl_ = UiKit::styleHeader(root, "Reglas", onBack, 64);

    plusBtn_ = lv_btn_create(root);
    lv_obj_remove_style_all(plusBtn_);
    lv_obj_set_size(plusBtn_, 48, AppTheme::BACK_H);
    lv_obj_align(plusBtn_, LV_ALIGN_TOP_RIGHT, -(AppTheme::PAD - 4), AppTheme::PAD - 4);
    lv_obj_set_style_radius(plusBtn_, 0, 0);
    lv_obj_set_style_border_width(plusBtn_, 0, 0);
    lv_obj_set_style_shadow_width(plusBtn_, 0, 0);
    UiKit::applyPressStyle(plusBtn_, AppTheme::accent(), AppTheme::accentPressed(), AppTheme::accent(),
                           AppTheme::accentPressed());
    lv_obj_add_event_cb(plusBtn_, onNew, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *plusLbl = lv_label_create(plusBtn_);
    lv_label_set_text(plusLbl, "+");
    lv_obj_set_style_text_color(plusLbl, AppTheme::bg(), 0);
    lv_obj_set_style_text_font(plusLbl, &lv_font_montserrat_20, 0);
    lv_obj_center(plusLbl);

    levelLine_ = lv_obj_create(root);
    lv_obj_remove_style_all(levelLine_);
    lv_obj_set_size(levelLine_, LCD_H_RES, 28);
    lv_obj_align(levelLine_, LV_ALIGN_TOP_LEFT, 0, AppTheme::MENU_LIST_TOP);
    lv_obj_clear_flag(levelLine_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *lvlA = lv_label_create(levelLine_);
    lv_label_set_text(lvlA, "Nivel ahora");
    lv_obj_set_style_text_color(lvlA, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(lvlA, &lv_font_montserrat_16, 0);
    lv_obj_align(lvlA, LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_t *lvlB = lv_label_create(levelLine_);
    lv_label_set_text(lvlB, "medio");
    lv_obj_set_style_text_color(lvlB, AppTheme::accentText(), 0);
    lv_obj_set_style_text_font(lvlB, &lv_font_montserrat_16, 0);
    lv_obj_align_to(lvlB, lvlA, LV_ALIGN_OUT_RIGHT_MID, 8, 0);

    const lv_coord_t listTop = AppTheme::MENU_LIST_TOP + 28;
    listHost_ = lv_obj_create(root);
    lv_obj_remove_style_all(listHost_);
    lv_obj_set_size(listHost_, LCD_H_RES, LCD_V_RES - listTop);
    lv_obj_align(listHost_, LV_ALIGN_TOP_LEFT, 0, listTop);
    lv_obj_set_style_bg_opa(listHost_, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(listHost_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(listHost_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(listHost_, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_bottom(listHost_, 8, 0);

    editHost_ = lv_obj_create(root);
    lv_obj_remove_style_all(editHost_);
    lv_obj_set_size(editHost_, LCD_H_RES, LCD_V_RES - AppTheme::MENU_LIST_TOP);
    lv_obj_align(editHost_, LV_ALIGN_TOP_LEFT, 0, AppTheme::MENU_LIST_TOP);
    lv_obj_set_style_bg_opa(editHost_, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(editHost_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(editHost_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(editHost_, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_bottom(editHost_, 12, 0);
    lv_obj_add_flag(editHost_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(editHost_, [](lv_event_t *) { closeMenu(); }, LV_EVENT_SCROLL, nullptr);

    lv_coord_t y = 4;
    lv_obj_t *nameLbl = lv_label_create(editHost_);
    lv_label_set_text(nameLbl, "Nombre");
    lv_obj_set_style_text_color(nameLbl, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(nameLbl, 12, y);
    y += 20;

    nameTa_ = lv_textarea_create(editHost_);
    lv_textarea_set_one_line(nameTa_, true);
    lv_textarea_set_max_length(nameTa_, kNameLen - 1);
    lv_textarea_set_placeholder_text(nameTa_, "Dreno");
    lv_obj_set_size(nameTa_, LCD_H_RES - 24, AppTheme::TOUCH_MIN_H + 4);
    lv_obj_set_pos(nameTa_, 12, y);
    lv_obj_set_style_bg_color(nameTa_, AppTheme::surface(), 0);
    lv_obj_set_style_bg_opa(nameTa_, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(nameTa_, AppTheme::text(), 0);
    lv_obj_set_style_text_font(nameTa_, &lv_font_montserrat_20, 0);
    lv_obj_set_style_pad_all(nameTa_, 6, 0);
    lv_obj_set_style_border_color(nameTa_, AppTheme::gridLine(), 0);
    lv_obj_set_style_border_width(nameTa_, 1, 0);
    lv_obj_set_style_radius(nameTa_, 0, 0);
    lv_obj_add_event_cb(nameTa_, onNameFocus, LV_EVENT_FOCUSED, nullptr);
    y += AppTheme::TOUCH_MIN_H + 12;

    y += placeStepFields(editHost_, y, 0);
    y += 8;

    addSecondBtn_ = UiKit::makeSecondaryButton(editHost_, "Anadir segundo paso", LCD_H_RES - 24,
                                               AppTheme::TOUCH_MIN_H, onAddSecond);
    lv_obj_set_pos(addSecondBtn_, 12, y);
    y += AppTheme::TOUCH_MIN_H + 8;
    ySecond_ = y;

    secondBlock_ = lv_obj_create(editHost_);
    lv_obj_remove_style_all(secondBlock_);
    lv_obj_set_width(secondBlock_, LCD_H_RES);
    lv_obj_set_pos(secondBlock_, 0, y);
    lv_obj_clear_flag(secondBlock_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(secondBlock_, LV_OBJ_FLAG_HIDDEN);

    lv_coord_t sy = placeStepFields(secondBlock_, 0, 1);
    sy += 8;
    lv_obj_t *drop = UiKit::makeSecondaryButton(secondBlock_, "Quitar segundo paso", LCD_H_RES - 24,
                                                AppTheme::TOUCH_MIN_H, onDropSecond);
    lv_obj_set_pos(drop, 12, sy);
    sy += AppTheme::TOUCH_MIN_H + 4;
    secondH_ = sy;
    lv_obj_set_height(secondBlock_, secondH_);

    errLbl_ = lv_label_create(editHost_);
    lv_label_set_text(errLbl_, "");
    lv_obj_set_width(errLbl_, LCD_H_RES - 24);
    lv_label_set_long_mode(errLbl_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(errLbl_, AppTheme::alarm(), 0);
    lv_obj_set_style_text_font(errLbl_, &lv_font_montserrat_14, 0);
    lv_obj_add_flag(errLbl_, LV_OBJ_FLAG_HIDDEN);

    saveBtn_ = UiKit::makePrimaryButton(editHost_, "Guardar", onSave);
    placeSave();

    kb_ = lv_keyboard_create(root);
    UiKit::styleDarkKeyboard(kb_);
    lv_obj_add_event_cb(kb_, onKbDone, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(kb_, onKbDone, LV_EVENT_CANCEL, nullptr);
    lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);

    menuPanel_ = lv_obj_create(root);
    lv_obj_remove_style_all(menuPanel_);
    lv_obj_set_size(menuPanel_, 220, 180);
    UiKit::forceOpaqueBg(menuPanel_, AppTheme::surface());
    lv_obj_set_style_border_width(menuPanel_, 1, 0);
    lv_obj_set_style_border_color(menuPanel_, AppTheme::gridLine(), 0);
    lv_obj_set_style_radius(menuPanel_, 0, 0);
    lv_obj_add_flag(menuPanel_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(menuPanel_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(menuPanel_, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_flag(menuPanel_, LV_OBJ_FLAG_HIDDEN);

    showList();
    return root;
}

void Screens::refreshRulesHub(lv_obj_t *root) { (void)root; }
