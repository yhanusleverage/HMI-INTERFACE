#include "DataStore.h"
#include "HydroRanges.h"
#include "Config.h"
#include <Preferences.h>
#include <math.h>

DataStore &DataStore::instance() {
    static DataStore store;
    return store;
}

void DataStore::begin() {
    HydroRanges::applyDefaults(cfgs_);
#if DATA_SOURCE_SIM
    tel_.ph = 5.8f;
    tel_.ec = 470.0f;
    tel_.tempAgua = 20.0f;
    tel_.orp = 362.0f;
    tel_.doMgL = 9.1f;
    tel_.phValid = true;
    tel_.ecValid = true;
    tel_.tempValid = true;
    tel_.source = DataSource::Sim;
#else
    /* Live: sin inventar PV hasta el primer telemetry válido. */
    tel_.ph = NAN;
    tel_.ec = NAN;
    tel_.tempAgua = NAN;
    tel_.orp = NAN;
    tel_.doMgL = NAN;
    tel_.phValid = false;
    tel_.ecValid = false;
    tel_.tempValid = false;
    tel_.source = DataSource::Live;
#endif
    tel_.updatedMs = millis();
    tel_.linkOk = false;
    loadPrefs();
}

void DataStore::loadPrefs() {
    Preferences prefs;
    if (!prefs.begin(PREF_NAMESPACE, true)) {
        return;
    }
    for (size_t i = 0; i < static_cast<size_t>(ParamId::Count); ++i) {
        char key[16];
        snprintf(key, sizeof(key), "sp%u", static_cast<unsigned>(i));
        cfgs_[i].setpoint = prefs.getFloat(key, cfgs_[i].setpoint);
        snprintf(key, sizeof(key), "lo%u", static_cast<unsigned>(i));
        cfgs_[i].low = prefs.getFloat(key, cfgs_[i].low);
        snprintf(key, sizeof(key), "hi%u", static_cast<unsigned>(i));
        cfgs_[i].high = prefs.getFloat(key, cfgs_[i].high);
        snprintf(key, sizeof(key), "off%u", static_cast<unsigned>(i));
        cfgs_[i].calibOffset = prefs.getFloat(key, cfgs_[i].calibOffset);
        snprintf(key, sizeof(key), "sc%u", static_cast<unsigned>(i));
        cfgs_[i].calibScale = prefs.getFloat(key, cfgs_[i].calibScale);
    }
    prefs.end();
}

void DataStore::savePrefs() {
    Preferences prefs;
    if (!prefs.begin(PREF_NAMESPACE, false)) {
        return;
    }
    for (size_t i = 0; i < static_cast<size_t>(ParamId::Count); ++i) {
        char key[16];
        snprintf(key, sizeof(key), "sp%u", static_cast<unsigned>(i));
        prefs.putFloat(key, cfgs_[i].setpoint);
        snprintf(key, sizeof(key), "lo%u", static_cast<unsigned>(i));
        prefs.putFloat(key, cfgs_[i].low);
        snprintf(key, sizeof(key), "hi%u", static_cast<unsigned>(i));
        prefs.putFloat(key, cfgs_[i].high);
        snprintf(key, sizeof(key), "off%u", static_cast<unsigned>(i));
        prefs.putFloat(key, cfgs_[i].calibOffset);
        snprintf(key, sizeof(key), "sc%u", static_cast<unsigned>(i));
        prefs.putFloat(key, cfgs_[i].calibScale);
    }
    prefs.end();
}

TelemetrySnapshot DataStore::snapshot() const { return tel_; }

void DataStore::setTelemetry(float ph, float ec, float tempAgua, float orp, float doMgL,
                             DataSource source, bool phValid, bool ecValid, bool tempValid) {
    auto apply = [](float raw, const ParamConfig &cfg) -> float {
        if (!isfinite(raw)) {
            return NAN;
        }
        return raw * cfg.calibScale + cfg.calibOffset;
    };

    tel_.phValid = phValid;
    tel_.ecValid = ecValid;
    tel_.tempValid = tempValid;
    tel_.ph = phValid ? apply(ph, cfgs_[static_cast<size_t>(ParamId::Ph)]) : NAN;
    tel_.ec = ecValid ? apply(ec, cfgs_[static_cast<size_t>(ParamId::Ec)]) : NAN;
    tel_.tempAgua = tempValid ? apply(tempAgua, cfgs_[static_cast<size_t>(ParamId::TempAgua)]) : NAN;

    /* ORP/DO: Master prod no emite; conservar si el campo falta (NAN). */
    if (isfinite(orp)) {
        tel_.orp = apply(orp, cfgs_[static_cast<size_t>(ParamId::Orp)]);
    }
    if (isfinite(doMgL)) {
        tel_.doMgL = apply(doMgL, cfgs_[static_cast<size_t>(ParamId::Do)]);
    }

    tel_.updatedMs = millis();
    tel_.source = source;
}

void DataStore::setLinkOk(bool ok) { tel_.linkOk = ok; }

ParamConfig DataStore::config(ParamId id) const {
    return cfgs_[static_cast<size_t>(id)];
}

void DataStore::setConfig(ParamId id, const ParamConfig &cfg) {
    cfgs_[static_cast<size_t>(id)] = cfg;
    savePrefs();
}

ParamStatus DataStore::status(ParamId id) const {
    return HydroRanges::evaluate(value(id), config(id));
}

float DataStore::value(ParamId id) const {
    switch (id) {
    case ParamId::Ph:
        return tel_.ph;
    case ParamId::Ec:
        return tel_.ec;
    case ParamId::TempAgua:
        return tel_.tempAgua;
    case ParamId::Orp:
        return tel_.orp;
    case ParamId::Do:
        return tel_.doMgL;
    default:
        return NAN;
    }
}

const char *DataStore::name(ParamId id) const {
    switch (id) {
    case ParamId::Ph:
        return "pH";
    case ParamId::Ec:
        return "EC";
    case ParamId::TempAgua:
        return "Temp";
    case ParamId::Orp:
        return "ORP";
    case ParamId::Do:
        return "DO";
    default:
        return "?";
    }
}

const char *DataStore::unit(ParamId id) const {
    switch (id) {
    case ParamId::Ph:
        return "";
    case ParamId::Ec:
        return "uS";
    case ParamId::TempAgua:
        return "\xC2\xB0" "C";
    case ParamId::Orp:
        return "mV";
    case ParamId::Do:
        return "mg/L";
    default:
        return "";
    }
}

const char *DataStore::labelWithUnit(ParamId id) const {
    switch (id) {
    case ParamId::Ph:
        return "pH";
    case ParamId::Ec:
        return "EC [uS]";
    case ParamId::TempAgua:
        return "Temp [\xC2\xB0" "C]";
    case ParamId::Orp:
        return "ORP [mV]";
    case ParamId::Do:
        return "DO [mg/L]";
    default:
        return "?";
    }
}
