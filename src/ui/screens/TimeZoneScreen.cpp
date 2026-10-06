#include "Screens.h"
#include "NavShell.h"
#include "AppLocale.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

#include <cstdio>

namespace {

lv_obj_t *listHost = nullptr;
lv_obj_t *hintLbl = nullptr;
int selectedIx = -1;

void paintRows() {
    if (!listHost) {
        return;
    }
    const int n = lv_obj_get_child_cnt(listHost);
    for (int i = 0; i < n; ++i) {
        lv_obj_t *row = lv_obj_get_child(listHost, i);
        if (!row) {
            continue;
        }
        UiKit::applySelectionStyle(row, i == selectedIx, false);
    }
}

void onBack(lv_event_t *) { NavShell::back(); }

void onPick(lv_event_t *e) {
    selectedIx = static_cast<int>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    paintRows();
}

void goNextOrBack() {
    if (NavShell::inWizard()) {
        NavShell::wizardContinue();
    } else {
        NavShell::back();
    }
}

void onSave(lv_event_t *) {
    if (selectedIx < 0 || selectedIx >= AppLocale::tzPresetCount()) {
        selectedIx = AppLocale::tzPresetIndexForMin(AppLocale::tzOffsetMin());
        if (selectedIx < 0) {
            selectedIx = AppLocale::tzPresetIndexForMin(-180);
        }
        if (selectedIx < 0) {
            selectedIx = 0;
        }
    }
    AppLocale::setTzOffsetMin(AppLocale::tzPresetMin(selectedIx));
    AppLocale::syncNtpIfOnline();
    goNextOrBack();
}

}  // namespace

lv_obj_t *Screens::createTimeZone(lv_obj_t *parent) {
    selectedIx = AppLocale::tzPresetIndexForMin(AppLocale::tzOffsetMin());
    if (selectedIx < 0) {
        selectedIx = AppLocale::tzPresetIndexForMin(-180);
        if (selectedIx < 0) {
            selectedIx = 0;
        }
    }

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    if (NavShell::inWizard()) {
        lv_obj_t *title = lv_label_create(root);
        lv_label_set_text(title, Strings::tr(Msg::TimeZoneTitle));
        UiKit::styleTitle(title);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, AppTheme::PAD, 10);
    } else {
        UiKit::styleHeader(root, Strings::tr(Msg::TimeZoneTitle), onBack);
    }

    hintLbl = lv_label_create(root);
    lv_label_set_text(hintLbl, NavShell::inWizard() ? Strings::tr(Msg::WizardTzHint)
                                                    : Strings::tr(Msg::TimeZoneHint));
    UiKit::styleHint(hintLbl);
    lv_obj_set_width(hintLbl, LCD_H_RES - 24);
    lv_label_set_long_mode(hintLbl, LV_LABEL_LONG_WRAP);
    lv_obj_align(hintLbl, LV_ALIGN_TOP_LEFT, AppTheme::PAD, AppTheme::MENU_LIST_TOP);
    lv_obj_update_layout(hintLbl);
    const lv_coord_t listTop = AppTheme::MENU_LIST_TOP + lv_obj_get_height(hintLbl) + 8;
    const lv_coord_t listH = LCD_V_RES - listTop - (AppTheme::BTN_PRIMARY_H + 16);

    listHost = lv_obj_create(root);
    lv_obj_remove_style_all(listHost);
    UiKit::forceOpaqueBg(listHost, AppTheme::bg());
    lv_obj_set_size(listHost, LCD_H_RES - 16, listH > 40 ? listH : 40);
    lv_obj_align(listHost, LV_ALIGN_TOP_MID, 0, listTop);
    lv_obj_set_flex_flow(listHost, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(listHost, 2, 0);
    lv_obj_set_style_pad_all(listHost, 4, 0);
    lv_obj_add_flag(listHost, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < AppLocale::tzPresetCount(); ++i) {
        lv_obj_t *row = UiKit::makeMenuRow(listHost, AppLocale::tzPresetLabel(i), onPick,
                                           reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
        lv_obj_set_width(row, LCD_H_RES - 32);
        lv_obj_set_style_border_color(row, AppTheme::gridLine(), 0);
    }
    paintRows();

    lv_obj_t *save = UiKit::makePrimaryButton(root, Strings::tr(Msg::SaveTz), onSave);
    lv_obj_set_size(save, 220, AppTheme::BTN_PRIMARY_H);
    lv_obj_align(save, LV_ALIGN_BOTTOM_LEFT, AppTheme::PAD, -6);

    if (NavShell::inWizard()) {
        lv_obj_t *step = lv_label_create(root);
        char sbuf[24];
        snprintf(sbuf, sizeof(sbuf), Strings::tr(Msg::WizardStep), NavShell::wizardSetupStep(),
                 NavShell::wizardSetupTotal());
        lv_label_set_text(step, sbuf);
        UiKit::styleHint(step);
        lv_obj_align(step, LV_ALIGN_TOP_RIGHT, -AppTheme::PAD, 12);
    }

    return root;
}

void Screens::refreshTimeZone(lv_obj_t *root) { (void)root; }
