#include "Screens.h"
#include "NavShell.h"
#include "MasterLink.h"
#include "SlaveInventory.h"
#include "RelayAliasConfig.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"

#include <cstdio>
#include <cstring>

/**
 * Relés L2: Atlas (ESP-NOW) primero → RelayActions; bombas Master abajo (relay_local).
 * Actualizar → slaves_req. Cortina Atlas si offline (velo hoy no bloquea Master).
 */

namespace {

void onBack(lv_event_t *) { NavShell::back(); }

void onRefresh(lv_event_t *) { MasterLink::requestSlaves(); }

void onPick(lv_event_t *e) {
    const uint8_t r =
        static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    NavShell::goToAtlasRelay(r);
}

void onPickMaster(lv_event_t *e) {
    const uint8_t r =
        static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    NavShell::goToMasterLocalRelay(r);
}

lv_obj_t *relayTitleLbls_[SlaveInventory::kMaxRelays] = {};
lv_obj_t *relayHintLbls_[SlaveInventory::kMaxRelays] = {};
lv_obj_t *masterTitleLbls_[SlaveInventory::kMaxRelays] = {};
lv_obj_t *veil_ = nullptr;
lv_obj_t *veilHint_ = nullptr;
lv_obj_t *masterSection_ = nullptr;

void resolveAtlasMac(char *buf, size_t n) {
    if (!buf || n == 0) {
        return;
    }
    buf[0] = '\0';
    const size_t ix0 = SlaveInventory::firstEspNowIndex();
    if (ix0 != SIZE_MAX) {
        const SlaveInventory::Target *t = SlaveInventory::at(ix0);
        if (t && t->mac[0]) {
            RelayAliasConfig::migratePlaceholderTo(t->mac);
            strncpy(buf, t->mac, n - 1);
            buf[n - 1] = '\0';
            return;
        }
    }
    strncpy(buf, RelayAliasConfig::kPlaceholderMac, n - 1);
    buf[n - 1] = '\0';
}

bool atlasHubBlocked() {
    char mac[SlaveInventory::kMacLen];
    resolveAtlasMac(mac, sizeof(mac));
    if (strcmp(mac, RelayAliasConfig::kPlaceholderMac) == 0 || strcmp(mac, "local") == 0 ||
        mac[0] == '\0') {
        return true;
    }
    const size_t ix0 = SlaveInventory::firstEspNowIndex();
    if (ix0 == SIZE_MAX) {
        return true;
    }
    const SlaveInventory::Target *t = SlaveInventory::at(ix0);
    return !t || !t->online;
}

void applyAtlasVeil() {
    /* Master local debe quedar usable; no velo a pantalla completa. */
    if (veil_) {
        lv_obj_add_flag(veil_, LV_OBJ_FLAG_HIDDEN);
    }
}

void syncRelaysHubRows() {
    char mac[SlaveInventory::kMacLen];
    resolveAtlasMac(mac, sizeof(mac));
    char tag[24];
    for (uint8_t r = 0; r < SlaveInventory::kMaxRelays; ++r) {
        if (!relayTitleLbls_[r]) {
            continue;
        }
        snprintf(tag, sizeof(tag), Strings::tr(Msg::AtlasRelayFmt), static_cast<int>(r + 1));
        const char *alias = RelayAliasConfig::getName(mac, r);
        const char *title = (alias && alias[0]) ? alias : tag;
        char hintBuf[48];
        hintBuf[0] = '\0';
        if (SlaveInventory::isRelayLocked(mac, r)) {
            const char *lab = SlaveInventory::relayLockLabel(mac, r);
            if (lab && lab[0]) {
                snprintf(hintBuf, sizeof(hintBuf), "%s", lab);
            } else {
                snprintf(hintBuf, sizeof(hintBuf), "%s", Strings::tr(Msg::RelaysLockByRule));
            }
        } else if (alias && alias[0]) {
            snprintf(hintBuf, sizeof(hintBuf), "%s", tag);
        }
        lv_label_set_text(relayTitleLbls_[r], title);
        if (relayHintLbls_[r]) {
            lv_label_set_text(relayHintLbls_[r], hintBuf);
        }
    }
    for (uint8_t r = 0; r < SlaveInventory::kMaxRelays; ++r) {
        if (!masterTitleLbls_[r]) {
            continue;
        }
        snprintf(tag, sizeof(tag), "R%d", static_cast<int>(r + 1));
        lv_label_set_text(masterTitleLbls_[r], tag);
    }
}

}  // namespace

