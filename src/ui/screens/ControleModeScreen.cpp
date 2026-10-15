#include "Screens.h"
#include "NavShell.h"
#include "MasterLink.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

#include <Arduino.h>
#include <string.h>

namespace {

lv_obj_t *normalBtn_ = nullptr;
lv_obj_t *carreraBtn_ = nullptr;
lv_obj_t *confirm_ = nullptr;
lv_obj_t *titleLbl_ = nullptr;
lv_obj_t *bodyLbl_ = nullptr;
lv_obj_t *errLbl_ = nullptr;
char pending_[8] = "normal";

void paintRow(lv_obj_t *btn, bool active) {
    if (!btn) {
        return;
    }
    lv_obj_set_style_border_color(btn, active ? AppTheme::accent() : AppTheme::gridLine(), 0);
    lv_obj_set_style_border_width(btn, active ? 2 : 1, 0);
    lv_obj_t *hint = lv_obj_get_child(btn, 1);
    if (hint) {
        lv_label_set_text(hint, active ? Strings::tr(Msg::ModeInUse) : "");
    }
}

void paintSelection() {
    const bool carrera = strcmp(MasterLink::levelInterlockMode(), "carrera") == 0;
    paintRow(normalBtn_, !carrera);
    paintRow(carreraBtn_, carrera);
}

void showErr(const char *text) {
    if (!errLbl_) {
        return;
    }
    lv_label_set_text(errLbl_, text ? text : "");
    if (text && text[0]) {
        lv_obj_clear_flag(errLbl_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(errLbl_, LV_OBJ_FLAG_HIDDEN);
    }
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

void onBack(lv_event_t *) { NavShell::back(); }

void openConfirm(const char *mode) {
    if (!confirm_ || !mode) {
        return;
    }
    const bool carrera = strcmp(mode, "carrera") == 0;
    strncpy(pending_, carrera ? "carrera" : "normal", sizeof(pending_) - 1);
    pending_[sizeof(pending_) - 1] = '\0';
    if (titleLbl_) {
        lv_label_set_text(titleLbl_, Strings::tr(carrera ? Msg::ModeCarrera : Msg::ModeNormal));
    }
    if (bodyLbl_) {
        lv_label_set_text(bodyLbl_,
                          Strings::tr(carrera ? Msg::ModeCarreraBody : Msg::ModeNormalBody));
    }
    lv_obj_clear_flag(confirm_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(confirm_);
}

void onNormal(lv_event_t *) { openConfirm("normal"); }
void onCarrera(lv_event_t *) { openConfirm("carrera"); }

void onNo(lv_event_t *) {
    if (confirm_) {
        lv_obj_add_flag(confirm_, LV_OBJ_FLAG_HIDDEN);
    }
}

void onYes(lv_event_t *) {
    if (confirm_) {
        lv_obj_add_flag(confirm_, LV_OBJ_FLAG_HIDDEN);
    }
    MasterLink::clearLastCmdAck();
    MasterLink::sendLevelInterlock(pending_);
    const uint8_t ack = waitCmd("level_interlock", 2500);
    if (ack == 1) {
        MasterLink::rememberLevelInterlock(pending_);
        showErr("");
        paintSelection();
        return;
    }
    showErr(Strings::tr(ack == 2 ? Msg::ModeReject : Msg::ModeNoReply));
}

}  // namespace

lv_obj_t *Screens::createControleMode(lv_obj_t *parent) {
    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    UiKit::styleHeader(root, Strings::tr(Msg::ControleMode), onBack);

    const lv_coord_t top = AppTheme::MENU_LIST_TOP;
    normalBtn_ = UiKit::makeHubMenuRow(root, Strings::tr(Msg::ModeNormal), "", onNormal, nullptr);
    lv_obj_set_pos(normalBtn_, 12, top);
    carreraBtn_ =
        UiKit::makeHubMenuRow(root, Strings::tr(Msg::ModeCarrera), "", onCarrera, nullptr);
    lv_obj_set_pos(carreraBtn_, 12, top + AppTheme::HUB_ROW_H + 4);

    errLbl_ = lv_label_create(root);
    lv_label_set_text(errLbl_, "");
    lv_obj_set_width(errLbl_, LCD_H_RES - 32);
    lv_obj_set_style_text_color(errLbl_, AppTheme::alarm(), 0);
    lv_obj_set_style_text_font(errLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(errLbl_, LV_LABEL_LONG_WRAP);
    lv_obj_align(errLbl_, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_add_flag(errLbl_, LV_OBJ_FLAG_HIDDEN);

    confirm_ = lv_obj_create(root);
    lv_obj_remove_style_all(confirm_);
    lv_obj_set_size(confirm_, LCD_H_RES, LCD_V_RES);
    lv_obj_set_pos(confirm_, 0, 0);
    lv_obj_set_style_bg_color(confirm_, AppTheme::bg(), 0);
    lv_obj_set_style_bg_opa(confirm_, LV_OPA_80, 0);
    lv_obj_clear_flag(confirm_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(confirm_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(confirm_, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *panel = lv_obj_create(confirm_);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, LCD_H_RES - 28, 196);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    UiKit::forceOpaqueBg(panel, AppTheme::surface());
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, AppTheme::gridLine(), 0);
    lv_obj_set_style_radius(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    titleLbl_ = lv_label_create(panel);
    lv_label_set_text(titleLbl_, "");
    lv_obj_set_style_text_color(titleLbl_, AppTheme::text(), 0);
    lv_obj_set_style_text_font(titleLbl_, &lv_font_montserrat_20, 0);
    lv_obj_set_width(titleLbl_, LCD_H_RES - 56);
    lv_obj_set_style_text_align(titleLbl_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(titleLbl_, LV_ALIGN_TOP_MID, 0, 16);

    bodyLbl_ = lv_label_create(panel);
    lv_label_set_text(bodyLbl_, "");
    lv_obj_set_style_text_color(bodyLbl_, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(bodyLbl_, &lv_font_montserrat_14, 0);
    lv_obj_set_width(bodyLbl_, LCD_H_RES - 56);
    lv_obj_set_style_text_align(bodyLbl_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(bodyLbl_, LV_LABEL_LONG_WRAP);
    lv_obj_align(bodyLbl_, LV_ALIGN_TOP_MID, 0, 52);

    const lv_coord_t btnW = (LCD_H_RES - 28 - 36) / 2;
    lv_obj_t *noBtn = UiKit::makeSecondaryButton(panel, Strings::tr(Msg::ModeNo), btnW,
                                                 AppTheme::BTN_PRIMARY_H, onNo);
    lv_obj_align(noBtn, LV_ALIGN_BOTTOM_LEFT, 12, -12);
    lv_obj_t *yesBtn = UiKit::makeCautionButton(panel, Strings::tr(Msg::ModeYes), onYes);
    lv_obj_set_size(yesBtn, btnW, AppTheme::BTN_PRIMARY_H);
    lv_obj_align(yesBtn, LV_ALIGN_BOTTOM_RIGHT, -12, -12);

    paintSelection();
    return root;
}

void Screens::refreshControleMode(lv_obj_t *root) {
    (void)root;
    if (confirm_ && !lv_obj_has_flag(confirm_, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }
    paintSelection();
}
