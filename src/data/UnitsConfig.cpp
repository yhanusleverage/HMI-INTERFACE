#include "UnitsConfig.h"
#include "Config.h"
#include <Preferences.h>
#include <cstdio>
#include <math.h>

namespace {

EcUnit ec_ = EcUnit::Us;
TempUnit temp_ = TempUnit::Celsius;
constexpr const char *NS = PREF_NAMESPACE;

}  // namespace

void UnitsConfig::begin() { load(); }

void UnitsConfig::load() {
    Preferences prefs;
    if (!prefs.begin(NS, true)) {
        return;
    }
    ec_ = static_cast<EcUnit>(prefs.getUChar("ec_unit", static_cast<uint8_t>(EcUnit::Us)));
    temp_ = static_cast<TempUnit>(prefs.getUChar("temp_unit", static_cast<uint8_t>(TempUnit::Celsius)));
    if (static_cast<uint8_t>(ec_) > static_cast<uint8_t>(EcUnit::Ppm640)) {
        ec_ = EcUnit::Us;
    }
    if (static_cast<uint8_t>(temp_) > static_cast<uint8_t>(TempUnit::Fahrenheit)) {
        temp_ = TempUnit::Celsius;
    }
    prefs.end();
}

void UnitsConfig::save() {
    Preferences prefs;
    if (!prefs.begin(NS, false)) {
        return;
    }
    prefs.putUChar("ec_unit", static_cast<uint8_t>(ec_));
    prefs.putUChar("temp_unit", static_cast<uint8_t>(temp_));
    prefs.end();
}

EcUnit UnitsConfig::ecUnit() { return ec_; }
TempUnit UnitsConfig::tempUnit() { return temp_; }

void UnitsConfig::setEcUnit(EcUnit u) {
    ec_ = u;
    save();
}

void UnitsConfig::setTempUnit(TempUnit u) {
    temp_ = u;
    save();
}

void UnitsConfig::formatEc(float us, char *buf, size_t n) {
    if (!buf || n == 0) {
        return;
    }
    switch (ec_) {
    case EcUnit::Ppm500:
        snprintf(buf, n, "%.0f", us * 0.5f);
        break;
    case EcUnit::Ppm640:
        snprintf(buf, n, "%.0f", us * 0.64f);
        break;
    case EcUnit::Us:
    default:
        snprintf(buf, n, "%.0f", us);
        break;
    }
}

void UnitsConfig::formatTemp(float celsius, char *buf, size_t n) {
    if (!buf || n == 0) {
        return;
    }
    if (temp_ == TempUnit::Fahrenheit) {
        snprintf(buf, n, "%.0f", celsius * 9.0f / 5.0f + 32.0f);
    } else {
        snprintf(buf, n, "%.0f", celsius);
    }
}

const char *UnitsConfig::ecUnitSuffix() {
    switch (ec_) {
    case EcUnit::Ppm500:
        return "ppm";
    case EcUnit::Ppm640:
        return "ppm";
    case EcUnit::Us:
    default:
        return "uS";
    }
}

const char *UnitsConfig::tempUnitSuffix() {
    return (temp_ == TempUnit::Fahrenheit) ? "\xC2\xB0" "F" : "\xC2\xB0" "C";
}
