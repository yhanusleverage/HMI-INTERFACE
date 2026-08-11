#include "Screens.h"
#include "NavShell.h"
#include "MasterLink.h"
#include "MasterWifiDraft.h"
#include "WifiConfig.h"
#include "ui/WifiIntroLayout.h"

/**
 * WiFi unificado: wizard 4/5 + Ajuste — mismo WifiIntroLayout (solo red).
 * Perfil cloud (email/nombre/location) → wizard 5/5 MasterWifiProfile.
 */

namespace {

void wizardOnContinue(const char *ssid, const char *pass) {
    MasterWifiDraft::setNetwork(ssid, pass ? pass : "");
    (void)MasterWifiDraft::commitProvision();
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
    }
    return WifiIntroLayout::create(parent, cfg);
}

void Screens::refreshMasterWifi(lv_obj_t *root) {
    WifiIntroLayout::refresh(root);
    if (NavShell::inWizard()) {
        return;
    }
    const uint8_t ack = MasterLink::wifiConfigAckState();
    if (ack == 1 || ack == 2) {
        MasterLink::clearWifiConfigAck();
    }
}
