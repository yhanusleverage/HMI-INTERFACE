#include "Screens.h"
#include "NavShell.h"
#include "AppLocale.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

namespace {

void onContinue(lv_event_t *) {
    AppLocale::setSetupDone(true);
    NavShell::finishWizard();
}

}  // namespace

lv_obj_t *Screens::createReady(lv_obj_t *parent) {
    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(root);
    lv_label_set_text(title, Strings::tr(Msg::ReadyTitle));
    lv_obj_set_style_text_color(title, AppTheme::text(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -40);

    lv_obj_t *body = lv_label_create(root);
    lv_label_set_text(body, Strings::tr(Msg::ReadyBody));
    lv_obj_set_style_text_color(body, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(body, &lv_font_montserrat_14, 0);
    lv_obj_set_width(body, LCD_H_RES - 48);
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(body, LV_ALIGN_CENTER, 0, 8);

    lv_obj_t *cont =
        UiKit::makePrimaryButton(root, Strings::tr(Msg::Continue), onContinue);
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, -12);

    return root;
}

void Screens::refreshReady(lv_obj_t *root) { (void)root; }
