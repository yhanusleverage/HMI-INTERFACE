#include "Screens.h"
#include "NavShell.h"
#include "MasterLink.h"
#include "ReservoirConfig.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

/**
 * Sub-hub de Ajustes. Confirmar corta la malla y deja el candado INACTIVO.
 * No vive en Controle y no cambia los interruptores Auto EC / Auto pH.
 */

namespace {

lv_obj_t *statusLbl_ = nullptr;
lv_obj_t *confirm_ = nullptr;
lv_obj_t *toast_ = nullptr;
lv_timer_t *toastTimer_ = nullptr;
bool stopped_ = false;

void paintStatus() {
    if (!statusLbl_) {
        return;
    }
    const char *text = Strings::tr(Msg::UrgentArmed);
    if (stopped_) {
        text = Strings::tr(Msg::UrgentDone);
    } else if (!ReservoirConfig::dosingArmed()) {
        text = Strings::tr(Msg::UrgentIdle);
    }
    lv_label_set_text(statusLbl_, text);
    lv_obj_set_style_text_color(statusLbl_,
                                ReservoirConfig::dosingArmed() ? AppTheme::warn() : AppTheme::alarm(),
                                0);
}

void hideToast() {
    if (toastTimer_) {
        lv_timer_del(toastTimer_);
        toastTimer_ = nullptr;
    }
    if (toast_) {
        lv_obj_add_flag(toast_, LV_OBJ_FLAG_HIDDEN);
    }
}

void hideConfirm() {
    if (confirm_) {
        lv_obj_add_flag(confirm_, LV_OBJ_FLAG_HIDDEN);
    }
}

void onToastTimer(lv_timer_t *t) {
    (void)t;
    toastTimer_ = nullptr;
    if (toast_) {
        lv_obj_add_flag(toast_, LV_OBJ_FLAG_HIDDEN);
    }
}

void showToast() {
    if (!toast_) {
        return;
    }
    hideToast();
    lv_obj_clear_flag(toast_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(toast_);
    toastTimer_ = lv_timer_create(onToastTimer, 1800, nullptr);
    lv_timer_set_repeat_count(toastTimer_, 1);
}

void onBack(lv_event_t *) {
    hideConfirm();
    hideToast();
    NavShell::back();
}

void onConfirmCancel(lv_event_t *) { hideConfirm(); }

void onConfirmOk(lv_event_t *) {
    hideConfirm();
    MasterLink::sendMeshStop();
    ReservoirConfig::setDosingArmed(false);
    stopped_ = true;
    paintStatus();
    showToast();
}

void onStop(lv_event_t *) {
    if (!confirm_) {
        return;
    }
    hideToast();
    lv_obj_clear_flag(confirm_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(confirm_);
}

}  // namespace

lv_obj_t *Screens::createUrgent(lv_obj_t *parent) {
    statusLbl_ = nullptr;
    confirm_ = nullptr;
    toast_ = nullptr;
    toastTimer_ = nullptr;
    stopped_ = false;

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root, Strings::tr(Msg::UrgentTitle), onBack);

    lv_obj_t *body = lv_label_create(root);
    lv_label_set_text(body, Strings::tr(Msg::UrgentBody));
    lv_obj_set_style_text_color(body, AppTheme::text(), 0);
    lv_obj_set_style_text_font(body, &lv_font_montserrat_16, 0);
    lv_obj_set_width(body, LCD_H_RES - 32);
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, AppTheme::MENU_LIST_TOP);

