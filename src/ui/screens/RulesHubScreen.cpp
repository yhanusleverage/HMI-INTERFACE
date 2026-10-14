#include "Screens.h"
#include "NavShell.h"
#include "MasterLink.h"
#include "SlaveInventory.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

#include <Arduino.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/**
 * Hub Reglas — lista del Master.
 * El editor es el de la referencia. El nombre del relé sale de Supabase.
 */

namespace {

enum class Mode : uint8_t { List, Explain, Edit };

constexpr uint8_t kLevelCount = 5;
constexpr size_t kNameLen = 20;

const char *const kUntil[kLevelCount] = {"vazio", "baixo", "medio", "medio_alto", "alto"};
const char *const kLevelBtn[kLevelCount] = {"Vacio", "Bajo", "Medio", "M.alto", "Alto"};
const char *const kLevelWord[kLevelCount] = {"vacio", "bajo", "medio", "medio alto", "alto"};

enum class OpenMenu : uint8_t {
    None = 0,
    Level1,
    Start1,
    End1,
    Level2,
    Start2,
    End2
};

Mode mode_ = Mode::List;
int explainIx_ = -1;
int editIx_ = -1;
bool deleteArmed_ = false;
enum class ConfirmAsk : uint8_t { None, Enable, Disable, Delete };
ConfirmAsk confirmAsk_ = ConfirmAsk::None;
uint32_t seenRule_ = 0;
uint32_t seenName_ = 0;
uint32_t lastRulesPollMs_ = 0;
constexpr uint32_t kRulesPollMs = 2000;

bool draftTwo_ = false;
uint8_t draftLevel_ = 0;
uint8_t draftLevel2_ = 4;
uint8_t draftR1_ = 1;
uint8_t draftR2_ = 2;
bool draftEndOn_ = false;
bool draftEndOn2_ = false;
char draftMac_[18] = {};

lv_obj_t *root_ = nullptr;
lv_obj_t *titleLbl_ = nullptr;
lv_obj_t *plusBtn_ = nullptr;
lv_obj_t *gearBtn_ = nullptr;
lv_obj_t *listHost_ = nullptr;
lv_obj_t *explainHost_ = nullptr;
lv_obj_t *editHost_ = nullptr;
lv_obj_t *nameTa_ = nullptr;
lv_obj_t *kb_ = nullptr;
lv_obj_t *errLbl_ = nullptr;
lv_obj_t *saveBtn_ = nullptr;
lv_obj_t *secondBlock_ = nullptr;
lv_obj_t *addSecondBtn_ = nullptr;
lv_obj_t *deleteBtn_ = nullptr;
lv_obj_t *confirmHost_ = nullptr;
lv_obj_t *confirmTitle_ = nullptr;
lv_obj_t *confirmName_ = nullptr;
lv_coord_t ySecond_ = 0;
lv_coord_t secondH_ = 0;
lv_obj_t *menuPanel_ = nullptr;
lv_obj_t *startFieldBtn_[2] = {};
lv_obj_t *endFieldBtn_[2] = {};
lv_obj_t *startFieldLbl_[2] = {};
lv_obj_t *endFieldLbl_[2] = {};
lv_obj_t *levelChip_[2][kLevelCount] = {};
OpenMenu openMenu_ = OpenMenu::None;

void showList();
void showExplain(int ix);
void showEdit(int ix);
uint8_t waitCmd(const char *action, uint32_t timeoutMs);

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

void atlasMac(char *dst, size_t n) {
    if (!dst || n == 0) {
        return;
    }
    dst[0] = '\0';
    if (MasterLink::relayCatalogCount() > 0) {
        const char *mac = MasterLink::relayCatalogMac(0);
        if (mac && mac[0]) {
            strncpy(dst, mac, n - 1);
            dst[n - 1] = '\0';
            return;
        }
    }
    const size_t ix = SlaveInventory::firstEspNowIndex();
    if (ix == SIZE_MAX) {
        return;
    }
    const SlaveInventory::Target *t = SlaveInventory::at(ix);
    if (t && t->mac[0]) {
        strncpy(dst, t->mac, n - 1);
        dst[n - 1] = '\0';
    }
}

void valveCaption(uint8_t relay1to8, char *buf, size_t n) {
    if (!buf || n == 0) {
        return;
    }
    buf[0] = '\0';
    if (relay1to8 < 1 || relay1to8 > 8) {
        snprintf(buf, n, "Relé");
        return;
    }
    const uint8_t idx = static_cast<uint8_t>(relay1to8 - 1);
    const char *web = MasterLink::relayDisplayName(draftMac_, idx);
    if (web && web[0]) {
        snprintf(buf, n, "%s", web);
        return;
    }
    snprintf(buf, n, "Relé %u", relay1to8);
}

const char *wordForUntil(const char *token) {
    if (!token || !token[0] || strcmp(token, "-") == 0) {
        return "";
    }
    for (uint8_t i = 0; i < kLevelCount; ++i) {
        if (strcmp(token, kUntil[i]) == 0) {
            return kLevelWord[i];
        }
    }
    return "";
}

int untilIndex(const char *token) {
    if (!token) {
        return 0;
    }
    for (uint8_t i = 0; i < kLevelCount; ++i) {
        if (strcmp(token, kUntil[i]) == 0) {
            return i;
        }
    }
    return 0;
}

void stepBareName(const char *step, char *dst, size_t n) {
    if (!dst || n == 0) {
        return;
    }
    dst[0] = '\0';
    if (!step) {
        return;
    }
    strncpy(dst, step, n - 1);
    dst[n - 1] = '\0';
    if (char *cut = strstr(dst, " ON")) {
        *cut = '\0';
    }
}

void levelTokenAt(const char *level, int ix, char *dst, size_t n) {
    if (!dst || n == 0) {
        return;
    }
    dst[0] = '\0';
    if (!level) {
        return;
    }
    int cur = 0;
    const char *p = level;
    while (*p) {
        const char *plus = strchr(p, '+');
        const size_t len = plus ? static_cast<size_t>(plus - p) : strlen(p);
        if (cur == ix) {
            const size_t copy = len < n - 1 ? len : n - 1;
            memcpy(dst, p, copy);
            dst[copy] = '\0';
            return;
        }
        if (!plus) {
            return;
        }
        p = plus + 1;
        ++cur;
    }
}

int relayOf(const MasterLink::RuleView *rule, int step) {
    if (!rule || step < 0 || step >= 4) {
        return 0;
    }
    if (rule->relays[step] >= 0 && rule->relays[step] <= 7) {
        return rule->relays[step];
    }
    char bare[48];
    stepBareName(rule->steps[step], bare, sizeof(bare));
    if (strncmp(bare, "Relé ", 5) == 0) {
        const int n = atoi(bare + 5);
        if (n >= 1 && n <= 8) {
            return n - 1;
        }
    }
    const char *mac = rule->macs[step][0] ? rule->macs[step] : draftMac_;
    for (uint8_t r = 0; r < 8; ++r) {
        const char *web = MasterLink::relayDisplayName(mac, r);
        if (web && web[0] && strcmp(web, bare) == 0) {
            return r;
        }
    }
    return 0;
}

void explainLine(lv_obj_t *parent, const char *text, bool arrow) {
    lv_obj_t *lb = lv_label_create(parent);
    lv_label_set_text(lb, text);
    lv_obj_set_width(lb, LCD_H_RES - 48);
    lv_label_set_long_mode(lb, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(lb, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lb, arrow ? AppTheme::muted() : AppTheme::text(), 0);
    lv_obj_set_style_text_align(lb, arrow ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT, 0);
}

void paintExplainSteps(lv_obj_t *parent, const MasterLink::RuleView *rule) {
    if (!parent) {
        return;
    }
    if (!rule || rule->stepCount == 0) {
        explainLine(parent, "Sin pasos de nivel.", false);
        return;
    }
    bool any = false;
    for (uint8_t s = 0; s < rule->stepCount && s < 4; ++s) {
        char bare[48];
        stepBareName(rule->steps[s], bare, sizeof(bare));
        if (rule->relays[s] >= 0 && rule->relays[s] <= 7) {
            const char *mac = rule->macs[s][0] ? rule->macs[s] : nullptr;
            const char *web =
                MasterLink::relayDisplayName(mac, static_cast<uint8_t>(rule->relays[s]));
            if (web && web[0]) {
                strncpy(bare, web, sizeof(bare) - 1);
                bare[sizeof(bare) - 1] = '\0';
            }
        }
        char token[16];
        if (rule->untils[s][0]) {
            strncpy(token, rule->untils[s], sizeof(token) - 1);
            token[sizeof(token) - 1] = '\0';
        } else {
            levelTokenAt(rule->level, rule->stepCount > 1 ? s : 0, token, sizeof(token));
        }
        const char *hasta = wordForUntil(token);
        char piece[96];
        if (bare[0] && hasta[0]) {
            snprintf(piece, sizeof(piece), "%s abierto hasta %s", bare, hasta);
        } else if (bare[0]) {
            snprintf(piece, sizeof(piece), "%s encendido", bare);
        } else {
            continue;
        }
        if (any) {
            explainLine(parent, LV_SYMBOL_DOWN, true);
        }
        explainLine(parent, piece, false);
        any = true;
        if (hasta[0]) {
            explainLine(parent, LV_SYMBOL_DOWN, true);
            explainLine(parent, "al llegar cierra", false);
        }
    }
    if (!any) {
        explainLine(parent, "Sin pasos de nivel.", false);
    }
}

void closeMenu() {
    openMenu_ = OpenMenu::None;
    if (menuPanel_) {
        lv_obj_add_flag(menuPanel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clean(menuPanel_);
    }
}

void hideKb() {
    if (kb_) {
        lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    }
    if (nameTa_) {
        lv_obj_clear_state(nameTa_, LV_STATE_FOCUSED);
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
    lv_label_set_text(errLbl_, msg ? msg : "");
    if (msg && msg[0]) {
        lv_obj_clear_flag(errLbl_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(errLbl_, LV_OBJ_FLAG_HIDDEN);
    }
}

void setFieldText(lv_obj_t *lbl, const char *text) {
    if (lbl) {
        lv_label_set_text(lbl, text ? text : "");
    }
}

void paintChoice(lv_obj_t *btn, bool on) {
    if (!btn) {
        return;
    }
    UiKit::applySelectionStyle(btn, on, false);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
}

void refreshFields() {
    char name[40];
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
    }
}

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

void onBack(lv_event_t *) {
    closeMenu();
    hideKb();
    if (mode_ == Mode::Edit || mode_ == Mode::Explain) {
        showList();
        return;
    }
    NavShell::back();
}

void onRow(lv_event_t *e) {
    if (!lv_event_get_user_data(e)) {
        return;
    }
    const int ix = static_cast<int>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e))) - 1;
    showExplain(ix);
}

void onNew(lv_event_t *) { showEdit(-1); }

void onGear(lv_event_t *) {
    if (explainIx_ < 0) {
        return;
    }
    showEdit(explainIx_);
}

void onToggle(lv_event_t *) {
    const MasterLink::RuleView *rule = MasterLink::ruleViewAt(static_cast<uint8_t>(explainIx_));
    if (!rule || !confirmHost_ || !confirmTitle_ || !confirmName_) {
        return;
    }
    confirmAsk_ = rule->enabled ? ConfirmAsk::Disable : ConfirmAsk::Enable;
    lv_label_set_text(confirmTitle_,
                      rule->enabled ? "Desactivar esta regla?" : "Activar esta regla?");
    lv_label_set_text(confirmName_, rule->name);
    lv_obj_clear_flag(confirmHost_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(confirmHost_);
}

void commitToggle(bool next) {
    const MasterLink::RuleView *rule = MasterLink::ruleViewAt(static_cast<uint8_t>(explainIx_));
    if (!rule) {
        showList();
        return;
    }
    char id[48];
    strncpy(id, rule->id, sizeof(id) - 1);
    id[sizeof(id) - 1] = '\0';
    MasterLink::clearLastCmdAck();
    MasterLink::sendRuleSet(id, next);
    if (waitCmd("rule_set", 2500) != 1) {
        return;
    }
    MasterLink::setRuleViewEnabled(static_cast<uint8_t>(explainIx_), next);
    showExplain(explainIx_);
    MasterLink::requestRules();
}

void onDelete(lv_event_t *) {
    const MasterLink::RuleView *rule = MasterLink::ruleViewAt(static_cast<uint8_t>(explainIx_));
    if (!rule || !confirmHost_ || !confirmTitle_ || !confirmName_) {
        return;
    }
    confirmAsk_ = ConfirmAsk::Delete;
    lv_label_set_text(confirmTitle_, "Eliminar esta regla?");
    lv_label_set_text(confirmName_, rule->name);
    lv_obj_clear_flag(confirmHost_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(confirmHost_);
}

void commitDelete() {
    const MasterLink::RuleView *rule = MasterLink::ruleViewAt(static_cast<uint8_t>(explainIx_));
    if (!rule) {
        showList();
        return;
    }
    MasterLink::sendRuleDelete(rule->id);
    MasterLink::dropRuleView(static_cast<uint8_t>(explainIx_));
    showList();
}

void onConfirmNo(lv_event_t *) {
    confirmAsk_ = ConfirmAsk::None;
    if (confirmHost_) {
        lv_obj_add_flag(confirmHost_, LV_OBJ_FLAG_HIDDEN);
    }
}

void onConfirmYes(lv_event_t *) {
    const ConfirmAsk ask = confirmAsk_;
    onConfirmNo(nullptr);
    if (ask == ConfirmAsk::Delete) {
        commitDelete();
    } else if (ask == ConfirmAsk::Enable || ask == ConfirmAsk::Disable) {
        commitToggle(ask == ConfirmAsk::Enable);
    }
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

void onPickLevel(lv_event_t *e) {
    const uintptr_t raw = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
    if (raw == 0) {
        return;
    }
    const uint8_t level = static_cast<uint8_t>(raw - 1);
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
        const bool on = code == 2;
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
        } else {
            draftR1_ = r;
        }
    }
    refreshFields();
    closeMenu();
    clearErr();
}

void onLevelChip(lv_event_t *e) {
    const uintptr_t code = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
    if (code == 0) {
        return;
    }
    const uint8_t step = static_cast<uint8_t>(((code >> 4) & 0x0f) - 1);
    const uint8_t level = static_cast<uint8_t>((code & 0x0f) - 1);
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
    const uintptr_t raw = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
    if (raw == 0) {
        return;
    }
    const uint8_t step = static_cast<uint8_t>(raw - 1);
    if (step) {
        draftEndOn2_ = !draftEndOn2_;
    } else {
        draftEndOn_ = !draftEndOn_;
    }
    refreshFields();
    closeMenu();
    clearErr();
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

uint8_t waitCmd(const char *action, uint32_t timeoutMs) {
    const uint32_t t0 = millis();
    while (static_cast<uint32_t>(millis() - t0) < timeoutMs) {
        MasterLink::loop();
        if (MasterLink::lastCmdAckState() != 0 &&
            strcmp(MasterLink::lastCmdAction(), action) == 0) {
            return MasterLink::lastCmdAckState();
        }
        delay(20);
    }
    return 0;
}

void onSave(lv_event_t *) {
    char name[kNameLen];
    copyName(name, sizeof(name), nameTa_ ? lv_textarea_get_text(nameTa_) : "");
    if (name[0] == '\0') {
        showErr("Escribe el nombre de la regla.");
        return;
    }
    if (!draftMac_[0]) {
        atlasMac(draftMac_, sizeof(draftMac_));
    }
    if (!draftMac_[0]) {
        showErr("Sin Atlas.");
        return;
    }
    const char *keepId = nullptr;
    if (editIx_ >= 0) {
        const MasterLink::RuleView *rule = MasterLink::ruleViewAt(static_cast<uint8_t>(editIx_));
        if (rule && rule->id[0]) {
            keepId = rule->id;
        }
    }
    const int relay2 = draftTwo_ ? static_cast<int>(draftR2_ - 1) : -1;
    const char *until2 = draftTwo_ ? kUntil[draftLevel2_ < kLevelCount ? draftLevel2_ : 0] : nullptr;
    MasterLink::clearLastCmdAck();
    MasterLink::sendRuleCreate(name, draftMac_, static_cast<uint8_t>(draftR1_ - 1),
                               kUntil[draftLevel_ < kLevelCount ? draftLevel_ : 0], relay2, until2,
                               keepId);
    const uint8_t ack = waitCmd("rule_create", 2500);
    hideKb();
    closeMenu();
    if (ack != 1) {
        showErr(ack == 2 ? "El Master rechazo la regla." : "Sin respuesta del Master.");
        return;
    }
    showList();
    MasterLink::requestRules();
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

void fillMenu(OpenMenu kind, uint8_t selectedRelay, bool selectedOn);
void openMenu(OpenMenu kind);

void onField(lv_event_t *e) {
    const auto kind = static_cast<OpenMenu>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    openMenu(kind);
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
        uintptr_t code = i + 1;
        bool selected = false;
        if (levels) {
            snprintf(lab, sizeof(lab), "%s", kLevelBtn[i]);
            selected = selectedRelay == i;
            code = i + 1;
        } else if (ending) {
            const bool on = i == 0;
            snprintf(lab, sizeof(lab), "%s", on ? "abre" : "cierra");
            code = on ? 2u : 1u;
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
    bool selectedOn = false;
    switch (kind) {
    case OpenMenu::Level1:
        anchor = levelChip_[0][draftLevel_ < kLevelCount ? draftLevel_ : 0];
        levels = true;
        selectedRelay = draftLevel_;
        break;
    case OpenMenu::Start1:
        anchor = startFieldBtn_[0];
        selectedRelay = draftR1_;
        break;
    case OpenMenu::End1:
        anchor = endFieldBtn_[0];
        selectedOn = draftEndOn_;
        break;
    case OpenMenu::Level2:
        anchor = levelChip_[1][draftLevel2_ < kLevelCount ? draftLevel2_ : 0];
        levels = true;
        selectedRelay = draftLevel2_;
        break;
    case OpenMenu::Start2:
        anchor = startFieldBtn_[1];
        selectedRelay = draftR2_ ? draftR2_ : 2;
        break;
    case OpenMenu::End2:
        anchor = endFieldBtn_[1];
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
    const lv_coord_t chipGaps = static_cast<lv_coord_t>(kLevelCount - 1) * gap;
    const lv_coord_t chipW = (chipsSpan - chipGaps) / kLevelCount;
    const lv_coord_t usedW = chipW > 48 ? chipW : 48;
    for (uint8_t i = 0; i < kLevelCount; ++i) {
        const uintptr_t pack = (static_cast<uintptr_t>(step + 1) << 4) | (i + 1);
        lv_obj_t *chip =
            UiKit::makeSecondaryButton(parent, kLevelBtn[i], usedW, h, onIgnore);
        lv_obj_set_pos(chip, x, y2);
        lv_obj_add_event_cb(chip, onLevelChip, LV_EVENT_CLICKED, reinterpret_cast<void *>(pack));
        levelChip_[step][i] = chip;
        x += usedW + gap;
    }
    placeWord(parent, "luego", x, y2);
    x += luegoW + gap;
    lv_obj_t *endBtn = UiKit::makeSecondaryButton(parent, "cierra", endW, h, onIgnore);
    lv_obj_set_pos(endBtn, x, y2);
    lv_obj_add_event_cb(endBtn, onToggleEnd, LV_EVENT_CLICKED,
                        reinterpret_cast<void *>(static_cast<uintptr_t>(step + 1)));
    endFieldBtn_[step] = endBtn;
    endFieldLbl_[step] = lv_obj_get_child(endBtn, 0);
    return h + 6 + h;
}

void showChrome(bool list, bool explain, bool edit) {
    if (plusBtn_) {
        if (list) {
            lv_obj_clear_flag(plusBtn_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(plusBtn_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (gearBtn_) {
        if (explain) {
            lv_obj_clear_flag(gearBtn_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(gearBtn_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (listHost_) {
        if (list) {
            lv_obj_clear_flag(listHost_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(listHost_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (explainHost_) {
        if (explain) {
            lv_obj_clear_flag(explainHost_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(explainHost_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (editHost_) {
        if (edit) {
            lv_obj_clear_flag(editHost_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(editHost_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void showList() {
    mode_ = Mode::List;
    explainIx_ = -1;
    editIx_ = -1;
    deleteArmed_ = false;
    confirmAsk_ = ConfirmAsk::None;
    if (confirmHost_) {
        lv_obj_add_flag(confirmHost_, LV_OBJ_FLAG_HIDDEN);
    }
    closeMenu();
    hideKb();
    if (titleLbl_) {
        lv_label_set_text(titleLbl_, "Reglas");
    }
    showChrome(true, false, false);
    if (!listHost_) {
        return;
    }
    lv_obj_clean(listHost_);
    const int n = MasterLink::ruleViewCount();
    if (n <= 0) {
        lv_obj_t *empty = lv_label_create(listHost_);
        lv_label_set_text(empty, "Sin reglas en el Master.");
        lv_obj_set_width(empty, LCD_H_RES - 48);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(empty, AppTheme::muted(), 0);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(empty, LV_ALIGN_CENTER, 0, 0);
        return;
    }
    for (int i = 0; i < n; ++i) {
        const MasterLink::RuleView *rule = MasterLink::ruleViewAt(static_cast<uint8_t>(i));
        if (!rule) {
            continue;
        }
        lv_obj_t *row =
            UiKit::makeHubMenuRow(listHost_, rule->name, rule->enabled ? "activa" : "parada", onRow,
                                  reinterpret_cast<void *>(static_cast<uintptr_t>(i + 1)));
        lv_obj_set_width(row, LCD_H_RES - 16);
    }
}

void showExplain(int ix) {
    const MasterLink::RuleView *rule = MasterLink::ruleViewAt(static_cast<uint8_t>(ix));
    if (!rule || !explainHost_) {
        showList();
        return;
    }
    mode_ = Mode::Explain;
    explainIx_ = ix;
    deleteArmed_ = false;
    closeMenu();
    hideKb();
    if (titleLbl_) {
        lv_label_set_text(titleLbl_, rule->name);
    }
    showChrome(false, true, false);
    lv_obj_clean(explainHost_);

    lv_obj_t *body = lv_obj_create(explainHost_);
    lv_obj_remove_style_all(body);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(body, 6, 0);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    paintExplainSteps(body, rule);

    lv_obj_t *actions = lv_obj_create(explainHost_);
    lv_obj_remove_style_all(actions);
    lv_obj_set_width(actions, lv_pct(100));
    lv_obj_set_height(actions, AppTheme::TOUCH_MIN_H);
    lv_obj_set_style_bg_opa(actions, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(actions, 8, 0);
    lv_obj_clear_flag(actions, LV_OBJ_FLAG_SCROLLABLE);

    deleteBtn_ = UiKit::makeCautionButton(actions, "Eliminar", onDelete);
    lv_obj_set_flex_grow(deleteBtn_, 1);
    lv_obj_set_height(deleteBtn_, AppTheme::TOUCH_MIN_H);
    lv_obj_t *toggle =
        UiKit::makePrimaryButton(actions, rule->enabled ? "Desactivar" : "Activar", onToggle);
    lv_obj_set_flex_grow(toggle, 1);
    lv_obj_set_height(toggle, AppTheme::TOUCH_MIN_H);
}

void showEdit(int ix) {
    mode_ = Mode::Edit;
    editIx_ = ix;
    deleteArmed_ = false;
    clearErr();
    hideKb();
    closeMenu();
    const MasterLink::RuleView *rule =
        ix >= 0 ? MasterLink::ruleViewAt(static_cast<uint8_t>(ix)) : nullptr;
    if (rule) {
        if (rule->macs[0][0]) {
            strncpy(draftMac_, rule->macs[0], sizeof(draftMac_) - 1);
        } else {
            atlasMac(draftMac_, sizeof(draftMac_));
        }
        draftMac_[sizeof(draftMac_) - 1] = '\0';
        const int r1 = relayOf(rule, 0);
        draftR1_ = static_cast<uint8_t>(r1 + 1);
        char token[16];
        if (rule->untils[0][0]) {
            strncpy(token, rule->untils[0], sizeof(token) - 1);
            token[sizeof(token) - 1] = '\0';
        } else {
            levelTokenAt(rule->level, 0, token, sizeof(token));
        }
        draftLevel_ = static_cast<uint8_t>(untilIndex(token));
        draftEndOn_ = false;
        draftTwo_ = rule->stepCount > 1;
        if (draftTwo_) {
            const int r2 = relayOf(rule, 1);
            draftR2_ = static_cast<uint8_t>(r2 + 1);
            if (rule->untils[1][0]) {
                strncpy(token, rule->untils[1], sizeof(token) - 1);
                token[sizeof(token) - 1] = '\0';
            } else {
                levelTokenAt(rule->level, 1, token, sizeof(token));
            }
            draftLevel2_ = static_cast<uint8_t>(untilIndex(token));
            draftEndOn2_ = false;
        } else {
            draftR2_ = 2;
            draftLevel2_ = 4;
            draftEndOn2_ = false;
        }
        if (nameTa_) {
            lv_textarea_set_text(nameTa_, rule->name);
        }
        if (titleLbl_) {
            lv_label_set_text(titleLbl_, "Editar");
        }
    } else {
        editIx_ = -1;
        atlasMac(draftMac_, sizeof(draftMac_));
        draftTwo_ = false;
        draftLevel_ = 0;
        draftLevel2_ = 4;
        draftR1_ = 1;
        draftR2_ = 2;
        draftEndOn_ = false;
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
    showChrome(false, false, true);
    if (editHost_) {
        lv_obj_scroll_to_y(editHost_, 0, LV_ANIM_OFF);
    }
}

void syncRulesHub() {
    const uint32_t rules = MasterLink::ruleViewGeneration();
    const uint32_t names = MasterLink::relayCatalogGeneration();
    if (rules == seenRule_ && names == seenName_) {
        return;
    }
    /* No reconstruir UI con el diálogo abierto; mantener seen stale para redibujar al cerrar. */
    if (confirmAsk_ != ConfirmAsk::None) {
        return;
    }
    seenRule_ = rules;
    seenName_ = names;
    if (mode_ == Mode::Edit) {
        refreshFields();
        return;
    }
    if (mode_ == Mode::Explain) {
        if (!MasterLink::ruleViewAt(static_cast<uint8_t>(explainIx_))) {
            showList();
        } else {
            showExplain(explainIx_);
        }
        return;
    }
    showList();
}

lv_obj_t *cornerButton(lv_obj_t *parent, const char *text, lv_event_cb_t cb) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, 48, AppTheme::BACK_H);
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -(AppTheme::PAD - 4), AppTheme::PAD - 4);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    UiKit::applyPressStyle(btn, AppTheme::accent(), AppTheme::accentPressed(), AppTheme::accent(),
                           AppTheme::accentPressed());
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, AppTheme::bg(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl);
    return btn;
}

}  // namespace

lv_obj_t *Screens::createRulesHub(lv_obj_t *parent) {
    mode_ = Mode::List;
    explainIx_ = -1;
    editIx_ = -1;

    lv_obj_t *root = lv_obj_create(parent);
    root_ = root;
    UiKit::styleScreen(root);
    titleLbl_ = UiKit::styleHeader(root, "Reglas", onBack, 64);

    plusBtn_ = cornerButton(root, "+", onNew);
    gearBtn_ = cornerButton(root, LV_SYMBOL_SETTINGS, onGear);
    lv_obj_add_flag(gearBtn_, LV_OBJ_FLAG_HIDDEN);

    listHost_ = lv_obj_create(root);
    lv_obj_remove_style_all(listHost_);
    lv_obj_set_size(listHost_, LCD_H_RES, LCD_V_RES - AppTheme::MENU_LIST_TOP);
    lv_obj_align(listHost_, LV_ALIGN_TOP_LEFT, 0, AppTheme::MENU_LIST_TOP);
    lv_obj_set_style_bg_opa(listHost_, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(listHost_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(listHost_, LV_DIR_VER);
    lv_obj_set_flex_flow(listHost_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(listHost_, 8, 0);
    lv_obj_set_style_pad_all(listHost_, 8, 0);

    explainHost_ = lv_obj_create(root);
    lv_obj_remove_style_all(explainHost_);
    lv_obj_set_size(explainHost_, LCD_H_RES, LCD_V_RES - AppTheme::MENU_LIST_TOP);
    lv_obj_align(explainHost_, LV_ALIGN_TOP_LEFT, 0, AppTheme::MENU_LIST_TOP);
    UiKit::forceOpaqueBg(explainHost_, AppTheme::bg());
    lv_obj_set_style_pad_all(explainHost_, 16, 0);
    lv_obj_set_flex_flow(explainHost_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(explainHost_, 8, 0);
    lv_obj_clear_flag(explainHost_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(explainHost_, LV_OBJ_FLAG_HIDDEN);

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

    addSecondBtn_ = UiKit::makeSecondaryButton(editHost_, "Añadir segundo paso", LCD_H_RES - 24,
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

    confirmHost_ = lv_obj_create(root);
    lv_obj_remove_style_all(confirmHost_);
    lv_obj_set_size(confirmHost_, LCD_H_RES, LCD_V_RES);
    lv_obj_set_pos(confirmHost_, 0, 0);
    lv_obj_set_style_bg_color(confirmHost_, AppTheme::bg(), 0);
    lv_obj_set_style_bg_opa(confirmHost_, LV_OPA_80, 0);
    lv_obj_set_style_border_width(confirmHost_, 0, 0);
    lv_obj_clear_flag(confirmHost_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(confirmHost_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(confirmHost_, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *panel = lv_obj_create(confirmHost_);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, LCD_H_RES - 28, 168);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    UiKit::forceOpaqueBg(panel, AppTheme::surface());
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, AppTheme::gridLine(), 0);
    lv_obj_set_style_radius(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    confirmTitle_ = lv_label_create(panel);
    lv_label_set_text(confirmTitle_, "");
    lv_obj_set_style_text_color(confirmTitle_, AppTheme::text(), 0);
    lv_obj_set_style_text_font(confirmTitle_, &lv_font_montserrat_16, 0);
    lv_obj_set_width(confirmTitle_, LCD_H_RES - 56);
    lv_obj_set_style_text_align(confirmTitle_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(confirmTitle_, LV_LABEL_LONG_WRAP);
    lv_obj_align(confirmTitle_, LV_ALIGN_TOP_MID, 0, 16);

    confirmName_ = lv_label_create(panel);
    lv_label_set_text(confirmName_, "");
    lv_obj_set_style_text_color(confirmName_, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(confirmName_, &lv_font_montserrat_14, 0);
    lv_obj_set_width(confirmName_, LCD_H_RES - 56);
    lv_obj_set_style_text_align(confirmName_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(confirmName_, LV_LABEL_LONG_DOT);
    lv_obj_align(confirmName_, LV_ALIGN_TOP_MID, 0, 48);

    const lv_coord_t btnW = (LCD_H_RES - 28 - 36) / 2;
    lv_obj_t *noBtn =
        UiKit::makeSecondaryButton(panel, "No", btnW, AppTheme::BTN_PRIMARY_H, onConfirmNo);
    lv_obj_align(noBtn, LV_ALIGN_BOTTOM_LEFT, 12, -12);
    lv_obj_t *yesBtn = UiKit::makeCautionButton(panel, "Si", onConfirmYes);
    lv_obj_set_size(yesBtn, btnW, AppTheme::BTN_PRIMARY_H);
    lv_obj_align(yesBtn, LV_ALIGN_BOTTOM_RIGHT, -12, -12);

    showList();
    lastRulesPollMs_ = millis();
    MasterLink::requestRules();
    return root;
}

void Screens::refreshRulesHub(lv_obj_t *root) {
    (void)root;
    const uint32_t now = millis();
    if (static_cast<uint32_t>(now - lastRulesPollMs_) >= kRulesPollMs) {
        lastRulesPollMs_ = now;
        /* Tras ejecutar, el Master apaga la regla; re-pedir dump para hint/botón. */
        MasterLink::requestRules();
    }
    syncRulesHub();
}
