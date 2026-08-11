#include "Screens.h"
#include "NavShell.h"
#include "MasterWifiDraft.h"
#include "AppStrings.h"
#include "Config.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

namespace {

unsigned long welcomeShownMs = 0;
bool advanced = false;

void onContinue(lv_event_t *) {
    advanced = true;
    NavShell::wizardContinue();
}

}  // namespace

lv_obj_t *Screens::createWelcome(lv_obj_t *parent) {
    welcomeShownMs = millis();
    advanced = false;
    MasterWifiDraft::clear();

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *brand = lv_label_create(root);
    lv_label_set_text(brand, SYSTEM_NAME);
    lv_obj_set_style_text_color(brand, AppTheme::text(), 0);
    lv_obj_set_style_text_font(brand, &lv_font_montserrat_28, 0);
    lv_obj_align(brand, LV_ALIGN_CENTER, 0, -36);

    lv_obj_t *tag = lv_label_create(root);
    lv_label_set_text(tag, Strings::tr(Msg::WelcomeTagline));
    lv_obj_set_style_text_color(tag, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(tag, &lv_font_montserrat_14, 0);
    lv_obj_align(tag, LV_ALIGN_CENTER, 0, 4);

    lv_obj_t *hint = lv_label_create(root);
    lv_label_set_text(hint, Strings::tr(Msg::WelcomeHint));
    UiKit::styleHint(hint);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 36);

    lv_obj_t *cont =
        UiKit::makePrimaryButton(root, Strings::tr(Msg::Continue), onContinue);
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, -12);

    return root;
}

void Screens::refreshWelcome(lv_obj_t *root) {
    (void)root;
    if (advanced || !NavShell::inWizard()) {
        return;
    }
    if (welcomeShownMs != 0 && (millis() - welcomeShownMs) >= 2000UL) {
        advanced = true;
        NavShell::wizardContinue();
    }
}
