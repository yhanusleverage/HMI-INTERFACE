#include "UiMirror.h"
#include "NavShell.h"
#include "DataStore.h"
#include "DoseChannel.h"
#include "DisplayHal.h"
#include "MasterLink.h"
#include "Screens.h"
#include "Config.h"

namespace {

bool autoMirror = true;
unsigned long lastDumpMs = 0;

const char *screenName(ScreenId id) {
    switch (id) {
    case ScreenId::Central:
        return "MONITORING 2x2";
    case ScreenId::Menu:
        return "SETTINGS (alias)";
    case ScreenId::WifiSetup:
        return "CLOUD / WIFI";
    case ScreenId::CalibList:
        return "CALIB LISTA";
    case ScreenId::ParamDetail:
        return "DETALLE PARAM";
    case ScreenId::Calib:
        return "CALIBRACION";
    case ScreenId::Niveles:
        return "NIVELES";
    case ScreenId::Settings:
        return "CONFIGURACION";
    case ScreenId::Language:
        return "IDIOMA";
    case ScreenId::System:
        return "SISTEMA";
    case ScreenId::DosingHub:
        return "DOSIFICACION HUB";
    case ScreenId::Dosing:
        return "DOSIFICACION MANUAL";
    case ScreenId::DosingChannel:
        return "QUICK DOSE";
    case ScreenId::PumpActions:
        return "BOMBA ACCIONES";
    case ScreenId::PumpName:
        return "BOMBA NOMBRE";
    case ScreenId::PumpPrime:
        return "BOMBA PRIME";
    case ScreenId::PumpCalib:
        return "BOMBA CALIB";
    case ScreenId::PumpTimeDose:
        return "TIME DOSE";
    case ScreenId::PumpQuantity:
        return "QUANTITY";
    case ScreenId::TimeZone:
        return "ZONA HORARIA";
    case ScreenId::Welcome:
        return "BIENVENIDA";
    case ScreenId::Ready:
        return "LISTO";
    case ScreenId::FactoryReset:
        return "RESET FABRICA";
    case ScreenId::Nutrients:
        return "NUTRIENTES";
    case ScreenId::DosingInfo:
        return "DOSING INFO";
    case ScreenId::DisplayReadings:
        return "DISPLAY READINGS";
    case ScreenId::Setup:
        return "SETUP";
    case ScreenId::Sensors:
        return "SENSORS";
    case ScreenId::Rules:
        return "REGLAS";
    case ScreenId::Controle:
        return "CONTROLE";
    case ScreenId::ControleAuto:
        return "CTRL_AUTO";
    case ScreenId::Units:
        return "UNITS";
    case ScreenId::Reservoir:
        return "RESERVOIR";
    case ScreenId::Backlight:
        return "BACKLIGHT";
    case ScreenId::RelayNames:
        return "RELAY NAMES";
    case ScreenId::PhRelay:
        return "PH RELAY";
    case ScreenId::RelaysHub:
        return "RELAYS HUB";
    case ScreenId::RelayCycle:
        return "RELAY CYCLE";
    case ScreenId::RelayTimer:
        return "RELAY TIMER";
    case ScreenId::MasterWifi:
        return "MASTER WIFI";
    case ScreenId::MasterWifiProfile:
        return "DEVICE PROFILE";
    }
    return "?";
}

const char *statusName(ParamStatus st) {
    switch (st) {
    case ParamStatus::Ok:
        return "OK";
    case ParamStatus::Low:
        return "BAJO";
    case ParamStatus::High:
        return "ALTO";
    default:
        return "--";
    }
}

bool isIntParam(ParamId id) {
    return id == ParamId::Ec || id == ParamId::Orp;
}

}  // namespace

void UiMirror::dump() {
    DataStore &store = DataStore::instance();
    const TelemetrySnapshot snap = store.snapshot();
    const ScreenId scr = NavShell::current();
    const ParamId focus = NavShell::currentParam();

    Serial.println();
    Serial.println("+------------- ESPEJO HMI (lo que ve el usuario) -------------+");
    Serial.printf("| Panel ready=%d  %dx%d  FW %s\n",
                  DisplayHal::ready() ? 1 : 0,
                  LCD_H_RES, LCD_V_RES,
                  FIRMWARE_VERSION);
    Serial.printf("| Pantalla: %-28s\n", screenName(scr));
    if (scr == ScreenId::ParamDetail || scr == ScreenId::Calib) {
        Serial.printf("| Foco param: %s\n", store.name(focus));
    }
    if (scr == ScreenId::DosingChannel || scr == ScreenId::PumpActions ||
        scr == ScreenId::PumpName || scr == ScreenId::PumpPrime || scr == ScreenId::PumpCalib ||
        scr == ScreenId::PumpTimeDose || scr == ScreenId::PumpQuantity) {
        Serial.printf("| Foco dose: %s\n", doseChannelLabel(NavShell::currentDose()));
    }
    Serial.printf("| Fuente: %-4s   UART link: %s\n",
                  snap.source == DataSource::Sim ? "SIM" : "LIVE",
                  MasterLink::linkOk() || snap.linkOk ? "OK" : "sin enlace");
    Serial.printf("| Header temp: %.1f C\n", snap.tempAgua);
    Serial.println("|--------------------------------------------------------------|");
    Serial.println("| Param        Valor      Unidad   Estado   SP      Niveles    |");
    Serial.println("|--------------------------------------------------------------|");

    for (uint8_t i = 0; i < static_cast<uint8_t>(ParamId::Count); ++i) {
        const ParamId id = static_cast<ParamId>(i);
        const ParamConfig cfg = store.config(id);
        const float v = store.value(id);
        char valBuf[16];
        char spBuf[16];
        char rangeBuf[24];
        if (!isfinite(v)) {
            snprintf(valBuf, sizeof(valBuf), "%8s", "--");
        } else if (isIntParam(id)) {
            snprintf(valBuf, sizeof(valBuf), "%8.0f", v);
        } else {
            snprintf(valBuf, sizeof(valBuf), "%8.2f", v);
        }
        if (isIntParam(id)) {
            snprintf(spBuf, sizeof(spBuf), "%6.0f", cfg.setpoint);
            snprintf(rangeBuf, sizeof(rangeBuf), "%4.0f..%-4.0f", cfg.low, cfg.high);
        } else {
            snprintf(spBuf, sizeof(spBuf), "%6.2f", cfg.setpoint);
            snprintf(rangeBuf, sizeof(rangeBuf), "%4.1f..%-4.1f", cfg.low, cfg.high);
        }
        Serial.printf("| %-10s %s  %-7s %-7s %s  %s |\n",
                      store.name(id),
                      valBuf,
                      store.unit(id)[0] ? store.unit(id) : "-",
                      statusName(store.status(id)),
                      spBuf,
                      rangeBuf);
    }

    Serial.println("+--------------------------------------------------------------+");
    Serial.println("| En Serial Monitor escribi: view | mirror on | mirror off     |");
    Serial.println("+--------------------------------------------------------------+");
    Serial.println();

    if (scr == ScreenId::System) {
        Screens::dumpSystemSerial();
    }
}

void UiMirror::setAuto(bool on) {
    autoMirror = on;
    Serial.printf("[MIRROR] auto=%s\n", on ? "ON (cada 2s)" : "OFF");
    if (on) {
        dump();
    }
}

bool UiMirror::autoOn() { return autoMirror; }

void UiMirror::tick(unsigned long nowMs) {
    if (!autoMirror) {
        return;
    }
    if (lastDumpMs != 0 && (nowMs - lastDumpMs) < 2000) {
        return;
    }
    lastDumpMs = nowMs;
    dump();
}
