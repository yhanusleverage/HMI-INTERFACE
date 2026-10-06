#include "Screens.h"
#include "NavShell.h"
#include "MasterLink.h"
#include "MasterWifiDraft.h"
#include "WifiConfig.h"
#include "ui/WifiIntroLayout.h"

/**
 * WiFi unificado: wizard 4/5 + Ajuste — mismo WifiIntroLayout (solo red).
 * Perfil cloud (email/nombre/location) → wizard 5/5 MasterWifiProfile.
 * Wizard: 4/5 solo draft; commitProvision (UART+NVS Master) en 5/5.
 * Ajuste: commit inmediato (solo red).
 */

namespace {

void wizardOnContinue(const char *ssid, const char *pass) {
    /* Solo draft: UART wifi_config + reboot Master al cerrar 5/5 (perfil incluido). */
    MasterWifiDraft::setNetwork(ssid, pass ? pass : "");
    NavShell::wizardContinue();
}

void wizardOnSkip() {
    MasterWifiDraft::clearNetwork();
    NavShell::wizardContinue();
}

void settingsOnContinue(const char *ssid, const char *pass) {
    MasterWifiDraft::setNetwork(ssid, pass ? pass : "");
    MasterWifiDraft::setProfile(nullptr, nullptr, nullptr);
    (void)MasterWifiDraft::commitProvision();
    NavShell::back();
}

void settingsOnBack() { NavShell::back(); }

}  // namespace

lv_obj_t *Screens::createMasterWifi(lv_obj_t *parent) {
    MasterLink::requestSysInfo();
    WifiIntroConfig cfg = {};
    if (NavShell::inWizard()) {
        cfg.stepNum = NavShell::wizardSetupStep();
        cfg.stepTotal = NavShell::wizardSetupTotal();
        cfg.settingsMode = false;
        cfg.callbacks.onContinue = wizardOnContinue;
        cfg.callbacks.onSkip = wizardOnSkip;
        if (MasterWifiDraft::hasNetwork()) {
            cfg.initialSsid = MasterWifiDraft::ssid();
            cfg.initialPass = MasterWifiDraft::pass();
        }
    } else {
        MasterLink::clearWifiConfigAck();
        cfg.settingsMode = true;
        cfg.stepTotal = 0;
        cfg.callbacks.onContinue = settingsOnContinue;
        cfg.callbacks.onSkip = settingsOnBack;
        cfg.callbacks.onBack = settingsOnBack;
        if (MasterWifiDraft::hasNetwork()) {
            cfg.initialSsid = MasterWifiDraft::ssid();
            cfg.initialPass = MasterWifiDraft::pass();
        }
    }
    return WifiIntroLayout::create(parent, cfg);
}

void Screens::refreshMasterWifi(lv_obj_t *root) {
    if (MasterWifiDraft::hasNetwork()) {
        WifiIntroLayout::applyPrefill(MasterWifiDraft::ssid(), MasterWifiDraft::pass());
    }
    WifiIntroLayout::refresh(root);
    if (NavShell::inWizard()) {
        return;
    }
    const uint8_t ack = MasterLink::wifiConfigAckState();
    if (ack == 1 || ack == 2) {
        MasterLink::clearWifiConfigAck();
    }
}
