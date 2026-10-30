#include "Screens.h"
#include "NavShell.h"
#include "DataStore.h"
#include "DisplayConfig.h"
#include "UnitsConfig.h"
#include "AppLocale.h"
#include "AppStrings.h"
#include "ReservoirConfig.h"
#include "MasterLink.h"
#include "theme/AppTheme.h"
#include "theme/HmiSemantics.h"
#include "theme/UiKit.h"
#include "ui/fonts/lv_font_montserrat_num_60.h"
#include "BoardPins.h"

#include <cstdio>
#include <cstring>
#include <math.h>

/**
 * Central — landscape lógico 480×320.
 * Header: ACTIVO|INACTIVO (izq) · temp°C (centro) · HH:MM ≡ (der) + grid 2×2.
 * Flag = solo lectura de dosingArmed (armar en Controle → Auto).
 * Tap en la bandera abre enlace / deriv EC / deriv pH. Cada casilla entra al detalle.
 * Tap EC/pH → Dosing Info. Display Readings off → "--".
 */

namespace {

struct Cell {
    ParamId id;
    lv_obj_t *box;
    lv_obj_t *value;
    lv_obj_t *badge;
};

static Cell cells[4];
static lv_obj_t *tempValLbl = nullptr;
static lv_obj_t *tempUnitLbl = nullptr;
static lv_obj_t *clockLbl = nullptr;
static lv_obj_t *armBtn = nullptr;
static lv_obj_t *armLbl = nullptr;
static lv_obj_t *armDoseDot = nullptr;
static lv_obj_t *menuBtn = nullptr;
static lv_obj_t *toast = nullptr;
static lv_obj_t *slotVal[3] = {};
static bool toastOpen_ = false;

constexpr lv_coord_t kToastH = 116;

constexpr lv_coord_t kClockSlot = 92;
constexpr lv_coord_t kGap = 8;
/** Banda superior fija para nombre + badge; el valor se centra en el resto. */
constexpr lv_coord_t kNameBand = 24;

void onMenu(lv_event_t *) { NavShell::goTo(ScreenId::Settings); }

void formatSignedEc(float us, char *buf, size_t n) {
    float v = us;
    switch (UnitsConfig::ecUnit()) {
    case EcUnit::Ppm500:
        v *= 0.5f;
        break;
    case EcUnit::Ppm640:
        v *= 0.64f;
        break;
    default:
        break;
    }
    snprintf(buf, n, "%+.0f", static_cast<double>(v));
}

void paintToast() {
    if (!toast) {
        return;
    }
    if (toastOpen_) {
        lv_obj_clear_flag(toast, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(toast);
    } else {
        lv_obj_add_flag(toast, LV_OBJ_FLAG_HIDDEN);
    }
    char buf[24];
    const char *link = "--";
    if (MasterLink::wifiUserKnown()) {
        link = MasterLink::wifiUserOffline() ? Strings::tr(Msg::ArmLinkOff) : Strings::tr(Msg::ArmLinkOn);
    }
    const MasterLink::DayWindow &ec = MasterLink::consumoEc();
    const MasterLink::DayWindow &ph = MasterLink::consumoPh();
    const char *vals[3];
    char ecBuf[24];
    char phBuf[24];
    vals[0] = link;
    if (ec.hasDeriv) {
        formatSignedEc(ec.deriv, ecBuf, sizeof(ecBuf));
        vals[1] = ecBuf;
    } else {
        vals[1] = "--";
    }
    if (ph.hasDeriv) {
        snprintf(phBuf, sizeof(phBuf), "%+.2f", static_cast<double>(ph.deriv));
        vals[2] = phBuf;
    } else {
        vals[2] = "--";
    }
    for (int i = 0; i < 3; ++i) {
        if (!slotVal[i]) {
            continue;
        }
        const char *cur = lv_label_get_text(slotVal[i]);
        if (!cur || strcmp(cur, vals[i]) != 0) {
            lv_label_set_text(slotVal[i], vals[i]);
        }
    }
    if (slotVal[0]) {
        const bool off = MasterLink::wifiUserKnown() && MasterLink::wifiUserOffline();
        lv_obj_set_style_text_color(slotVal[0], off ? AppTheme::alarm() : AppTheme::text(), 0);
    }
}

void onArm(lv_event_t *) {
    toastOpen_ = !toastOpen_;
    paintToast();
}

void onSlot(lv_event_t *e) {
    const auto slot = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    NavShell::goToArmSlot(slot);
}

void paintArmFlag() {
    if (!armLbl) {
        return;
    }
    const bool armed = ReservoirConfig::dosingArmed();
    const char *txt = armed ? Strings::tr(Msg::DosingActive) : Strings::tr(Msg::DosingInactive);
    const char *cur = lv_label_get_text(armLbl);
    if (!cur || strcmp(cur, txt) != 0) {
        lv_label_set_text(armLbl, txt);
    }
    lv_obj_set_style_text_color(armLbl, armed ? AppTheme::accent() : AppTheme::muted(), 0);
    if (armDoseDot) {
        const bool dosing = MasterLink::autoDosing();
        lv_obj_set_style_bg_color(armDoseDot, lv_color_hex(dosing ? 0x3DDC84 : 0xE24B4B), 0);
        lv_obj_clear_flag(armDoseDot, LV_OBJ_FLAG_HIDDEN);
    }
}

void onCellTap(lv_event_t *e) {
    const ParamId id = static_cast<ParamId>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    if (id == ParamId::Ec || id == ParamId::Ph) {
        NavShell::goTo(ScreenId::DosingInfo);
    }
}

void styleTempHeader(lv_obj_t *val, lv_obj_t *unit) {
    if (val) {
        lv_obj_set_style_text_color(val, AppTheme::text(), LV_PART_MAIN);
        lv_obj_set_style_text_opa(val, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_text_font(val, &lv_font_montserrat_28, LV_PART_MAIN);
    }
    if (unit) {
        lv_obj_set_style_text_color(unit, AppTheme::text(), LV_PART_MAIN);
        lv_obj_set_style_text_opa(unit, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_text_font(unit, &lv_font_montserrat_28, LV_PART_MAIN);
    }
}

void styleClockHeader(lv_obj_t *clk) {
    if (!clk) {
        return;
    }
    lv_obj_set_style_text_color(clk, AppTheme::text(), LV_PART_MAIN);
    lv_obj_set_style_text_opa(clk, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_font(clk, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_width(clk, kClockSlot);
    lv_obj_set_style_text_align(clk, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(clk, LV_LABEL_LONG_CLIP);
}

lv_obj_t *makeMenuBtn(lv_obj_t *parent) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, AppTheme::TOUCH_MIN_W, AppTheme::HEADER_H);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, AppTheme::surfaceAlt(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 1, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, AppTheme::accent(), LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, onMenu, LV_EVENT_CLICKED, nullptr);

    for (int i = 0; i < 3; ++i) {
        lv_obj_t *bar = lv_obj_create(btn);
        lv_obj_remove_style_all(bar);
        lv_obj_set_size(bar, 28, 3);
        lv_obj_set_style_bg_color(bar, AppTheme::text(), 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_align(bar, LV_ALIGN_CENTER, 0, static_cast<lv_coord_t>((i - 1) * 9));
    }
    return btn;
}

void makeToast(lv_obj_t *parent) {
    toast = lv_obj_create(parent);
    lv_obj_remove_style_all(toast);
    lv_obj_set_size(toast, LCD_H_RES, kToastH);
    lv_obj_set_pos(toast, 0, AppTheme::HEADER_H);
    UiKit::forceOpaqueBg(toast, AppTheme::surface());
    lv_obj_set_style_border_width(toast, 1, 0);
    lv_obj_set_style_border_side(toast, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(toast, AppTheme::gridLine(), 0);
    lv_obj_set_style_radius(toast, 0, 0);
    lv_obj_clear_flag(toast, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(toast, LV_OBJ_FLAG_CLICKABLE);
    if (!toastOpen_) {
        lv_obj_add_flag(toast, LV_OBJ_FLAG_HIDDEN);
    }

    const Msg titles[3] = {Msg::ArmLinkTitle, Msg::ArmEcTitle, Msg::ArmPhTitle};
    const lv_coord_t slotW = LCD_H_RES / 3;
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *box = lv_obj_create(toast);
        lv_obj_remove_style_all(box);
        const lv_coord_t w = (i == 2) ? (LCD_H_RES - slotW * 2) : slotW;
        lv_obj_set_size(box, w, kToastH);
        lv_obj_set_pos(box, slotW * i, 0);
        lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
        UiKit::applyPressStyle(box, AppTheme::surface(), AppTheme::surfaceAlt(), AppTheme::surface(),
                               AppTheme::accent());
        lv_obj_add_event_cb(box, onSlot, LV_EVENT_CLICKED,
                            reinterpret_cast<void *>(static_cast<uintptr_t>(i)));

        lv_obj_t *title = lv_label_create(box);
        lv_label_set_text(title, Strings::tr(titles[i]));
        lv_obj_set_style_text_color(title, AppTheme::muted(), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
        lv_obj_set_width(title, w - 8);
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(title, LV_LABEL_LONG_CLIP);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

        slotVal[i] = lv_label_create(box);
        lv_label_set_text(slotVal[i], "--");
        lv_obj_set_style_text_color(slotVal[i], AppTheme::text(), 0);
        lv_obj_set_style_text_font(slotVal[i], &lv_font_montserrat_20, 0);
        lv_obj_set_width(slotVal[i], w - 12);
        lv_obj_set_style_text_align(slotVal[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(slotVal[i], LV_LABEL_LONG_WRAP);
        lv_obj_align(slotVal[i], LV_ALIGN_BOTTOM_MID, 0, -14);
    }
}

lv_obj_t *makeTile(lv_obj_t *parent, ParamId id, lv_coord_t x, lv_coord_t y, lv_coord_t w,
                   lv_coord_t h, Cell &out) {
    DataStore &store = DataStore::instance();

    lv_obj_t *box = lv_obj_create(parent);
    UiKit::styleMonitorCell(box);
    lv_obj_set_size(box, w, h);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_style_pad_all(box, 0, 0);

    if (id == ParamId::Ec || id == ParamId::Ph) {
        lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
        UiKit::applyPressStyle(box, AppTheme::bg(), AppTheme::surface(), AppTheme::gridLine(),
                               AppTheme::accent());
        lv_obj_add_event_cb(box, onCellTap, LV_EVENT_CLICKED,
                            reinterpret_cast<void *>(static_cast<uintptr_t>(id)));
    } else {
        lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_t *name = lv_label_create(box);
    lv_label_set_text(name, store.labelWithUnit(id));
    lv_obj_set_style_text_color(name, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_16, 0);
    lv_obj_align(name, LV_ALIGN_TOP_LEFT, 8, 4);

    lv_obj_t *badge = lv_label_create(box);
    lv_label_set_text(badge, "");
    lv_obj_set_style_text_color(badge, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(badge, &lv_font_montserrat_16, 0);
    lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, -8, 4);

    lv_obj_t *value = lv_label_create(box);
    lv_label_set_text(value, "--");
    lv_obj_set_style_text_color(value, AppTheme::text(), 0);
    /* Fuente bitmap real 60 (solo dígitos). Sin transform_zoom — clip/desaparición. */
    lv_obj_set_style_text_font(value, &lv_font_montserrat_num_60, 0);
    lv_obj_set_width(value, w - 16);
    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(value, LV_LABEL_LONG_CLIP);
    const lv_coord_t bandMid = kNameBand + (h - kNameBand) / 2;
    const lv_coord_t boxMid = h / 2;
    lv_obj_align(value, LV_ALIGN_CENTER, 0, bandMid - boxMid);

    out.id = id;
    out.box = box;
    out.value = value;
    out.badge = badge;
    return box;
}

/** Header: flag limpia izq · temp°C centro · HH:MM ≡ der (sin separador ·). */
void layoutHeader() {
    const lv_coord_t midTemp = (AppTheme::HEADER_H - 28) / 2;

    if (armBtn) {
        lv_obj_align(armBtn, LV_ALIGN_TOP_LEFT, 0, 0);
    }

    if (tempValLbl && tempUnitLbl) {
        /* Bloque temp centrado: valor + unidad como un solo eje óptico. */
        lv_obj_update_layout(tempValLbl);
        lv_obj_update_layout(tempUnitLbl);
        const lv_coord_t tw = lv_obj_get_width(tempValLbl);
        const lv_coord_t uw = lv_obj_get_width(tempUnitLbl);
        const lv_coord_t blockW = tw + 2 + uw;
        const lv_coord_t left = (LCD_H_RES - blockW) / 2;
        lv_obj_align(tempValLbl, LV_ALIGN_TOP_LEFT, left, midTemp);
        lv_obj_align_to(tempUnitLbl, tempValLbl, LV_ALIGN_OUT_RIGHT_MID, 2, 0);
    }

    if (menuBtn) {
        lv_obj_align(menuBtn, LV_ALIGN_TOP_RIGHT, -4, 0);
    }

    const lv_coord_t menuW = AppTheme::TOUCH_MIN_W;
    if (clockLbl) {
        styleClockHeader(clockLbl);
        lv_obj_align(clockLbl, LV_ALIGN_TOP_RIGHT, -(menuW + kGap), midTemp);
    }
}

}  // namespace

lv_obj_t *Screens::createCentral(lv_obj_t *parent) {
    tempValLbl = nullptr;
    tempUnitLbl = nullptr;
    clockLbl = nullptr;
    armBtn = nullptr;
    armLbl = nullptr;
    armDoseDot = nullptr;
    menuBtn = nullptr;
    toast = nullptr;
    slotVal[0] = slotVal[1] = slotVal[2] = nullptr;
    for (int i = 0; i < 4; ++i) {
        cells[i] = {};
    }

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);

    armBtn = lv_btn_create(root);
    lv_obj_remove_style_all(armBtn);
    lv_obj_set_size(armBtn, 168, AppTheme::HEADER_H);
    lv_obj_add_flag(armBtn, LV_OBJ_FLAG_CLICKABLE);
    UiKit::applyPressStyle(armBtn, AppTheme::bg(), AppTheme::surfaceAlt(), AppTheme::bg(),
                           AppTheme::accent());
    lv_obj_add_event_cb(armBtn, onArm, LV_EVENT_CLICKED, nullptr);

    armLbl = lv_label_create(armBtn);
    lv_obj_set_style_text_font(armLbl, &lv_font_montserrat_28, 0);
    lv_obj_set_width(armLbl, 128);
    lv_label_set_long_mode(armLbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(armLbl, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(armLbl, LV_ALIGN_LEFT_MID, AppTheme::PAD + 2, 0);

    armDoseDot = lv_obj_create(armBtn);
    lv_obj_remove_style_all(armDoseDot);
    lv_obj_set_size(armDoseDot, 16, 16);
    lv_obj_set_style_radius(armDoseDot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(armDoseDot, lv_color_hex(0xE24B4B), 0);
    lv_obj_set_style_bg_opa(armDoseDot, LV_OPA_COVER, 0);
    lv_obj_clear_flag(armDoseDot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(armDoseDot, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_move_foreground(armDoseDot);
    paintArmFlag();

    tempValLbl = lv_label_create(root);
    lv_label_set_text(tempValLbl, "--");
    tempUnitLbl = lv_label_create(root);
    lv_label_set_text(tempUnitLbl, "\xC2\xB0" "C");
    styleTempHeader(tempValLbl, tempUnitLbl);

    clockLbl = lv_label_create(root);
    lv_label_set_text(clockLbl, "--:--");
    styleClockHeader(clockLbl);

    menuBtn = makeMenuBtn(root);

    layoutHeader();

    const lv_coord_t top = AppTheme::HEADER_H;
    const lv_coord_t gap = 4;
    const lv_coord_t cellW = (LCD_H_RES - gap) / 2;
    const lv_coord_t cellH = (LCD_V_RES - top - gap) / 2;

    makeTile(root, ParamId::Ec, 0, top, cellW, cellH, cells[0]);
    makeTile(root, ParamId::Ph, cellW + gap, top, cellW, cellH, cells[1]);
    makeTile(root, ParamId::Orp, 0, top + cellH + gap, cellW, cellH, cells[2]);
    makeTile(root, ParamId::Do, cellW + gap, top + cellH + gap, cellW, cellH, cells[3]);

    makeToast(root);
    paintToast();

    return root;
}

void Screens::refreshCentral(lv_obj_t *root) {
    (void)root;
    DataStore &store = DataStore::instance();
    const TelemetrySnapshot snap = store.snapshot();

    paintArmFlag();
    paintToast();

    bool needLayout = false;

    if (tempValLbl && tempUnitLbl) {
        char tbuf[16];
        const char *unit = "";
        if (DisplayConfig::showTemp() && isfinite(snap.tempAgua)) {
            UnitsConfig::formatTemp(snap.tempAgua, tbuf, sizeof(tbuf));
            unit = UnitsConfig::tempUnitSuffix();
        } else {
            snprintf(tbuf, sizeof(tbuf), "--");
        }
        const char *curT = lv_label_get_text(tempValLbl);
        const char *curU = lv_label_get_text(tempUnitLbl);
        if (!curT || strcmp(curT, tbuf) != 0) {
            lv_label_set_text(tempValLbl, tbuf);
            needLayout = true;
        }
        if (!curU || strcmp(curU, unit) != 0) {
            lv_label_set_text(tempUnitLbl, unit);
            needLayout = true;
        }
    }

    if (clockLbl) {
        char cbuf[16];
        AppLocale::formatNowClock(cbuf, sizeof(cbuf));
        const char *cur = lv_label_get_text(clockLbl);
        if (!cur || strcmp(cur, cbuf) != 0) {
            lv_label_set_text(clockLbl, cbuf);
        }
    }

    if (needLayout) {
        layoutHeader();
    }

    for (int i = 0; i < 4; ++i) {
        if (!cells[i].value || !cells[i].box) {
            continue;
        }
        const ParamId id = cells[i].id;

        if (!DisplayConfig::showParam(id)) {
            const char *cur = lv_label_get_text(cells[i].value);
            if (!cur || strcmp(cur, "--") != 0) {
                lv_label_set_text(cells[i].value, "--");
            }
            lv_obj_set_style_text_color(cells[i].value, AppTheme::muted(), 0);
            lv_obj_set_style_border_width(cells[i].box, 1, 0);
            lv_obj_set_style_border_color(cells[i].box, AppTheme::gridLine(), 0);
            if (cells[i].badge) {
                const char *b = lv_label_get_text(cells[i].badge);
                if (b && b[0]) {
                    lv_label_set_text(cells[i].badge, "");
                }
            }
            continue;
        }

        char buf[32];
        const float v = store.value(id);
        if (!isfinite(v)) {
            if (!cells[i].value) {
                continue;
            }
            const char *cur = lv_label_get_text(cells[i].value);
            if (!cur || strcmp(cur, "--") != 0) {
                lv_label_set_text(cells[i].value, "--");
            }
            lv_obj_set_style_text_color(cells[i].value, AppTheme::muted(), 0);
            lv_obj_set_style_border_width(cells[i].box, 1, 0);
            lv_obj_set_style_border_color(cells[i].box, AppTheme::gridLine(), 0);
            if (cells[i].badge) {
                const char *b = lv_label_get_text(cells[i].badge);
                if (b && b[0]) {
                    lv_label_set_text(cells[i].badge, "");
                }
            }
            continue;
        }
        if (id == ParamId::Ec) {
            UnitsConfig::formatEc(v, buf, sizeof(buf));
        } else if (id == ParamId::Orp) {
            snprintf(buf, sizeof(buf), "%.0f", v);
        } else {
            snprintf(buf, sizeof(buf), "%.1f", v);
        }
        const char *curV = lv_label_get_text(cells[i].value);
        if (!curV || strcmp(curV, buf) != 0) {
            lv_label_set_text(cells[i].value, buf);
        }
        lv_obj_set_style_text_color(cells[i].value, AppTheme::text(), 0);

        const ParamStatus st = store.status(id);
        lv_obj_set_style_border_color(cells[i].box, HmiSemantics::statusBorder(st), 0);
        lv_obj_set_style_border_width(cells[i].box, HmiSemantics::statusBorderWidth(st), 0);

        if (cells[i].badge) {
            if (st == ParamStatus::Low || st == ParamStatus::High) {
                const char *stTxt = HmiSemantics::statusText(st);
                const char *curB = lv_label_get_text(cells[i].badge);
                if (!curB || strcmp(curB, stTxt) != 0) {
                    lv_label_set_text(cells[i].badge, stTxt);
                }
                lv_obj_set_style_text_color(cells[i].badge, HmiSemantics::statusLabel(st), 0);
            } else {
                const char *curB = lv_label_get_text(cells[i].badge);
                if (curB && curB[0]) {
                    lv_label_set_text(cells[i].badge, "");
                }
            }
        }
    }
}
