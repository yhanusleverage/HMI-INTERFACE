#include "Screens.h"
#include "NavShell.h"
#include "MasterLink.h"
#include "MasterWifiDraft.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr lv_coord_t kKbH = AppTheme::KEYBOARD_H;
constexpr lv_coord_t kFooterH = 44;

lv_obj_t *root_ = nullptr;
lv_obj_t *title_ = nullptr;
lv_obj_t *stepLbl_ = nullptr;
lv_obj_t *hint_ = nullptr;
lv_obj_t *scroll_ = nullptr;
lv_obj_t *networkBanner_ = nullptr;
lv_obj_t *cont_ = nullptr;
lv_obj_t *emailLbl_ = nullptr;
lv_obj_t *nameLbl_ = nullptr;
lv_obj_t *locLbl_ = nullptr;
lv_obj_t *emailTa_ = nullptr;
lv_obj_t *nameTa_ = nullptr;
lv_obj_t *locTa_ = nullptr;
lv_obj_t *kb_ = nullptr;
lv_coord_t scrollTop_ = 0;
lv_coord_t scrollH_ = 0;

void setShown(lv_obj_t *obj, bool shown) {
    if (!obj) {
        return;
    }
    if (shown) {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

void layoutForKeyboard(bool open) {
    if (!scroll_) {
        return;
    }
    setShown(title_, !open);
    setShown(stepLbl_, !open);
    setShown(hint_, !open);
    setShown(networkBanner_, !open);
    setShown(cont_, !open);
    if (open) {
        lv_obj_set_pos(scroll_, 0, 4);
        lv_obj_set_size(scroll_, LCD_H_RES, LCD_V_RES - kKbH - 8);
    } else {
        lv_obj_set_pos(scroll_, 0, scrollTop_);
        lv_obj_set_size(scroll_, LCD_H_RES, scrollH_ > 40 ? scrollH_ : 40);
        lv_obj_scroll_to_y(scroll_, 0, LV_ANIM_OFF);
    }
}

lv_obj_t *labelFor(lv_obj_t *ta) {
    if (ta == emailTa_) {
        return emailLbl_;
    }
    if (ta == nameTa_) {
        return nameLbl_;
    }
    if (ta == locTa_) {
        return locLbl_;
    }
    return nullptr;
}

void hideKb() {
    if (kb_) {
        lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    }
    layoutForKeyboard(false);
}

void showKb(lv_obj_t *ta) {
    if (!kb_ || !ta) {
        return;
    }
    lv_keyboard_set_mode(kb_, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(kb_, ta);
    lv_obj_clear_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(kb_);
    layoutForKeyboard(true);
    if (scroll_) {
        lv_obj_t *lbl = labelFor(ta);
        lv_coord_t y = lbl ? lv_obj_get_y(lbl) : lv_obj_get_y(ta);
        if (y > 2) {
            y -= 2;
        }
        lv_obj_scroll_to_y(scroll_, y, LV_ANIM_OFF);
    }
}

void onKbDone(lv_event_t *) { hideKb(); }

void onTaFocus(lv_event_t *e) { showKb(lv_event_get_target(e)); }

void readProfileFromUi() {
    const char *email = emailTa_ ? lv_textarea_get_text(emailTa_) : "";
    const char *name = nameTa_ ? lv_textarea_get_text(nameTa_) : "";
    const char *loc = locTa_ ? lv_textarea_get_text(locTa_) : "";
    MasterWifiDraft::setProfile(email, name, loc);
}

void advanceWizard(bool withProfile) {
    hideKb();
    if (withProfile) {
        readProfileFromUi();
    } else {
        MasterWifiDraft::setProfile("", "", "");
    }
    if (MasterWifiDraft::hasNetwork()) {
        (void)MasterWifiDraft::commitProvision();
    }
    NavShell::wizardContinue();
}

void onContinue(lv_event_t *) {
    hideKb();
    const char *email = emailTa_ ? lv_textarea_get_text(emailTa_) : "";
    const char *name = nameTa_ ? lv_textarea_get_text(nameTa_) : "";
    const char *loc = locTa_ ? lv_textarea_get_text(locTa_) : "";
    if (!email || !email[0] || !name || !name[0] || !loc || !loc[0] ||
        !MasterWifiDraft::hasNetwork()) {
        if (networkBanner_) {
            lv_label_set_text(networkBanner_, Strings::tr(Msg::WizardFieldsRequired));
        }
        return;
    }
    advanceWizard(true);
}

void paintNetworkBanner() {
    if (!networkBanner_) {
        return;
    }
    char buf[64];
    if (MasterWifiDraft::hasNetwork()) {
        snprintf(buf, sizeof(buf), Strings::tr(Msg::WizardDeviceNetworkFmt),
                 MasterWifiDraft::ssid());
        lv_label_set_text(networkBanner_, buf);
        lv_obj_set_style_text_color(networkBanner_, AppTheme::accent(), 0);
    } else {
        lv_label_set_text(networkBanner_, Strings::tr(Msg::WizardDeviceNoNetwork));
        lv_obj_set_style_text_color(networkBanner_, AppTheme::muted(), 0);
    }
}

lv_obj_t *makeFieldRow(lv_obj_t *parent, lv_coord_t *y, const char *label, size_t maxLen,
                       const char *initial, lv_obj_t **lblOut, lv_obj_t **taOut) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl, 12, *y);
    *y += 18;

    lv_obj_t *ta = lv_textarea_create(parent);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, Strings::tr(Msg::WizardDeviceOptionalPh));
    lv_textarea_set_max_length(ta, static_cast<uint32_t>(maxLen - 1));
    if (initial && initial[0]) {
        lv_textarea_set_text(ta, initial);
    }
    lv_obj_set_size(ta, LCD_H_RES - 24, AppTheme::TOUCH_MIN_H + 4);
    lv_obj_set_pos(ta, 12, *y);
    lv_obj_set_style_bg_color(ta, AppTheme::surface(), 0);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(ta, AppTheme::text(), 0);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_20, 0);
    lv_obj_set_style_border_width(ta, 1, 0);
    lv_obj_set_style_border_color(ta, AppTheme::gridLine(), 0);
    lv_obj_set_style_pad_all(ta, 6, 0);
    lv_obj_add_event_cb(ta, onTaFocus, LV_EVENT_FOCUSED, nullptr);
    *y += AppTheme::TOUCH_MIN_H + 12;
    if (lblOut) {
        *lblOut = lbl;
    }
    *taOut = ta;
    return ta;
}

}  // namespace

