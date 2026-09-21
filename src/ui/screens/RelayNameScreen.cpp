#include "Screens.h"
#include "NavShell.h"
#include "RelayAliasConfig.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

#include <cstdio>
#include <cstring>

/** Nombre (alias) del relé Atlas actual. */

namespace {

lv_obj_t *nameTa = nullptr;
lv_obj_t *kb = nullptr;
lv_obj_t *statusLbl = nullptr;
char draft_[RELAY_ALIAS_NAME_LEN] = {};

void setStatus(UiKit::PumpUiStatus st, const char *msg) {
    UiKit::setPumpStatus(statusLbl, st, msg);
}

void hideKb() {
    if (kb) {
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
}

void onBack(lv_event_t *) {
    hideKb();
    NavShell::back();
}

void onKbDone(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY && nameTa) {
        strncpy(draft_, lv_textarea_get_text(nameTa), RELAY_ALIAS_NAME_LEN - 1);
        draft_[RELAY_ALIAS_NAME_LEN - 1] = '\0';
        RelayAliasConfig::setName(NavShell::currentAtlasMac(), NavShell::currentAtlasRelay(), draft_);
        hideKb();
        NavShell::back();
        return;
    }
    hideKb();
}

void onNameFocus(lv_event_t *) {
    if (!kb || !nameTa) {
        return;
    }
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(kb, nameTa);
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(kb);
}

void onSave(lv_event_t *) {
    if (nameTa) {
        strncpy(draft_, lv_textarea_get_text(nameTa), RELAY_ALIAS_NAME_LEN - 1);
        draft_[RELAY_ALIAS_NAME_LEN - 1] = '\0';
    }
    RelayAliasConfig::setName(NavShell::currentAtlasMac(), NavShell::currentAtlasRelay(), draft_);
    hideKb();
    NavShell::back();
}

}  // namespace

lv_obj_t *Screens::createRelayName(lv_obj_t *parent) {
    nameTa = nullptr;
    kb = nullptr;
    statusLbl = nullptr;
    memset(draft_, 0, sizeof(draft_));

    const char *cur =
        RelayAliasConfig::getName(NavShell::currentAtlasMac(), NavShell::currentAtlasRelay());
    if (cur && cur[0]) {
        strncpy(draft_, cur, RELAY_ALIAS_NAME_LEN - 1);
    } else {
        snprintf(draft_, sizeof(draft_), Strings::tr(Msg::AtlasRelayFmt),
                 static_cast<int>(NavShell::currentAtlasRelay() + 1));
    }

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root, Strings::tr(Msg::RelayActionName), onBack);

    lv_coord_t y = AppTheme::MENU_LIST_TOP + 4;

    lv_obj_t *hint = lv_label_create(root);
    lv_label_set_text(hint, Strings::tr(Msg::RelayActionNameHint));
    UiKit::styleHint(hint);
    lv_obj_set_width(hint, LCD_H_RES - 24);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(hint, 12, y);
    lv_obj_update_layout(hint);
    y += lv_obj_get_height(hint) + 8;

    nameTa = lv_textarea_create(root);
    lv_textarea_set_one_line(nameTa, true);
    lv_textarea_set_max_length(nameTa, RELAY_ALIAS_NAME_LEN - 1);
    lv_textarea_set_text(nameTa, draft_);
    lv_obj_set_size(nameTa, LCD_H_RES - 24, AppTheme::TOUCH_MIN_H + 8);
    lv_obj_set_pos(nameTa, 12, y);
    lv_obj_set_style_bg_color(nameTa, AppTheme::surface(), 0);
    lv_obj_set_style_bg_opa(nameTa, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(nameTa, AppTheme::text(), 0);
    lv_obj_set_style_text_font(nameTa, &lv_font_montserrat_20, 0);
    lv_obj_set_style_pad_all(nameTa, 6, 0);
    lv_obj_set_style_border_color(nameTa, AppTheme::gridLine(), 0);
    lv_obj_set_style_border_width(nameTa, 1, 0);
    lv_obj_add_event_cb(nameTa, onNameFocus, LV_EVENT_FOCUSED, nullptr);
    y += AppTheme::TOUCH_MIN_H + 20;

    lv_obj_t *save = UiKit::makePrimaryButton(root, Strings::tr(Msg::NutSave), onSave);
    lv_obj_set_size(save, LCD_H_RES - 16, AppTheme::BTN_PRIMARY_H);
    lv_obj_set_pos(save, 8, y);
    y += AppTheme::BTN_PRIMARY_H + 8;

    statusLbl = lv_label_create(root);
    lv_obj_set_pos(statusLbl, 12, y);
    setStatus(UiKit::PumpUiStatus::Idle, Strings::tr(Msg::ReadyStatus));

    kb = lv_keyboard_create(root);
    UiKit::styleDarkKeyboard(kb);
    lv_obj_add_event_cb(kb, onKbDone, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(kb, onKbDone, LV_EVENT_CANCEL, nullptr);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

    return root;
}

void Screens::refreshRelayName(lv_obj_t *root) { (void)root; }
