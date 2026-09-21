#include "NavShell.h"
#include "Screens.h"
#include "AppLocale.h"
#include "MasterLink.h"
#include "MasterWifiDraft.h"
#include "NutrientConfig.h"
#include "RelayAliasConfig.h"
#include "SlaveInventory.h"
#include "theme/AppTheme.h"
#include "theme/UiKit.h"
#include "BoardPins.h"
#include "Config.h"

#include <cstring>

namespace {

lv_obj_t *scrRoot = nullptr;
lv_obj_t *bgPlate = nullptr;
lv_obj_t *contentHost = nullptr;
lv_obj_t *active = nullptr;
ScreenId cur = ScreenId::Central;
ParamId curParam = ParamId::Ph;
DoseChannel curDose = DoseChannel::R1;
bool curParamEditable = false;
bool curPhUp_ = true;
char curAtlasMac_[SlaveInventory::kMacLen] = {};
uint8_t curAtlasRelay_ = 0;
unsigned long lastUiMs = 0;
bool wizard_ = false;

constexpr size_t STACK_MAX = 8;
ScreenId stack_[STACK_MAX];
ParamId stackParam_[STACK_MAX];
DoseChannel stackDose_[STACK_MAX];
bool stackEditable_[STACK_MAX];
bool stackPhUp_[STACK_MAX];
char stackAtlasMac_[STACK_MAX][SlaveInventory::kMacLen];
uint8_t stackAtlasRelay_[STACK_MAX];
size_t stackLen_ = 0;

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

void clearActive() {
    if (active) {
        lv_obj_del(active);
        active = nullptr;
    }
}

lv_obj_t *build(ScreenId id, ParamId param, DoseChannel dose) {
    switch (id) {
    case ScreenId::Central:
        return Screens::createCentral(contentHost);
    case ScreenId::Menu:
        return Screens::createMenu(contentHost);
    case ScreenId::WifiSetup:
        return Screens::createMasterWifi(contentHost);
    case ScreenId::CalibList:
        return Screens::createCalibList(contentHost);
    case ScreenId::ParamDetail:
        return Screens::createParamDetail(contentHost, param);
    case ScreenId::Calib:
        return Screens::createCalib(contentHost, param);
    case ScreenId::Niveles:
        return Screens::createNiveles(contentHost);
    case ScreenId::Settings:
        return Screens::createSettings(contentHost);
    case ScreenId::Language:
        return Screens::createLanguage(contentHost);
    case ScreenId::System:
        return Screens::createSystem(contentHost);
    case ScreenId::DosingHub:
        return Screens::createDosingHub(contentHost);
    case ScreenId::Dosing:
        return Screens::createDosing(contentHost);
    case ScreenId::DosingChannel:
        return Screens::createDosingChannel(contentHost, dose);
    case ScreenId::PumpActions:
        return Screens::createPumpActions(contentHost, dose);
    case ScreenId::PumpName:
        return Screens::createPumpName(contentHost, dose);
    case ScreenId::PumpPrime:
        return Screens::createPumpPrime(contentHost, dose);
    case ScreenId::PumpCalib:
        return Screens::createPumpCalib(contentHost, dose);
    case ScreenId::PumpTimeDose:
        return Screens::createPumpTimeDose(contentHost, dose);
    case ScreenId::PumpQuantity:
        return Screens::createPumpQuantity(contentHost, dose);
    case ScreenId::TimeZone:
        return Screens::createTimeZone(contentHost);
    case ScreenId::Welcome:
        return Screens::createWelcome(contentHost);
    case ScreenId::Ready:
        return Screens::createReady(contentHost);
    case ScreenId::FactoryReset:
        return Screens::createFactoryReset(contentHost);
    case ScreenId::Nutrients:
        return Screens::createNutrients(contentHost);
    case ScreenId::DosingInfo:
        return Screens::createDosingInfo(contentHost);
    case ScreenId::DisplayReadings:
        return Screens::createDisplayReadings(contentHost);
    case ScreenId::Setup:
        return Screens::createSetup(contentHost);
    case ScreenId::Sensors:
        return Screens::createSensors(contentHost);
    case ScreenId::Rules:
        return Screens::createControle(contentHost); /* legacy id → Controle */
    case ScreenId::Controle:
        return Screens::createControle(contentHost);
    case ScreenId::ControleAuto:
        return Screens::createControleAuto(contentHost);
    case ScreenId::Units:
        return Screens::createUnits(contentHost);
    case ScreenId::Reservoir:
        return Screens::createReservoir(contentHost);
    case ScreenId::Backlight:
        return Screens::createBacklight(contentHost);
    case ScreenId::RelayNames:
        return Screens::createRelayOnOff(contentHost);
    case ScreenId::PhRelay:
        return Screens::createPhRelay(contentHost);
    case ScreenId::RelaysHub:
        return Screens::createRelaysHub(contentHost);
    case ScreenId::RelayCycle:
        return Screens::createRelayCycle(contentHost);
    case ScreenId::RelayTimer:
        return Screens::createRelayTimer(contentHost);
    case ScreenId::RelayActions:
        return Screens::createRelayActions(contentHost);
    case ScreenId::RelayActuation:
        return Screens::createRelayActuation(contentHost);
    case ScreenId::RelayName:
        return Screens::createRelayName(contentHost);
    case ScreenId::RelayOnOff:
        return Screens::createRelayOnOff(contentHost);
    case ScreenId::MasterWifi:
        return Screens::createMasterWifi(contentHost);
    case ScreenId::MasterWifiProfile:
        return Screens::createMasterWifiProfile(contentHost);
    }
    return Screens::createCentral(contentHost);
}

void refreshActive() {
    if (!active) {
        return;
    }
    switch (cur) {
    case ScreenId::Central:
        Screens::refreshCentral(active);
        break;
    case ScreenId::Menu:
        Screens::refreshMenu(active);
        break;
    case ScreenId::WifiSetup:
        Screens::refreshMasterWifi(active);
        break;
    case ScreenId::CalibList:
        Screens::refreshCalibList(active);
        break;
    case ScreenId::ParamDetail:
        Screens::refreshParamDetail(active);
        break;
    case ScreenId::Calib:
        Screens::refreshCalib(active);
        break;
    case ScreenId::Niveles:
        Screens::refreshNiveles(active);
        break;
    case ScreenId::Settings:
        Screens::refreshSettings(active);
        break;
    case ScreenId::Language:
        Screens::refreshLanguage(active);
        break;
    case ScreenId::System:
        Screens::refreshSystem(active);
        break;
    case ScreenId::DosingHub:
        Screens::refreshDosingHub(active);
        break;
    case ScreenId::Dosing:
        Screens::refreshDosing(active);
        break;
    case ScreenId::DosingChannel:
        Screens::refreshDosingChannel(active);
        break;
    case ScreenId::PumpActions:
        Screens::refreshPumpActions(active);
        break;
    case ScreenId::PumpName:
        Screens::refreshPumpName(active);
        break;
    case ScreenId::PumpPrime:
        Screens::refreshPumpPrime(active);
        break;
    case ScreenId::PumpCalib:
        Screens::refreshPumpCalib(active);
        break;
    case ScreenId::PumpTimeDose:
        Screens::refreshPumpTimeDose(active);
        break;
    case ScreenId::PumpQuantity:
        Screens::refreshPumpQuantity(active);
        break;
    case ScreenId::TimeZone:
        Screens::refreshTimeZone(active);
        break;
    case ScreenId::Welcome:
        Screens::refreshWelcome(active);
        break;
    case ScreenId::Ready:
        Screens::refreshReady(active);
        break;
    case ScreenId::FactoryReset:
        Screens::refreshFactoryReset(active);
        break;
    case ScreenId::Nutrients:
        Screens::refreshNutrients(active);
        break;
    case ScreenId::DosingInfo:
        Screens::refreshDosingInfo(active);
        break;
    case ScreenId::DisplayReadings:
        Screens::refreshDisplayReadings(active);
        break;
    case ScreenId::Setup:
        Screens::refreshSetup(active);
        break;
    case ScreenId::Sensors:
        Screens::refreshSensors(active);
        break;
    case ScreenId::Rules:
    case ScreenId::Controle:
        Screens::refreshControle(active);
        break;
    case ScreenId::ControleAuto:
        Screens::refreshControleAuto(active);
        break;
    case ScreenId::Units:
        Screens::refreshUnits(active);
        break;
    case ScreenId::Reservoir:
        Screens::refreshReservoir(active);
        break;
    case ScreenId::Backlight:
        Screens::refreshBacklight(active);
        break;
    case ScreenId::RelayNames:
    case ScreenId::RelayOnOff:
        Screens::refreshRelayOnOff(active);
        break;
    case ScreenId::PhRelay:
        Screens::refreshPhRelay(active);
        break;
    case ScreenId::RelaysHub:
        Screens::refreshRelaysHub(active);
        break;
    case ScreenId::RelayCycle:
        Screens::refreshRelayCycle(active);
        break;
    case ScreenId::RelayTimer:
        Screens::refreshRelayTimer(active);
        break;
    case ScreenId::RelayActions:
        Screens::refreshRelayActions(active);
        break;
    case ScreenId::RelayActuation:
        Screens::refreshRelayActuation(active);
        break;
    case ScreenId::RelayName:
        Screens::refreshRelayName(active);
        break;
    case ScreenId::MasterWifi:
        Screens::refreshMasterWifi(active);
        break;
    case ScreenId::MasterWifiProfile:
        Screens::refreshMasterWifiProfile(active);
        break;
    }
}

void pushCurrent() {
    if (stackLen_ >= STACK_MAX) {
        for (size_t i = 1; i < STACK_MAX; ++i) {
            stack_[i - 1] = stack_[i];
            stackParam_[i - 1] = stackParam_[i];
            stackDose_[i - 1] = stackDose_[i];
            stackEditable_[i - 1] = stackEditable_[i];
            stackPhUp_[i - 1] = stackPhUp_[i];
            memcpy(stackAtlasMac_[i - 1], stackAtlasMac_[i], SlaveInventory::kMacLen);
            stackAtlasRelay_[i - 1] = stackAtlasRelay_[i];
        }
        stackLen_ = STACK_MAX - 1;
    }
    stack_[stackLen_] = cur;
    stackParam_[stackLen_] = curParam;
    stackDose_[stackLen_] = curDose;
    stackEditable_[stackLen_] = curParamEditable;
    stackPhUp_[stackLen_] = curPhUp_;
    memcpy(stackAtlasMac_[stackLen_], curAtlasMac_, SlaveInventory::kMacLen);
    stackAtlasRelay_[stackLen_] = curAtlasRelay_;
    ++stackLen_;
}

void showScreen(ScreenId id, ParamId param, DoseChannel dose, bool editable, bool push) {
    if (push && active && !(cur == ScreenId::Central && id == ScreenId::Central)) {
        pushCurrent();
    }
    clearActive();
    cur = id;
    curParam = param;
    curDose = dose;
    curParamEditable = editable;
    active = build(id, param, dose);
    refreshActive();
}

void navigate(ScreenId id, ParamId param, DoseChannel dose, bool editable) {
    showScreen(id, param, dose, editable, !wizard_);
}

void navigatePhHub(bool isUp, bool pushStack) {
    if (pushStack && !wizard_ && active && cur != ScreenId::Central) {
        pushCurrent();
    }
    curPhUp_ = isUp;
    DoseChannel ch;
    if (tryDoseFromRelayNumber(isUp ? NutrientConfig::phUpRelay() : NutrientConfig::phDownRelay(),
                               &ch)) {
        showScreen(ScreenId::PumpActions, curParam, ch, false, false);
    } else {
        showScreen(ScreenId::PhRelay, curParam, curDose, false, false);
    }
}

}  // namespace