lv_obj_t *Screens::createMasterWifiProfile(lv_obj_t *parent) {
    MasterLink::requestSysInfo();
    root_ = nullptr;
    title_ = nullptr;
    stepLbl_ = nullptr;
    hint_ = nullptr;
    scroll_ = nullptr;
    networkBanner_ = nullptr;
    cont_ = nullptr;
    emailLbl_ = nameLbl_ = locLbl_ = nullptr;
    emailTa_ = nameTa_ = locTa_ = nullptr;
    kb_ = nullptr;
    scrollTop_ = 0;
    scrollH_ = 0;

    root_ = lv_obj_create(parent);
    UiKit::styleScreen(root_);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    title_ = lv_label_create(root_);
    lv_label_set_text(title_, Strings::tr(Msg::WizardDeviceTitle));
    UiKit::styleTitle(title_);
    lv_obj_align(title_, LV_ALIGN_TOP_LEFT, AppTheme::PAD, 10);

    stepLbl_ = lv_label_create(root_);
    char sbuf[24];
    snprintf(sbuf, sizeof(sbuf), Strings::tr(Msg::WizardStep), NavShell::wizardSetupStep(),
             NavShell::wizardSetupTotal());
    lv_label_set_text(stepLbl_, sbuf);
    UiKit::styleHint(stepLbl_);
    lv_obj_align(stepLbl_, LV_ALIGN_TOP_RIGHT, -AppTheme::PAD, 12);

    hint_ = lv_label_create(root_);
    lv_label_set_text(hint_, Strings::tr(Msg::WizardDeviceHint));
    UiKit::styleHint(hint_);
    lv_obj_set_width(hint_, LCD_H_RES - 24);
    lv_label_set_long_mode(hint_, LV_LABEL_LONG_WRAP);
    lv_obj_align(hint_, LV_ALIGN_TOP_LEFT, AppTheme::PAD, AppTheme::MENU_LIST_TOP);
    lv_obj_update_layout(hint_);
    const lv_coord_t bannerTop = AppTheme::MENU_LIST_TOP + lv_obj_get_height(hint_) + 6;

    networkBanner_ = lv_label_create(root_);
    lv_obj_set_style_text_font(networkBanner_, &lv_font_montserrat_16, 0);
    lv_obj_set_width(networkBanner_, LCD_H_RES - 24);
    lv_label_set_long_mode(networkBanner_, LV_LABEL_LONG_CLIP);
    lv_obj_align(networkBanner_, LV_ALIGN_TOP_LEFT, AppTheme::PAD, bannerTop);
    paintNetworkBanner();
    lv_obj_update_layout(networkBanner_);
    scrollTop_ = bannerTop + lv_obj_get_height(networkBanner_) + 8;
    scrollH_ = LCD_V_RES - scrollTop_ - kFooterH;

    scroll_ = lv_obj_create(root_);
    lv_obj_remove_style_all(scroll_);
    lv_obj_set_size(scroll_, LCD_H_RES, scrollH_ > 40 ? scrollH_ : 40);
    lv_obj_set_pos(scroll_, 0, scrollTop_);
    lv_obj_set_style_bg_opa(scroll_, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(scroll_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(scroll_, LV_DIR_VER);
    lv_obj_set_style_pad_bottom(scroll_, kKbH + 8, 0);

    lv_coord_t y = 4;
    makeFieldRow(scroll_, &y, Strings::tr(Msg::WizardDeviceEmailLabel), MasterWifiDraft::kEmailMax,
                 MasterWifiDraft::email(), &emailLbl_, &emailTa_);
    makeFieldRow(scroll_, &y, Strings::tr(Msg::WizardDeviceNameLabel), MasterWifiDraft::kNameMax,
                 MasterWifiDraft::deviceName(), &nameLbl_, &nameTa_);
    makeFieldRow(scroll_, &y, Strings::tr(Msg::WizardDeviceLocLabel), MasterWifiDraft::kLocMax,
                 MasterWifiDraft::location(), &locLbl_, &locTa_);

    cont_ = UiKit::makePrimaryButton(root_, Strings::tr(Msg::Continue), onContinue);
    lv_obj_align(cont_, LV_ALIGN_BOTTOM_MID, 0, -6);

    kb_ = lv_keyboard_create(root_);
    UiKit::styleDarkKeyboard(kb_);
    lv_obj_set_size(kb_, LCD_H_RES, kKbH);
    lv_obj_align(kb_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(kb_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(kb_, onKbDone, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(kb_, onKbDone, LV_EVENT_CANCEL, nullptr);

    return root_;
}

void Screens::refreshMasterWifiProfile(lv_obj_t *root) {
    (void)root;
    paintNetworkBanner();
    if (emailTa_ && MasterWifiDraft::email()[0] &&
        (!lv_textarea_get_text(emailTa_) || !lv_textarea_get_text(emailTa_)[0])) {
        lv_textarea_set_text(emailTa_, MasterWifiDraft::email());
    }
    if (nameTa_ && MasterWifiDraft::deviceName()[0] &&
        (!lv_textarea_get_text(nameTa_) || !lv_textarea_get_text(nameTa_)[0])) {
        lv_textarea_set_text(nameTa_, MasterWifiDraft::deviceName());
    }
    if (locTa_ && MasterWifiDraft::location()[0] &&
        (!lv_textarea_get_text(locTa_) || !lv_textarea_get_text(locTa_)[0])) {
        lv_textarea_set_text(locTa_, MasterWifiDraft::location());
    }
}
