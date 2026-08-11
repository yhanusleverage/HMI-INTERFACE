#ifndef NAV_SHELL_H
#define NAV_SHELL_H

#include <lvgl.h>
#include "DataStore.h"
#include "DoseChannel.h"

enum class ScreenId : uint8_t {
    Central = 0,
    Menu,
    WifiSetup,
    CalibList,
    ParamDetail,
    Calib,
    Niveles,
    Settings,
    Language,
    System,
    DosingHub,
    Dosing,
    DosingChannel,
    PumpActions,
    PumpName,
    PumpPrime,
    PumpCalib,
    PumpTimeDose,
    PumpQuantity,
    TimeZone,
    Welcome,
    Ready,
    FactoryReset,
    Nutrients,
    DosingInfo,
    DisplayReadings,
    Setup,
    Sensors,
    Rules, /* legacy UI; no menú — ver Controle */
    Controle,
    ControleAuto,
    Units,
    Reservoir,
    Backlight,
    RelayNames, /* legacy → RelayOnOff */
    PhRelay,
    RelaysHub,
    RelayCycle,
    RelayTimer,
    RelayActions,
    RelayActuation,
    RelayName,
    RelayOnOff,
    MasterWifi,
    MasterWifiProfile
};

namespace NavShell {
void begin();
void goTo(ScreenId id, ParamId param = ParamId::Ph);
void goToParam(ParamId param, bool editable);
void goToDose(DoseChannel channel);
/** Hub pH Up/Down: Relé + mismas acciones que bomba. */
void goToPhPumpActions(bool isUp);
/** Asignación de relé pH Up (true) / pH Down (false). */
void goToPhRelay(bool isUp);
/** Tras elegir relé en PhRelay: abre PumpActions sin apilar otra pantalla. */
void completePhRelayPick(bool isUp);
/** Navega a pantalla de bomba con DoseChannel concreto. */
void goToDoseScreen(ScreenId id, DoseChannel channel);
/** Atlas: lista → hub del relé 0..7 (resuelve MAC ESP-NOW o placeholder). */
void goToAtlasRelay(uint8_t relayIndex0to7);
/** Pantalla Atlas reutilizando mac+relé actuales. */
void goToAtlasScreen(ScreenId id);
void back();
void tick();
/** Recrea la pantalla activa (tras cambio de idioma). */
void reloadUi();
/** Wizard: Welcome → Language → Reservoir → TimeZone → MasterWifi → MasterWifiProfile → Ready → Central. */
void wizardContinue();
void finishWizard();
bool inWizard();
/** Paso de setup 1..wizardSetupTotal(); 0 si no aplica. */
int wizardSetupStep();
int wizardSetupTotal();
ScreenId current();
ParamId currentParam();
DoseChannel currentDose();
bool paramEditable();
/** Dirección de PhRelay: true = Up. */
bool phRelayIsUp();
/** Bomba actual es pH Up o pH Down asignada. */
bool isPhPumpChannel(DoseChannel ch);
/** true si ch es la bomba pH Up (false = pH Down). */
bool phPumpIsUp(DoseChannel ch);
const char *currentAtlasMac();
uint8_t currentAtlasRelay();
}

#endif