void NavShell::begin() {
    scrRoot = lv_scr_act();
    lv_obj_remove_style_all(scrRoot);
    UiKit::forceOpaqueBg(scrRoot, AppTheme::bg());
    lv_obj_set_style_border_width(scrRoot, 0, 0);
    lv_obj_set_style_pad_all(scrRoot, 0, 0);
    lv_obj_clear_flag(scrRoot, LV_OBJ_FLAG_SCROLLABLE);

    bgPlate = lv_obj_create(scrRoot);
    lv_obj_remove_style_all(bgPlate);
    lv_obj_set_size(bgPlate, LCD_H_RES, LCD_V_RES);
    lv_obj_set_pos(bgPlate, 0, 0);
    UiKit::forceOpaqueBg(bgPlate, AppTheme::bg());
    lv_obj_set_style_border_width(bgPlate, 0, 0);
    lv_obj_set_style_radius(bgPlate, 0, 0);
    lv_obj_clear_flag(bgPlate, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(bgPlate, LV_OBJ_FLAG_CLICKABLE);

    contentHost = lv_obj_create(scrRoot);
    lv_obj_remove_style_all(contentHost);
    lv_obj_set_size(contentHost, LCD_H_RES, LCD_V_RES);
    lv_obj_set_pos(contentHost, 0, 0);
    UiKit::forceOpaqueBg(contentHost, AppTheme::bg());
    lv_obj_set_style_border_width(contentHost, 0, 0);
    lv_obj_set_style_pad_all(contentHost, 0, 0);
    lv_obj_set_style_radius(contentHost, 0, 0);
    lv_obj_clear_flag(contentHost, LV_OBJ_FLAG_SCROLLABLE);

    stackLen_ = 0;
    clearActive();
    curParam = ParamId::Ph;
    curDose = DoseChannel::R1;
    curAtlasMac_[0] = '\0';
    curAtlasRelay_ = 0;
    curParamEditable = false;

    wizard_ = !AppLocale::setupDone();
    cur = wizard_ ? ScreenId::Welcome : ScreenId::Central;
    active = build(cur, curParam, curDose);
    refreshActive();

    lv_obj_invalidate(scrRoot);
    lv_obj_invalidate(bgPlate);
    lv_obj_invalidate(contentHost);
    if (active) {
        lv_obj_invalidate(active);
    }
}

void NavShell::goTo(ScreenId id, ParamId param) {
    const bool editable = (id == ScreenId::ParamDetail) ? curParamEditable : false;
    navigate(id, param, curDose, editable);
}

void NavShell::goToParam(ParamId param, bool editable) {
    navigate(ScreenId::ParamDetail, param, curDose, editable);
}

void NavShell::goToDose(DoseChannel channel) {
    navigate(ScreenId::PumpActions, curParam, channel, false);
}

void NavShell::goToPhPumpActions(bool isUp) { navigatePhHub(isUp, true); }

void NavShell::completePhRelayPick(bool isUp) { navigatePhHub(isUp, false); }

void NavShell::goToPhRelay(bool isUp) {
    if (!wizard_ && active && cur != ScreenId::Central) {
        pushCurrent();
    }
    curPhUp_ = isUp;
    showScreen(ScreenId::PhRelay, curParam, curDose, false, false);
}

void NavShell::goToDoseScreen(ScreenId id, DoseChannel channel) {
    navigate(id, curParam, channel, false);
}

void NavShell::goToAtlasRelay(uint8_t relayIndex0to7) {
    resolveAtlasMac(curAtlasMac_, sizeof(curAtlasMac_));
    curAtlasRelay_ = (relayIndex0to7 < SlaveInventory::kMaxRelays) ? relayIndex0to7 : 0;
    navigate(ScreenId::RelayActions, curParam, curDose, false);
}

void NavShell::goToMasterLocalRelay(uint8_t relayIndex0to7) {
    strncpy(curAtlasMac_, "local", sizeof(curAtlasMac_) - 1);
    curAtlasMac_[sizeof(curAtlasMac_) - 1] = '\0';
    curAtlasRelay_ = (relayIndex0to7 < SlaveInventory::kMaxRelays) ? relayIndex0to7 : 0;
    navigate(ScreenId::RelayActions, curParam, curDose, false);
}

void NavShell::goToAtlasScreen(ScreenId id) {
    if (curAtlasMac_[0] == '\0') {
        resolveAtlasMac(curAtlasMac_, sizeof(curAtlasMac_));
    }
    navigate(id, curParam, curDose, false);
}

void NavShell::back() {
    if (wizard_) {
        return;
    }
    if (stackLen_ == 0) {
        if (cur != ScreenId::Central) {
            showScreen(ScreenId::Central, ParamId::Ph, DoseChannel::R1, false, false);
        }
        return;
    }
    --stackLen_;
    clearActive();
    cur = stack_[stackLen_];
    curParam = stackParam_[stackLen_];
    curDose = stackDose_[stackLen_];
    curParamEditable = stackEditable_[stackLen_];
    curPhUp_ = stackPhUp_[stackLen_];
    memcpy(curAtlasMac_, stackAtlasMac_[stackLen_], SlaveInventory::kMacLen);
    curAtlasRelay_ = stackAtlasRelay_[stackLen_];
    active = build(cur, curParam, curDose);
    refreshActive();
}

void NavShell::tick() {
    const unsigned long now = millis();
    if (lastUiMs != 0 && (now - lastUiMs) < UI_REFRESH_MS) {
        return;
    }
    lastUiMs = now;
    refreshActive();
}

void NavShell::reloadUi() {
    clearActive();
    active = build(cur, curParam, curDose);
    refreshActive();
}

void NavShell::wizardContinue() {
    if (!wizard_) {
        return;
    }
    if (cur == ScreenId::Welcome) {
        showScreen(ScreenId::Language, curParam, curDose, false, false);
    } else if (cur == ScreenId::Language) {
        showScreen(ScreenId::Reservoir, curParam, curDose, false, false);
    } else if (cur == ScreenId::Reservoir) {
        MasterLink::requestSysInfo();
        showScreen(ScreenId::TimeZone, curParam, curDose, false, false);
    } else if (cur == ScreenId::TimeZone) {
        MasterLink::requestSysInfo();
        showScreen(ScreenId::MasterWifi, curParam, curDose, false, false);
    } else if (cur == ScreenId::MasterWifi) {
        showScreen(ScreenId::MasterWifiProfile, curParam, curDose, false, false);
    } else if (cur == ScreenId::MasterWifiProfile) {
        showScreen(ScreenId::Ready, curParam, curDose, false, false);
    } else if (cur == ScreenId::Ready) {
        finishWizard();
    }
}

void NavShell::finishWizard() {
    wizard_ = false;
    stackLen_ = 0;
    MasterWifiDraft::clear();
    showScreen(ScreenId::Central, ParamId::Ph, DoseChannel::R1, false, false);
}

bool NavShell::inWizard() { return wizard_; }

int NavShell::wizardSetupStep() {
    if (!wizard_) {
        return 0;
    }
    switch (cur) {
    case ScreenId::Language:
        return 1;
    case ScreenId::Reservoir:
        return 2;
    case ScreenId::TimeZone:
        return 3;
    case ScreenId::MasterWifi:
        return 4;
    case ScreenId::MasterWifiProfile:
        return 5;
    default:
        return 0;
    }
}

int NavShell::wizardSetupTotal() { return 5; }

ScreenId NavShell::current() { return cur; }

ParamId NavShell::currentParam() { return curParam; }

DoseChannel NavShell::currentDose() { return curDose; }

bool NavShell::paramEditable() { return curParamEditable; }

bool NavShell::phRelayIsUp() { return curPhUp_; }

bool NavShell::isPhPumpChannel(DoseChannel ch) {
    const uint8_t r = doseRelayNumber(ch);
    return r == NutrientConfig::phUpRelay() || r == NutrientConfig::phDownRelay();
}

bool NavShell::phPumpIsUp(DoseChannel ch) {
    return doseRelayNumber(ch) == NutrientConfig::phUpRelay();
}

const char *NavShell::currentAtlasMac() { return curAtlasMac_; }

uint8_t NavShell::currentAtlasRelay() { return curAtlasRelay_; }
