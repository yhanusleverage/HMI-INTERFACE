#include "Screens.h"
#include "NavShell.h"
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
constexpr lv_coord_t kRangeH = 44;
constexpr lv_coord_t kTotH = 44;
constexpr lv_coord_t kNutRowH = 40;

lv_obj_t *ecLbl_ = nullptr;
lv_obj_t *phLbl_ = nullptr;
lv_obj_t *totLbl_ = nullptr;
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
}

}  // namespace

lv_obj_t *Screens::createDosingInfo(lv_obj_t *parent) {
    ecLbl_ = nullptr;
    phLbl_ = nullptr;
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

    ecLbl_ = lv_label_create(rangeRow);
    lv_obj_set_style_text_color(ecLbl_, AppTheme::warn(), 0);
    lv_obj_set_style_text_font(ecLbl_, &lv_font_montserrat_20, 0);
    lv_obj_set_width(ecLbl_, halfW);
    lv_label_set_long_mode(ecLbl_, LV_LABEL_LONG_CLIP);
    lv_obj_align(ecLbl_, LV_ALIGN_LEFT_MID, 0, 0);

    phLbl_ = lv_label_create(rangeRow);
    lv_obj_set_style_text_color(phLbl_, AppTheme::warn(), 0);
    lv_obj_set_style_text_font(phLbl_, &lv_font_montserrat_20, 0);
    lv_obj_set_width(phLbl_, halfW);
    lv_label_set_long_mode(phLbl_, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(phLbl_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(phLbl_, LV_ALIGN_RIGHT_MID, 0, 0);

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
