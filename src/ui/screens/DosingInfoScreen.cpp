#include "Screens.h"
#include "NavShell.h"
#include "MasterLink.h"
#include "NutrientConfig.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr lv_coord_t kTopY = AppTheme::HEADER_H;
constexpr lv_coord_t kPad = 10;
constexpr lv_coord_t kRangeH = 64;
constexpr lv_coord_t kTotH = 44;
constexpr lv_coord_t kNutRowH = 40;

lv_obj_t *ecLbl_ = nullptr;
lv_obj_t *phLbl_ = nullptr;
lv_obj_t *ecDot_ = nullptr;
lv_obj_t *phDot_ = nullptr;
lv_obj_t *ecGlass_ = nullptr;
lv_obj_t *phGlass_ = nullptr;
lv_obj_t *ecMl_ = nullptr;
lv_obj_t *phMl_ = nullptr;
lv_obj_t *totLbl_ = nullptr;

constexpr lv_coord_t kDot = 14;

static lv_point_t kGlassPts[] = {
    {1, 1}, {12, 1}, {6, 6}, {12, 12}, {1, 12}, {6, 6}, {1, 1},
};

void paintDoseDot(lv_obj_t *dot, bool on) {
    if (!dot) {
        return;
    }
    if (!on) {
        lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_HIDDEN);
    const bool bright = ((lv_tick_get() / 480u) % 2u) == 0u;
    lv_obj_set_style_bg_color(dot, lv_color_hex(0x3DDC84), 0);
    lv_obj_set_style_bg_opa(dot, bright ? LV_OPA_COVER : LV_OPA_40, 0);
}

