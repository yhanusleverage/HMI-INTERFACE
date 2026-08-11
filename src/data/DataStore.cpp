#include "DataStore.h"
#include "HydroRanges.h"
#include "Config.h"
#include <Preferences.h>

DataStore &DataStore::instance() {
    static DataStore store;
    return store;
}

void DataStore::begin() {
    HydroRanges::applyDefaults(cfgs_);
    tel_.ph = 5.8f;
    tel_.ec = 470.0f;
    tel_.tempAgua = 20.0f;
    tel_.orp = 362.0f;
    tel_.doMgL = 9.1f;
    tel_.updatedMs = millis();
    tel_.source = DATA_SOURCE_SIM ? DataSource::Sim : DataSource::Live;
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
                             DataSource source) {
    auto apply = [](float raw, const ParamConfig &cfg) {
        return raw * cfg.calibScale + cfg.calibOffset;
    };
    tel_.ph = apply(ph, cfgs_[static_cast<size_t>(ParamId::Ph)]);
    tel_.ec = apply(ec, cfgs_[static_cast<size_t>(ParamId::Ec)]);
    tel_.tempAgua = apply(tempAgua, cfgs_[static_cast<size_t>(ParamId::TempAgua)]);
    tel_.orp = apply(orp, cfgs_[static_cast<size_t>(ParamId::Orp)]);
    tel_.doMgL = apply(doMgL, cfgs_[static_cast<size_t>(ParamId::Do)]);
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
