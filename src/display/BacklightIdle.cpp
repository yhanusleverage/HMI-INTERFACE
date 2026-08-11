#include "BacklightIdle.h"
#include "DisplayHal.h"
#include "DataStore.h"
#include "Config.h"
#include <Preferences.h>

namespace {

constexpr uint32_t kAutoTimeoutMs = 60UL * 1000UL;
/** Con alarma de proceso: no apagar de inmediato, pero sí tras idle largo (invernadero vacío). */
constexpr uint32_t kAlarmHoldTimeoutMs = 3UL * 60UL * 1000UL;
constexpr const char *kNs = PREF_NAMESPACE;
constexpr const char *kKeyMode = "bl_mode";
constexpr const char *kKeyLegacy = "bl_on";

BacklightIdle::Mode mode_ = BacklightIdle::Mode::AutoOff;
bool lit_ = true;
uint32_t lastActivityMs_ = 0;
bool swallowUntilRelease_ = false;
bool prevAlarm_ = false;

bool paramAlarm(ParamId id) {
    const ParamStatus st = DataStore::instance().status(id);
    return st == ParamStatus::Low || st == ParamStatus::High;
}

void applyHw(bool on) {
    if (lit_ == on) {
        return;
    }
    lit_ = on;
    DisplayHal::setBacklight(on);
}

void bumpActivity() { lastActivityMs_ = millis(); }

void load() {
    Preferences prefs;
    if (!prefs.begin(kNs, true)) {
        mode_ = BacklightIdle::Mode::AutoOff;
        return;
    }
    if (prefs.isKey(kKeyMode)) {
        const uint8_t v = prefs.getUChar(kKeyMode, static_cast<uint8_t>(BacklightIdle::Mode::AutoOff));
        if (v <= static_cast<uint8_t>(BacklightIdle::Mode::ForcedOff)) {
            mode_ = static_cast<BacklightIdle::Mode>(v);
        } else {
            mode_ = BacklightIdle::Mode::AutoOff;
        }
    } else if (prefs.isKey(kKeyLegacy)) {
        /* Migración: bl_on true → AutoOff; false → ForcedOff */
        mode_ = prefs.getBool(kKeyLegacy, true) ? BacklightIdle::Mode::AutoOff
                                                : BacklightIdle::Mode::ForcedOff;
    } else {
        mode_ = BacklightIdle::Mode::AutoOff;
    }
    prefs.end();
}

void save() {
    Preferences prefs;
    if (!prefs.begin(kNs, false)) {
        return;
    }
    prefs.putUChar(kKeyMode, static_cast<uint8_t>(mode_));
    prefs.putBool(kKeyLegacy, mode_ != BacklightIdle::Mode::ForcedOff);
    prefs.end();
}

}  // namespace

bool BacklightIdle::processAlarmActive() {
    const TelemetrySnapshot snap = DataStore::instance().snapshot();
    /* Sin enlace LIVE: no congelar BL por bandas vs valores default (p.ej. EC 470 vs nec 1580). */
#if !DATA_SOURCE_SIM
    if (!snap.linkOk) {
        return false;
    }
    /* Telemetría demasiado vieja (>30 s) → no contar como alarma de proceso. */
    if (snap.updatedMs == 0 || (millis() - snap.updatedMs) > 30000UL) {
        return false;
    }
#endif
    return paramAlarm(ParamId::Ph) || paramAlarm(ParamId::Ec) || paramAlarm(ParamId::Orp) ||
           paramAlarm(ParamId::Do);
}

void BacklightIdle::begin() {
    load();
    bumpActivity();
    swallowUntilRelease_ = false;
    prevAlarm_ = processAlarmActive();
    if (prevAlarm_ || mode_ != Mode::ForcedOff) {
        applyHw(true);
    } else {
        applyHw(false);
    }
}

BacklightIdle::Mode BacklightIdle::mode() { return mode_; }

uint32_t BacklightIdle::idleTimeoutMs() { return kAutoTimeoutMs; }

bool BacklightIdle::lit() { return lit_; }

void BacklightIdle::setMode(Mode m) {
    mode_ = m;
    save();
    bumpActivity();
    swallowUntilRelease_ = false;
    if (processAlarmActive()) {
        applyHw(true);
        return;
    }
    switch (mode_) {
    case Mode::AlwaysOn:
    case Mode::AutoOff:
        applyHw(true);
        break;
    case Mode::ForcedOff:
        applyHw(false);
        break;
    }
}

void BacklightIdle::setLit(bool on) {
    if (processAlarmActive() && !on) {
        return;
    }
    if (on) {
        bumpActivity();
    }
    applyHw(on);
}

bool BacklightIdle::onTouchSample(bool pressed) {
    if (pressed) {
        bumpActivity();
        if (!lit_) {
            applyHw(true);
            if (mode_ == Mode::ForcedOff) {
                /* Operador despertó la pantalla: salir de OFF forzado. */
                mode_ = Mode::AlwaysOn;
                save();
            }
            swallowUntilRelease_ = true;
            return true;
        }
        if (swallowUntilRelease_) {
            return true;
        }
        return false;
    }
    /* release */
    if (swallowUntilRelease_) {
        swallowUntilRelease_ = false;
        return true;
    }
    return false;
}

void BacklightIdle::tick() {
    const bool alarm = processAlarmActive();

    if (alarm) {
        if (!lit_) {
            applyHw(true);
        }
        /* No resetear activity cada tick — si no, nunca apaga con BAJO/ALTO crónico. */
        if (!prevAlarm_) {
            bumpActivity();
            if (mode_ == Mode::ForcedOff) {
                mode_ = Mode::AlwaysOn;
                save();
            }
        }
        prevAlarm_ = true;
        if (mode_ == Mode::AutoOff && lit_ &&
            (millis() - lastActivityMs_) >= kAlarmHoldTimeoutMs) {
            applyHw(false);
        }
        return;
    }

    if (prevAlarm_) {
        /* Alarma limpia: deja ON; si estaba ForcedOff, migrar (no apagar al instante). */
        bumpActivity();
        if (!lit_) {
            applyHw(true);
        }
        if (mode_ == Mode::ForcedOff) {
            mode_ = Mode::AlwaysOn;
            save();
        }
        prevAlarm_ = false;
    }

    switch (mode_) {
    case Mode::AlwaysOn:
        if (!lit_) {
            applyHw(true);
        }
        break;
    case Mode::ForcedOff:
        if (lit_) {
            applyHw(false);
        }
        break;
    case Mode::AutoOff:
        if (lit_ && (millis() - lastActivityMs_) >= kAutoTimeoutMs) {
            applyHw(false);
        }
        break;
    }
}