    statusLbl_ = lv_label_create(root);
    lv_obj_set_style_text_font(statusLbl_, &lv_font_montserrat_20, 0);
    lv_obj_set_width(statusLbl_, LCD_H_RES - 32);
    lv_label_set_long_mode(statusLbl_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(statusLbl_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(statusLbl_, body, LV_ALIGN_OUT_BOTTOM_MID, 0, 16);
    paintStatus();

    lv_obj_t *stop = UiKit::makeCautionButton(root, Strings::tr(Msg::UrgentStop), onStop);
    lv_obj_align(stop, LV_ALIGN_BOTTOM_MID, 0, -16);

    confirm_ = lv_obj_create(root);
    lv_obj_remove_style_all(confirm_);
    lv_obj_set_size(confirm_, LCD_H_RES, LCD_V_RES);
    lv_obj_set_pos(confirm_, 0, 0);
    lv_obj_set_style_bg_color(confirm_, AppTheme::bg(), 0);
    lv_obj_set_style_bg_opa(confirm_, LV_OPA_80, 0);
    lv_obj_set_style_border_width(confirm_, 0, 0);
    lv_obj_set_style_pad_all(confirm_, 0, 0);
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

    lv_obj_t *bar = lv_obj_create(panel);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, LCD_H_RES - 28, 4);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    UiKit::forceOpaqueBg(bar, AppTheme::alarm());
    lv_obj_set_style_border_width(bar, 0, 0);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, Strings::tr(Msg::UrgentConfirmTitle));
    lv_obj_set_style_text_color(title, AppTheme::text(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_width(title, LCD_H_RES - 56);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    lv_obj_t *ask = lv_label_create(panel);
    lv_label_set_text(ask, Strings::tr(Msg::UrgentConfirmBody));
    lv_obj_set_style_text_color(ask, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(ask, &lv_font_montserrat_14, 0);
    lv_obj_set_width(ask, LCD_H_RES - 56);
    lv_obj_set_style_text_align(ask, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(ask, LV_LABEL_LONG_WRAP);
    lv_obj_align(ask, LV_ALIGN_TOP_MID, 0, 56);

    const lv_coord_t btnW = (LCD_H_RES - 28 - 36) / 2;
    lv_obj_t *cancelBtn = UiKit::makeSecondaryButton(panel, Strings::tr(Msg::ArmConfirmCancel), btnW,
                                                     AppTheme::BTN_PRIMARY_H, onConfirmCancel);
    lv_obj_align(cancelBtn, LV_ALIGN_BOTTOM_LEFT, 12, -12);

    lv_obj_t *okBtn = UiKit::makeCautionButton(panel, Strings::tr(Msg::UrgentConfirmOk), onConfirmOk);
    lv_obj_set_size(okBtn, btnW, AppTheme::BTN_PRIMARY_H);
    lv_obj_align(okBtn, LV_ALIGN_BOTTOM_RIGHT, -12, -12);

    toast_ = lv_obj_create(root);
    lv_obj_remove_style_all(toast_);
    lv_obj_set_size(toast_, LCD_H_RES - 40, 44);
    lv_obj_align(toast_, LV_ALIGN_BOTTOM_MID, 0, -(AppTheme::BTN_PRIMARY_H + 20));
    UiKit::forceOpaqueBg(toast_, AppTheme::surfaceAlt());
    lv_obj_set_style_border_width(toast_, 1, 0);
    lv_obj_set_style_border_color(toast_, AppTheme::alarm(), 0);
    lv_obj_set_style_radius(toast_, 0, 0);
    lv_obj_clear_flag(toast_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(toast_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(toast_, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *toastBar = lv_obj_create(toast_);
    lv_obj_remove_style_all(toastBar);
    lv_obj_set_size(toastBar, 4, 44);
    lv_obj_align(toastBar, LV_ALIGN_LEFT_MID, 0, 0);
    UiKit::forceOpaqueBg(toastBar, AppTheme::alarm());
    lv_obj_set_style_border_width(toastBar, 0, 0);

    lv_obj_t *toastLbl = lv_label_create(toast_);
    lv_label_set_text(toastLbl, Strings::tr(Msg::UrgentToastInactive));
    lv_obj_set_style_text_color(toastLbl, AppTheme::text(), 0);
    lv_obj_set_style_text_font(toastLbl, &lv_font_montserrat_16, 0);
    lv_obj_set_width(toastLbl, LCD_H_RES - 72);
    lv_obj_set_style_text_align(toastLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(toastLbl, LV_LABEL_LONG_CLIP);
    lv_obj_align(toastLbl, LV_ALIGN_CENTER, 4, 0);

    return root;
}

void Screens::refreshUrgent(lv_obj_t *root) {
    (void)root;
    paintStatus();
}