lv_obj_t *makeHourglass(lv_obj_t *parent) {
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, kDot, kDot);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *line = lv_line_create(box);
    lv_line_set_points(line, kGlassPts, sizeof(kGlassPts) / sizeof(kGlassPts[0]));
    lv_obj_set_style_line_width(line, 2, 0);
    lv_obj_set_style_line_color(line, AppTheme::warn(), 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *sand = lv_obj_create(box);
    lv_obj_remove_style_all(sand);
    lv_obj_set_size(sand, 3, 3);
    lv_obj_set_style_radius(sand, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(sand, AppTheme::warn(), 0);
    lv_obj_set_style_bg_opa(sand, LV_OPA_COVER, 0);
    lv_obj_set_pos(sand, 5, 2);
    lv_obj_clear_flag(sand, LV_OBJ_FLAG_SCROLLABLE);
    return box;
}

void paintHourglass(lv_obj_t *glass, bool on) {
    if (!glass) {
        return;
    }
    if (!on) {
        lv_obj_add_flag(glass, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(glass, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *line = lv_obj_get_child(glass, 0);
    lv_obj_t *sand = lv_obj_get_child(glass, 1);
    const bool bright = ((lv_tick_get() / 480u) % 2u) == 0u;
    if (line) {
        lv_obj_set_style_line_opa(line, bright ? LV_OPA_COVER : LV_OPA_60, 0);
    }
    if (sand) {
        const uint8_t step = static_cast<uint8_t>((lv_tick_get() / 420u) % 3u);
        const lv_coord_t y = step == 0 ? 2 : (step == 1 ? 5 : 9);
        lv_obj_set_pos(sand, 5, y);
        lv_obj_set_style_bg_opa(sand, bright ? LV_OPA_COVER : LV_OPA_70, 0);
    }
}

void paintPhase(lv_obj_t *dot, lv_obj_t *glass, bool dosing, bool homo) {
    paintDoseDot(dot, dosing);
    paintHourglass(glass, homo && !dosing);
}

void paintDoseMl(lv_obj_t *lbl, bool show, float ml);
lv_obj_t *listHost_ = nullptr;
lv_coord_t listContentW_ = 0;

void onBack(lv_event_t *) { NavShell::back(); }

void rebuildNutrientList() {
    if (!listHost_) {
        return;
    }
    lv_obj_clean(listHost_);

    char line[64];
    size_t nutN = 0;
    const size_t n = NutrientConfig::listCount();
    lv_coord_t y = 0;

    for (size_t i = 0; i < n; ++i) {
        const char *nm = NutrientConfig::listName(i);
        if (!nm || nm[0] == '\0' || NutrientConfig::listMlPerL(i) <= 0.0f) {
            continue;
        }
        ++nutN;

        lv_obj_t *row = lv_obj_create(listHost_);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, listContentW_, kNutRowH);
        lv_obj_set_pos(row, 0, y);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *nameLbl = lv_label_create(row);
        lv_label_set_text(nameLbl, nm);
        lv_obj_set_style_text_color(nameLbl, AppTheme::text(), 0);
        lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_20, 0);
        lv_obj_set_width(nameLbl, listContentW_ / 2);
        lv_label_set_long_mode(nameLbl, LV_LABEL_LONG_CLIP);
        lv_obj_align(nameLbl, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *mlLbl = lv_label_create(row);
        snprintf(line, sizeof(line), "%.1f ml/L",
                 static_cast<double>(NutrientConfig::listMlPerL(i)));
        lv_label_set_text(mlLbl, line);
        lv_obj_set_style_text_color(mlLbl, AppTheme::text(), 0);
        lv_obj_set_style_text_font(mlLbl, &lv_font_montserrat_20, 0);
        lv_obj_align(mlLbl, LV_ALIGN_RIGHT_MID, 0, 0);

        y += kNutRowH;
    }

    if (nutN == 0) {
        lv_obj_t *empty = lv_label_create(listHost_);
        lv_label_set_text(empty, Strings::tr(Msg::NutEmptyHint));
        lv_obj_set_style_text_color(empty, AppTheme::muted(), 0);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_16, 0);
        lv_obj_set_width(empty, listContentW_);
        lv_label_set_long_mode(empty, LV_LABEL_LONG_WRAP);
        lv_obj_set_pos(empty, 0, 8);
    }
}

void syncDosingInfoLabels() {
    char line[64];
    if (ecLbl_) {
        snprintf(line, sizeof(line), Strings::tr(Msg::DosingInfoEcRange), NutrientConfig::ecLow(),
                 NutrientConfig::ecHigh());
        const char *cur = lv_label_get_text(ecLbl_);
        if (!cur || strcmp(cur, line) != 0) {
            lv_label_set_text(ecLbl_, line);
        }
    }
    if (phLbl_) {
        snprintf(line, sizeof(line), Strings::tr(Msg::DosingInfoPhRange), NutrientConfig::phLow(),
                 NutrientConfig::phHigh());
        const char *cur = lv_label_get_text(phLbl_);
        if (!cur || strcmp(cur, line) != 0) {
            lv_label_set_text(phLbl_, line);
        }
    }
    if (totLbl_) {
        snprintf(line, sizeof(line), Strings::tr(Msg::DosingInfoTotalFmt),
                 static_cast<double>(NutrientConfig::totalMlPerL()));
        const char *cur = lv_label_get_text(totLbl_);
        if (!cur || strcmp(cur, line) != 0) {
            lv_label_set_text(totLbl_, line);
        }
    }
    rebuildNutrientList();
    paintPhase(ecDot_, ecGlass_, MasterLink::autoDosingEc(), MasterLink::autoHomoEc());
    paintPhase(phDot_, phGlass_, MasterLink::autoDosingPh(), MasterLink::autoHomoPh());
    paintDoseMl(ecMl_, MasterLink::hasDoseMlEc(), MasterLink::doseMlEc());
    paintDoseMl(phMl_, MasterLink::hasDoseMlPh(), MasterLink::doseMlPh());
}

void paintDoseMl(lv_obj_t *lbl, bool show, float ml) {
    if (!lbl) {
        return;
    }
    if (!show) {
        lv_obj_add_flag(lbl, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    char buf[24];
    snprintf(buf, sizeof(buf), "%.1f ml", static_cast<double>(ml));
    const char *cur = lv_label_get_text(lbl);
    if (!cur || strcmp(cur, buf) != 0) {
        lv_label_set_text(lbl, buf);
    }
    lv_obj_clear_flag(lbl, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace

lv_obj_t *Screens::createDosingInfo(lv_obj_t *parent) {
    ecLbl_ = nullptr;
    phLbl_ = nullptr;
    ecDot_ = nullptr;
    phDot_ = nullptr;
    ecGlass_ = nullptr;
    phGlass_ = nullptr;
    ecMl_ = nullptr;
    phMl_ = nullptr;
    totLbl_ = nullptr;
    listHost_ = nullptr;

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root, Strings::tr(Msg::DosingInfoTitle), onBack);

    const lv_coord_t panelW = LCD_H_RES;
    const lv_coord_t panelH = LCD_V_RES - kTopY;
    lv_obj_t *panel = lv_obj_create(root);
    lv_obj_remove_style_all(panel);
    UiKit::forceOpaqueBg(panel, AppTheme::bg());
    lv_obj_set_size(panel, panelW, panelH);
    lv_obj_align(panel, LV_ALIGN_TOP_LEFT, 0, kTopY);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    listContentW_ = panelW - 2 * kPad;
    const lv_coord_t halfW = (listContentW_ - 8) / 2;

    lv_obj_t *rangeRow = lv_obj_create(panel);
    lv_obj_remove_style_all(rangeRow);
    lv_obj_set_size(rangeRow, listContentW_, kRangeH);
    lv_obj_set_pos(rangeRow, kPad, 4);
    lv_obj_set_style_bg_opa(rangeRow, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(rangeRow, LV_OBJ_FLAG_SCROLLABLE);

    ecDot_ = lv_obj_create(rangeRow);
    lv_obj_remove_style_all(ecDot_);
    lv_obj_set_size(ecDot_, kDot, kDot);
    lv_obj_set_style_radius(ecDot_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ecDot_, lv_color_hex(0x3DDC84), 0);
    lv_obj_set_style_bg_opa(ecDot_, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(ecDot_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(ecDot_, LV_ALIGN_TOP_LEFT, 0, 6);

    ecGlass_ = makeHourglass(rangeRow);
    lv_obj_align(ecGlass_, LV_ALIGN_TOP_LEFT, 0, 6);

    ecLbl_ = lv_label_create(rangeRow);
    lv_obj_set_style_text_color(ecLbl_, AppTheme::warn(), 0);
    lv_obj_set_style_text_font(ecLbl_, &lv_font_montserrat_20, 0);
    lv_obj_set_width(ecLbl_, halfW - kDot - 8);
    lv_label_set_long_mode(ecLbl_, LV_LABEL_LONG_CLIP);
    lv_obj_align(ecLbl_, LV_ALIGN_TOP_LEFT, kDot + 8, 2);

    ecMl_ = lv_label_create(rangeRow);
    lv_obj_set_style_text_color(ecMl_, lv_color_hex(0x3DDC84), 0);
    lv_obj_set_style_text_font(ecMl_, &lv_font_montserrat_16, 0);
    lv_obj_align(ecMl_, LV_ALIGN_TOP_LEFT, kDot + 8, 30);
    lv_obj_add_flag(ecMl_, LV_OBJ_FLAG_HIDDEN);

    phDot_ = lv_obj_create(rangeRow);
    lv_obj_remove_style_all(phDot_);
    lv_obj_set_size(phDot_, kDot, kDot);
    lv_obj_set_style_radius(phDot_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(phDot_, lv_color_hex(0x3DDC84), 0);
    lv_obj_set_style_bg_opa(phDot_, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(phDot_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(phDot_, LV_ALIGN_TOP_RIGHT, 0, 6);

    phGlass_ = makeHourglass(rangeRow);
    lv_obj_align(phGlass_, LV_ALIGN_TOP_RIGHT, 0, 6);

    phLbl_ = lv_label_create(rangeRow);
    lv_obj_set_style_text_color(phLbl_, AppTheme::warn(), 0);
    lv_obj_set_style_text_font(phLbl_, &lv_font_montserrat_20, 0);
    lv_obj_set_width(phLbl_, halfW - kDot - 8);
    lv_label_set_long_mode(phLbl_, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(phLbl_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(phLbl_, LV_ALIGN_TOP_RIGHT, -(kDot + 8), 2);

    phMl_ = lv_label_create(rangeRow);
    lv_obj_set_style_text_color(phMl_, lv_color_hex(0x3DDC84), 0);
    lv_obj_set_style_text_font(phMl_, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(phMl_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(phMl_, LV_ALIGN_TOP_RIGHT, -(kDot + 8), 30);
    lv_obj_add_flag(phMl_, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *totRow = lv_obj_create(panel);
    lv_obj_remove_style_all(totRow);
    lv_obj_set_size(totRow, listContentW_, kTotH);
    lv_obj_align(totRow, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_opa(totRow, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(totRow, LV_OBJ_FLAG_SCROLLABLE);

    totLbl_ = lv_label_create(totRow);
    lv_obj_set_style_text_color(totLbl_, AppTheme::accent(), 0);
    lv_obj_set_style_text_font(totLbl_, &lv_font_montserrat_20, 0);
    lv_obj_align(totLbl_, LV_ALIGN_LEFT_MID, 0, 0);

    const lv_coord_t listTop = 4 + kRangeH + 4;
    const lv_coord_t listH = panelH - listTop - kTotH - 8;
    listHost_ = lv_obj_create(panel);
    lv_obj_remove_style_all(listHost_);
    lv_obj_set_size(listHost_, listContentW_, listH > 40 ? listH : 40);
    lv_obj_set_pos(listHost_, kPad, listTop);
    lv_obj_set_style_bg_opa(listHost_, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(listHost_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(listHost_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(listHost_, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_bottom(listHost_, 8, 0);

    syncDosingInfoLabels();
    return root;
}

void Screens::refreshDosingInfo(lv_obj_t *root) {
    (void)root;
    syncDosingInfoLabels();
}