lv_obj_t *Screens::createRelaysHub(lv_obj_t *parent) {
    for (uint8_t r = 0; r < SlaveInventory::kMaxRelays; ++r) {
        relayTitleLbls_[r] = nullptr;
        relayHintLbls_[r] = nullptr;
        masterTitleLbls_[r] = nullptr;
    }
    veil_ = nullptr;
    veilHint_ = nullptr;
    masterSection_ = nullptr;

    lv_obj_t *root = lv_obj_create(parent);
    UiKit::styleScreen(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    UiKit::styleHeader(root, Strings::tr(Msg::RelaysTitle), onBack, 108);
    lv_obj_t *ref =
        UiKit::makeSecondaryButton(root, Strings::tr(Msg::RulesRefresh), 100, AppTheme::BACK_H,
                                   onRefresh);
    lv_obj_align(ref, LV_ALIGN_TOP_RIGHT, -4, AppTheme::PAD - 4);

    lv_obj_t *hint = lv_label_create(root);
    lv_label_set_text(hint, Strings::tr(Msg::AtlasListHint));
    UiKit::styleHint(hint);
    lv_obj_set_width(hint, LCD_H_RES - 24);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 12, AppTheme::MENU_LIST_TOP);
    lv_obj_update_layout(hint);
    const lv_coord_t top = AppTheme::MENU_LIST_TOP + lv_obj_get_height(hint) + 8;

    lv_obj_t *list = lv_obj_create(root);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, LCD_H_RES, LCD_V_RES - top);
    lv_obj_align(list, LV_ALIGN_TOP_LEFT, 0, top);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_bottom(list, 8, 0);

    char mac[SlaveInventory::kMacLen];
    resolveAtlasMac(mac, sizeof(mac));

    lv_coord_t y = 0;
    const lv_coord_t step = AppTheme::HUB_ROW_H + 4;
    char tag[24];

    /* Atlas primero (ESP-NOW) — relay_slave. Bombas Master abajo — relay_local. */
    lv_obj_t *atlasHdr = lv_label_create(list);
    lv_label_set_text(atlasHdr, Strings::tr(Msg::RelaysTitle));
    lv_obj_set_style_text_color(atlasHdr, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(atlasHdr, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(atlasHdr, 12, y);
    y += 22;

    for (uint8_t r = 0; r < SlaveInventory::kMaxRelays; ++r) {
        snprintf(tag, sizeof(tag), Strings::tr(Msg::AtlasRelayFmt), static_cast<int>(r + 1));
        const char *alias = RelayAliasConfig::getName(mac, r);
        const char *title = (alias && alias[0]) ? alias : tag;
        const char *rowHint = (alias && alias[0]) ? tag : "";
        lv_obj_t *row = UiKit::makeHubMenuRow(list, title, rowHint, onPick,
                                              reinterpret_cast<void *>(static_cast<uintptr_t>(r)));
        lv_obj_set_pos(row, 12, y);
        relayTitleLbls_[r] = lv_obj_get_child(row, 0);
        relayHintLbls_[r] = lv_obj_get_child(row, 1);
        y += step;
    }

    y += 8;
    lv_obj_t *masterHdr = lv_label_create(list);
    lv_label_set_text(masterHdr, Strings::tr(Msg::MasterLocalRelays));
    lv_obj_set_style_text_color(masterHdr, AppTheme::muted(), 0);
    lv_obj_set_style_text_font(masterHdr, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(masterHdr, 12, y);
    y += 22;
    masterSection_ = masterHdr;

    for (uint8_t r = 0; r < SlaveInventory::kMaxRelays; ++r) {
        snprintf(tag, sizeof(tag), "R%d", static_cast<int>(r + 1));
        lv_obj_t *row = UiKit::makeHubMenuRow(
            list, tag, "", onPickMaster, reinterpret_cast<void *>(static_cast<uintptr_t>(r)));
        lv_obj_set_pos(row, 12, y);
        masterTitleLbls_[r] = lv_obj_get_child(row, 0);
        y += step;
    }

    /* Velo solo sobre sección Atlas (Master local siempre usable). */
    const lv_coord_t veilTop = AppTheme::MENU_LIST_TOP;
    veil_ = lv_obj_create(root);
    lv_obj_remove_style_all(veil_);
    lv_obj_set_size(veil_, LCD_H_RES, LCD_V_RES - veilTop);
    lv_obj_align(veil_, LV_ALIGN_TOP_LEFT, 0, veilTop);
    lv_obj_set_style_bg_color(veil_, AppTheme::bg(), 0);
    lv_obj_set_style_bg_opa(veil_, LV_OPA_70, 0);
    lv_obj_add_flag(veil_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(veil_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(veil_, LV_OBJ_FLAG_HIDDEN);

    veilHint_ = lv_label_create(veil_);
    lv_label_set_text(veilHint_, Strings::tr(Msg::AtlasHwOfflineHint));
    lv_obj_set_style_text_color(veilHint_, AppTheme::text(), 0);
    lv_obj_set_style_text_font(veilHint_, &lv_font_montserrat_16, 0);
    lv_obj_set_width(veilHint_, LCD_H_RES - 40);
    lv_label_set_long_mode(veilHint_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(veilHint_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(veilHint_, LV_ALIGN_CENTER, 0, 0);

    applyAtlasVeil();
    MasterLink::requestSlaves();
    return root;
}

void Screens::refreshRelaysHub(lv_obj_t *root) {
    (void)root;
    syncRelaysHubRows();
    applyAtlasVeil();
}
