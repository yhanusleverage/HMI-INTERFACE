#include "Screens.h"
#include "NavShell.h"
#include "DataStore.h"
#include "MasterLink.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

#include <cstdio>

namespace {

/** true = puntos P1/P2/Reset bloqueados (calib en modulo fisico). Quitar al reactivar UART. */
constexpr bool kSensorCalibUiBlocked = true;

ParamId calibId = ParamId::Ph;
lv_obj_t *infoLbl = nullptr;

const char *paramKey(ParamId id) {
    switch (id) {
    case ParamId::Ph:
        return "ph";
    case ParamId::Ec:
        return "ec";
    case ParamId::TempAgua:
        return "temp_agua";
    case ParamId::Orp:
        return "orp";
    case ParamId::Do:
        return "do";
    default:
        return "ph";
    }
}

void onBack(lv_event_t *) { NavShell::back(); }

void applyPoint(float point) {
    if (kSensorCalibUiBlocked) {
        return;
    }
    DataStore &store = DataStore::instance();
    ParamConfig cfg = store.config(calibId);
    const float measured = store.value(calibId);
    /* Offset simple: desplaza lectura hacia el punto patrón. */
    cfg.calibOffset += (point - measured);
    store.setConfig(calibId, cfg);
    MasterLink::sendCalib(paramKey(calibId), point);
    if (infoLbl) {
        char buf[64];
        snprintf(buf, sizeof(buf), Strings::tr(Msg::CalibDoneFmt), point);
        lv_label_set_text(infoLbl, buf);
    }
}

void onP1(lv_event_t *) {
    switch (calibId) {
    case ParamId::Ph:
        applyPoint(4.0f);
        break;
    case ParamId::Ec:
        applyPoint(1413.0f);
        break;
    case ParamId::Orp:
        applyPoint(220.0f);
        break;
    case ParamId::Do:
        applyPoint(0.0f);
        break;
    default:
        applyPoint(25.0f);
        break;
    }
}

void onP2(lv_event_t *) {
    switch (calibId) {
    case ParamId::Ph:
        applyPoint(7.0f);
        break;
    case ParamId::Ec:
        applyPoint(84.0f);
        break;
    case ParamId::Orp:
        applyPoint(468.0f);
        break;
    case ParamId::Do:
        applyPoint(8.26f);
        break;
    default:
        applyPoint(20.0f);
        break;
    }
}

void onReset(lv_event_t *) {
    if (kSensorCalibUiBlocked) {
        return;
    }
    DataStore &store = DataStore::instance();
    ParamConfig cfg = store.config(calibId);
    cfg.calibOffset = 0.0f;
    cfg.calibScale = 1.0f;
    store.setConfig(calibId, cfg);
    if (infoLbl) {
        lv_label_set_text(infoLbl, Strings::tr(Msg::CalibResetOk));
    }
}

}  // namespace

lv_obj_t *Screens::createCalib(lv_obj_t *parent, ParamId id) {
    calibId = id;
    DataStore &store = DataStore::instance();

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);

    char tbuf[40];
    snprintf(tbuf, sizeof(tbuf), Strings::tr(Msg::CalibrateFmt), store.name(id));
    UiKit::styleHeader(root, tbuf, onBack);

    infoLbl = lv_label_create(root);
    lv_label_set_text(infoLbl, Strings::tr(Msg::CalibImmerse));
    lv_obj_set_style_text_color(infoLbl, AppTheme::muted(), 0);
    lv_obj_set_width(infoLbl, LCD_H_RES - 24);
    lv_label_set_long_mode(infoLbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(infoLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(infoLbl, LV_ALIGN_TOP_MID, 0, AppTheme::MENU_LIST_TOP);

    const char *p1 = Strings::tr(Msg::CalibPointPh4);
    const char *p2 = Strings::tr(Msg::CalibPointPh7);
    if (id == ParamId::Ec) {
        p1 = Strings::tr(Msg::CalibPointEc1413);
        p2 = Strings::tr(Msg::CalibPointEc84);
    } else if (id == ParamId::TempAgua) {
        p1 = Strings::tr(Msg::CalibPointTemp25);
        p2 = Strings::tr(Msg::CalibPointTemp20);
    } else if (id == ParamId::Orp) {
        p1 = Strings::tr(Msg::CalibPointOrp220);
        p2 = Strings::tr(Msg::CalibPointOrp468);
    } else if (id == ParamId::Do) {
        p1 = Strings::tr(Msg::CalibPointDoZero);
        p2 = Strings::tr(Msg::CalibPointDoAir);
    }

    lv_obj_t *b1 = UiKit::makeSecondaryButton(root, p1, LCD_H_RES - 40, AppTheme::BTN_PRIMARY_H, onP1);
    lv_obj_align_to(b1, infoLbl, LV_ALIGN_OUT_BOTTOM_MID, 0, 12);

    lv_obj_t *b2 = UiKit::makeSecondaryButton(root, p2, LCD_H_RES - 40, AppTheme::BTN_PRIMARY_H, onP2);
    lv_obj_align_to(b2, b1, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);

    lv_obj_t *rst = UiKit::makeCautionButton(root, Strings::tr(Msg::CalibResetBtn), onReset);
    lv_obj_align(rst, LV_ALIGN_BOTTOM_MID, 0, -6);

    if (kSensorCalibUiBlocked) {
        lv_obj_add_state(b1, LV_STATE_DISABLED);
        lv_obj_add_state(b2, LV_STATE_DISABLED);
        lv_obj_add_state(rst, LV_STATE_DISABLED);

        const lv_coord_t veilTop = AppTheme::MENU_LIST_TOP;
        lv_obj_t *veil = lv_obj_create(root);
        lv_obj_remove_style_all(veil);
        lv_obj_set_size(veil, LCD_H_RES, LCD_V_RES - veilTop);
        lv_obj_align(veil, LV_ALIGN_TOP_LEFT, 0, veilTop);
        lv_obj_set_style_bg_color(veil, AppTheme::bg(), 0);
        lv_obj_set_style_bg_opa(veil, LV_OPA_70, 0);
        lv_obj_add_flag(veil, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(veil, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_move_foreground(veil);

        lv_obj_t *hint = lv_label_create(veil);
        lv_label_set_text(hint, Strings::tr(Msg::CalibHwModuleHint));
        lv_obj_set_style_text_color(hint, AppTheme::text(), 0);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);
        lv_obj_set_width(hint, LCD_H_RES - 40);
        lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(hint, LV_ALIGN_CENTER, 0, 0);
    }

    return root;
}

void Screens::refreshCalib(lv_obj_t *root) { (void)root; }
