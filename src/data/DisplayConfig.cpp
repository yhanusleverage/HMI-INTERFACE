#include "DisplayConfig.h"
#include "Config.h"
#include <Preferences.h>

namespace {

bool ph_ = true;
bool ec_ = true;
bool temp_ = true;
bool orp_ = true;
bool do_ = true;

constexpr const char *NS = PREF_NAMESPACE;

}  // namespace

void DisplayConfig::begin() { load(); }

void DisplayConfig::load() {
    Preferences prefs;
    if (!prefs.begin(NS, true)) {
        return;
    }
    ph_ = prefs.getBool("show_ph", true);
    ec_ = prefs.getBool("show_ec", true);
    temp_ = prefs.getBool("show_temp", true);
    orp_ = prefs.getBool("show_orp", true);
    do_ = prefs.getBool("show_do", true);
    prefs.end();
}

void DisplayConfig::save() {
    Preferences prefs;
    if (!prefs.begin(NS, false)) {
        return;
    }
    prefs.putBool("show_ph", ph_);
    prefs.putBool("show_ec", ec_);
    prefs.putBool("show_temp", temp_);
    prefs.putBool("show_orp", orp_);
    prefs.putBool("show_do", do_);
    prefs.end();
}

bool DisplayConfig::showPh() { return ph_; }
bool DisplayConfig::showEc() { return ec_; }
bool DisplayConfig::showTemp() { return temp_; }
bool DisplayConfig::showOrp() { return orp_; }
bool DisplayConfig::showDo() { return do_; }

void DisplayConfig::setShowPh(bool on) {
    ph_ = on;
    save();
}
void DisplayConfig::setShowEc(bool on) {
    ec_ = on;
    save();
}
void DisplayConfig::setShowTemp(bool on) {
    temp_ = on;
    save();
}
void DisplayConfig::setShowOrp(bool on) {
    orp_ = on;
    save();
}
void DisplayConfig::setShowDo(bool on) {
    do_ = on;
    save();
}

bool DisplayConfig::showParam(ParamId id) {
    switch (id) {
    case ParamId::Ph:
        return ph_;
    case ParamId::Ec:
        return ec_;
    case ParamId::TempAgua:
        return temp_;
    case ParamId::Orp:
        return orp_;
    case ParamId::Do:
        return do_;
    default:
        return true;
    }
}
