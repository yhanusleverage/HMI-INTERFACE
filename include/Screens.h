#ifndef SCREENS_H
#define SCREENS_H

#include <lvgl.h>
#include "DataStore.h"
#include "DoseChannel.h"

namespace Screens {
lv_obj_t *createCentral(lv_obj_t *parent);
lv_obj_t *createMenu(lv_obj_t *parent);
lv_obj_t *createWifiSetup(lv_obj_t *parent);
lv_obj_t *createCalibList(lv_obj_t *parent);
lv_obj_t *createParamDetail(lv_obj_t *parent, ParamId id);
lv_obj_t *createCalib(lv_obj_t *parent, ParamId id);
lv_obj_t *createNiveles(lv_obj_t *parent);
lv_obj_t *createSettings(lv_obj_t *parent);
lv_obj_t *createLanguage(lv_obj_t *parent);
lv_obj_t *createSystem(lv_obj_t *parent);
lv_obj_t *createDosingHub(lv_obj_t *parent);
lv_obj_t *createDosing(lv_obj_t *parent);
lv_obj_t *createDosingChannel(lv_obj_t *parent, DoseChannel channel);
lv_obj_t *createPumpActions(lv_obj_t *parent, DoseChannel channel);
lv_obj_t *createPumpName(lv_obj_t *parent, DoseChannel channel);
lv_obj_t *createPumpPrime(lv_obj_t *parent, DoseChannel channel);
lv_obj_t *createPumpCalib(lv_obj_t *parent, DoseChannel channel);
lv_obj_t *createPumpTimeDose(lv_obj_t *parent, DoseChannel channel);
lv_obj_t *createPumpQuantity(lv_obj_t *parent, DoseChannel channel);
lv_obj_t *createTimeZone(lv_obj_t *parent);
lv_obj_t *createWelcome(lv_obj_t *parent);
lv_obj_t *createReady(lv_obj_t *parent);
lv_obj_t *createFactoryReset(lv_obj_t *parent);
lv_obj_t *createNutrients(lv_obj_t *parent);
lv_obj_t *createDosingInfo(lv_obj_t *parent);
lv_obj_t *createDisplayReadings(lv_obj_t *parent);
lv_obj_t *createSetup(lv_obj_t *parent);
lv_obj_t *createSensors(lv_obj_t *parent);
lv_obj_t *createControle(lv_obj_t *parent);
lv_obj_t *createControleAuto(lv_obj_t *parent);
lv_obj_t *createUnits(lv_obj_t *parent);
lv_obj_t *createReservoir(lv_obj_t *parent);
lv_obj_t *createBacklight(lv_obj_t *parent);
lv_obj_t *createRelayNames(lv_obj_t *parent);
lv_obj_t *createPhRelay(lv_obj_t *parent);
lv_obj_t *createRelaysHub(lv_obj_t *parent);
lv_obj_t *createRelayCycle(lv_obj_t *parent);
lv_obj_t *createRelayTimer(lv_obj_t *parent);
lv_obj_t *createRelayActions(lv_obj_t *parent);
lv_obj_t *createRelayActuation(lv_obj_t *parent);
lv_obj_t *createRelayName(lv_obj_t *parent);
lv_obj_t *createRelayOnOff(lv_obj_t *parent);
lv_obj_t *createMasterWifi(lv_obj_t *parent);
lv_obj_t *createMasterWifiProfile(lv_obj_t *parent);

void refreshCentral(lv_obj_t *root);
void refreshMenu(lv_obj_t *root);
void refreshWifiSetup(lv_obj_t *root);
void refreshCalibList(lv_obj_t *root);
void refreshParamDetail(lv_obj_t *root);
void refreshCalib(lv_obj_t *root);
void refreshNiveles(lv_obj_t *root);
void refreshSettings(lv_obj_t *root);
void refreshLanguage(lv_obj_t *root);
void refreshSystem(lv_obj_t *root);
void refreshDosingHub(lv_obj_t *root);
void refreshDosing(lv_obj_t *root);
void refreshDosingChannel(lv_obj_t *root);
void refreshPumpActions(lv_obj_t *root);
void refreshPumpName(lv_obj_t *root);
void refreshPumpPrime(lv_obj_t *root);
void refreshPumpCalib(lv_obj_t *root);
void refreshPumpTimeDose(lv_obj_t *root);
void refreshPumpQuantity(lv_obj_t *root);
void refreshTimeZone(lv_obj_t *root);
void refreshWelcome(lv_obj_t *root);
void refreshReady(lv_obj_t *root);
void refreshFactoryReset(lv_obj_t *root);
void refreshNutrients(lv_obj_t *root);
void refreshDosingInfo(lv_obj_t *root);
void refreshDisplayReadings(lv_obj_t *root);
void refreshSetup(lv_obj_t *root);
void refreshSensors(lv_obj_t *root);
void refreshControle(lv_obj_t *root);
void refreshControleAuto(lv_obj_t *root);
void refreshUnits(lv_obj_t *root);
void refreshReservoir(lv_obj_t *root);
void refreshBacklight(lv_obj_t *root);
void refreshRelayNames(lv_obj_t *root);
void refreshPhRelay(lv_obj_t *root);
void refreshRelaysHub(lv_obj_t *root);
void refreshRelayCycle(lv_obj_t *root);
void refreshRelayTimer(lv_obj_t *root);
void refreshRelayActions(lv_obj_t *root);
void refreshRelayActuation(lv_obj_t *root);
void refreshRelayName(lv_obj_t *root);
void refreshRelayOnOff(lv_obj_t *root);
void refreshMasterWifi(lv_obj_t *root);
void refreshMasterWifiProfile(lv_obj_t *root);
}

#endif
