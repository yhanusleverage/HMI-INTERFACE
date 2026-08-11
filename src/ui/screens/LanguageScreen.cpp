#include "Screens.h"
#include "NavShell.h"
#include "AppLocale.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

namespace {

lv_obj_t *esRow = nullptr;
lv_obj_t *enRow = nullptr;
lv_obj_t *ptRow = nullptr;
lv_obj_t *hintLbl = nullptr;

void styleLangRow(lv_obj_t *row, bool on) {
    UiKit::applySelectionStyle(row, on, false);
}

void paintSelection() {
    const AppLang cur = AppLocale::language();
    styleLangRow(esRow, cur == AppLang::Es);
    styleLangRow(enRow, cur == AppLang::En);
    styleLangRow(ptRow, cur == AppLang::Pt);
    if (hintLbl) {
        char buf[64];
        if (NavShell::inWizard()) {
            snprintf(buf, sizeof(buf), "%s", Strings::tr(Msg::WizardLangHint));
        } else {
            snprintf(buf, sizeof(buf), "%s: %s", Strings::tr(Msg::ActiveLang),
                     AppLocale::languageName(cur));
        }
        lv_label_set_text(hintLbl, buf);
    }
}

void applyLang(AppLang lang) {
    AppLocale::setLanguage(lang);
    NavShell::reloadUi();
}

void onBack(lv_event_t *) { NavShell::back(); }
void onEs(lv_event_t *) { applyLang(AppLang::Es); }
void onEn(lv_event_t *) { applyLang(AppLang::En); }
void onPt(lv_event_t *) { applyLang(AppLang::Pt); }
void onContinue(lv_event_t *) { NavShell::wizardContinue(); }

}  // namespace

lv_obj_t *Screens::createLanguage(lv_obj_t *parent) {
    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);

    if (NavShell::inWizard()) {
        lv_obj_t *title = lv_label_create(root);
        lv_label_set_text(title, Strings::tr(Msg::LanguageTitle));
        UiKit::styleTitle(title);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, AppTheme::PAD, 10);
    } else {
        UiKit::styleHeader(root, Strings::tr(Msg::LanguageTitle), onBack);
    }

    const lv_coord_t step = AppTheme::MENU_ROW_H + 4;
    lv_coord_t y = AppTheme::MENU_LIST_TOP;

    esRow = UiKit::makeMenuRow(root, Strings::tr(Msg::LangEs), onEs, nullptr);
    lv_obj_align(esRow, LV_ALIGN_TOP_MID, 0, y);
    y += step;

    enRow = UiKit::makeMenuRow(root, Strings::tr(Msg::LangEn), onEn, nullptr);
    lv_obj_align(enRow, LV_ALIGN_TOP_MID, 0, y);
    y += step;

    ptRow = UiKit::makeMenuRow(root, Strings::tr(Msg::LangPt), onPt, nullptr);
    lv_obj_align(ptRow, LV_ALIGN_TOP_MID, 0, y);

    lv_obj_t *cont = nullptr;
    if (NavShell::inWizard()) {
        lv_obj_t *step = lv_label_create(root);
        char sbuf[24];
        snprintf(sbuf, sizeof(sbuf), Strings::tr(Msg::WizardStep), NavShell::wizardSetupStep(),
                 NavShell::wizardSetupTotal());
        lv_label_set_text(step, sbuf);
        UiKit::styleHint(step);
        lv_obj_align(step, LV_ALIGN_TOP_RIGHT, -AppTheme::PAD, 12);

        cont = UiKit::makePrimaryButton(root, Strings::tr(Msg::Continue), onContinue);
        lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, -6);
    }

    hintLbl = lv_label_create(root);
    UiKit::styleHint(hintLbl);
    lv_obj_set_width(hintLbl, LCD_H_RES - 32);
    lv_label_set_long_mode(hintLbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(hintLbl, LV_TEXT_ALIGN_CENTER, 0);
    if (cont) {
        lv_obj_align_to(hintLbl, cont, LV_ALIGN_OUT_TOP_MID, 0, -8);
    } else {
        lv_obj_align(hintLbl, LV_ALIGN_BOTTOM_MID, 0, -12);
    }

    paintSelection();
    return root;
}

void Screens::refreshLanguage(lv_obj_t *root) {
    (void)root;
    paintSelection();
}
