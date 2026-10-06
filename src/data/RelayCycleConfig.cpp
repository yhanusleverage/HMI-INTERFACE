#include "RelayCycleConfig.h"
#include "RelayAliasConfig.h"
#include "RelayActuationLock.h"
#include "MasterLink.h"
#include "Config.h"
#include <Preferences.h>
#include <cstdio>
#include <cstring>

namespace {

constexpr const char *NS = PREF_NAMESPACE;

RelayCycleConfig::Entry entries_[RELAY_CYCLE_MAX] = {};
size_t count_ = 0;
unsigned long lastTickMs_ = 0;

void copyMac(char *dst, size_t n, const char *src) {
    if (!dst || n == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
}

int findIx(const char *mac, uint8_t relay) {
    if (!mac || mac[0] == '\0' || relay > 7) {
        return -1;
    }
    for (size_t i = 0; i < count_; ++i) {
        if (entries_[i].used && entries_[i].relay == relay && strcmp(entries_[i].mac, mac) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int ensureIx(const char *mac, uint8_t relay) {
    int ix = findIx(mac, relay);
    if (ix >= 0) {
        return ix;
    }
    if (count_ >= RELAY_CYCLE_MAX) {
        return -1;
    }
    RelayCycleConfig::Entry &e = entries_[count_];
    memset(&e, 0, sizeof(e));
    e.used = true;
    e.enabled = false;
    copyMac(e.mac, sizeof(e.mac), mac);
    e.relay = relay;
    e.onHours = 12;
    e.offHours = 12;
    e.phaseStartMs = millis();
    e.lastWantOn = false;
    ix = static_cast<int>(count_);
    ++count_;
    return ix;
}

bool atlasMac(const RelayCycleConfig::Entry &e) {
    return e.mac[0] != '\0' && strcmp(e.mac, "local") != 0 &&
           strcmp(e.mac, RelayAliasConfig::kPlaceholderMac) != 0;
}

int phaseSec(uint8_t hours, uint8_t mins) {
    const int sec = static_cast<int>(hours) * 3600 + static_cast<int>(mins) * 60;
    return sec > 0 ? sec : 60;
}

/** Un solo cycle / cycle_stop. El esclavo cuenta; el HMI no alterna on/off. */
bool pushMaster(RelayCycleConfig::Entry &e) {
    if (!atlasMac(e)) {
        e.needsMasterPush = false;
        return true;
    }
    if (!MasterLink::linkOk()) {
        return false;
    }
    if (e.enabled) {
        if (!RelayActuationLock::allowsCiclo(e.mac, e.relay)) {
            return false;
        }
        RelayActuationLock::set(e.mac, e.relay, RelayActuationLock::Owner::Ciclo);
        MasterLink::sendRelaySlave(e.mac, e.relay, "cycle", phaseSec(e.onHours, e.onMin),
                                   phaseSec(e.offHours, e.offMin), "cycle");
    } else {
        MasterLink::sendRelaySlave(e.mac, e.relay, "cycle_stop", 0, 0, "cycle_stop");
    }
    e.needsMasterPush = false;
    return true;
}

}  // namespace

void RelayCycleConfig::begin() { load(); }

void RelayCycleConfig::load() {
    count_ = 0;
    Preferences prefs;
    if (!prefs.begin(NS, true)) {
        return;
    }
    const uint8_t n = prefs.getUChar("rc_n", 0);
    for (uint8_t i = 0; i < n && count_ < RELAY_CYCLE_MAX; ++i) {
        char key[12];
        snprintf(key, sizeof(key), "rc_%u", static_cast<unsigned>(i));
        String blob = prefs.getString(key, "");
        if (blob.length() == 0) {
            continue;
        }
        /* mac|relay|en|onH|offH */
        Entry &e = entries_[count_];
        memset(&e, 0, sizeof(e));
        char buf[80];
        strncpy(buf, blob.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *save = nullptr;
        char *tok = strtok_r(buf, "|", &save);
        if (!tok) {
            continue;
        }
        copyMac(e.mac, sizeof(e.mac), tok);
        tok = strtok_r(nullptr, "|", &save);
        if (!tok) {
            continue;
        }
        e.relay = static_cast<uint8_t>(atoi(tok));
        if (e.relay > 7) {
            e.relay = 7;
        }
        tok = strtok_r(nullptr, "|", &save);
        e.enabled = tok && atoi(tok) != 0;
        tok = strtok_r(nullptr, "|", &save);
        e.onHours = tok ? static_cast<uint8_t>(atoi(tok)) : 12;
        tok = strtok_r(nullptr, "|", &save);
        e.offHours = tok ? static_cast<uint8_t>(atoi(tok)) : 12;
        tok = strtok_r(nullptr, "|", &save);
        e.onMin = tok ? static_cast<uint8_t>(atoi(tok)) : 0;
        tok = strtok_r(nullptr, "|", &save);
        e.offMin = tok ? static_cast<uint8_t>(atoi(tok)) : 0;
        if (e.onHours > 23) {
            e.onHours = 23;
        }
        if (e.offHours > 23) {
            e.offHours = 23;
        }
        if (e.onMin > 59) {
            e.onMin = 59;
        }
        if (e.offMin > 59) {
            e.offMin = 59;
        }
        if (e.onHours == 0 && e.onMin == 0) {
            e.onMin = 1;
        }
        if (e.offHours == 0 && e.offMin == 0) {
            e.offMin = 1;
        }
        e.used = true;
        e.phaseStartMs = millis();
        e.lastWantOn = false;
        e.needsMasterPush = e.enabled;
        ++count_;
    }
    prefs.end();
}

void RelayCycleConfig::save() {
    Preferences prefs;
    if (!prefs.begin(NS, false)) {
        return;
    }
    prefs.putUChar("rc_n", static_cast<uint8_t>(count_));
    for (size_t i = 0; i < count_; ++i) {
        char key[12];
        char blob[96];
        snprintf(key, sizeof(key), "rc_%u", static_cast<unsigned>(i));
        snprintf(blob, sizeof(blob), "%s|%u|%u|%u|%u|%u|%u", entries_[i].mac,
                 static_cast<unsigned>(entries_[i].relay),
                 entries_[i].enabled ? 1u : 0u,
                 static_cast<unsigned>(entries_[i].onHours),
                 static_cast<unsigned>(entries_[i].offHours),
                 static_cast<unsigned>(entries_[i].onMin),
                 static_cast<unsigned>(entries_[i].offMin));
        prefs.putString(key, blob);
    }
    prefs.end();
}

const RelayCycleConfig::Entry *RelayCycleConfig::get(const char *mac, uint8_t relay) {
    const int ix = findIx(mac, relay);
    if (ix < 0) {
        return nullptr;
    }
    return &entries_[static_cast<size_t>(ix)];
}

bool RelayCycleConfig::setEnabled(const char *mac, uint8_t relay, bool on) {
    const int ix = ensureIx(mac, relay);
    if (ix < 0) {
        return false;
    }
    Entry &e = entries_[static_cast<size_t>(ix)];
    e.enabled = on;
    e.phaseStartMs = millis();
    e.lastWantOn = on;
    e.needsMasterPush = true;
    if (on) {
        if (RelayActuationLock::allowsCiclo(mac, relay)) {
            RelayActuationLock::set(mac, relay, RelayActuationLock::Owner::Ciclo);
            pushMaster(e);
        }
    } else {
        pushMaster(e);
        if (RelayActuationLock::get(mac, relay) == RelayActuationLock::Owner::Ciclo) {
            RelayActuationLock::set(mac, relay, RelayActuationLock::Owner::Idle);
        }
    }
    save();
    return true;
}

bool RelayCycleConfig::setHours(const char *mac, uint8_t relay, uint8_t onHours, uint8_t onMin,
                                uint8_t offHours, uint8_t offMin) {
    const int ix = ensureIx(mac, relay);
    if (ix < 0) {
        return false;
    }
    if (onHours > 23) {
        onHours = 23;
    }
    if (offHours > 23) {
        offHours = 23;
    }
    if (onMin > 59) {
        onMin = 59;
    }
    if (offMin > 59) {
        offMin = 59;
    }
    if (onHours == 0 && onMin == 0) {
        onMin = 1;
    }
    if (offHours == 0 && offMin == 0) {
        offMin = 1;
    }
    Entry &e = entries_[static_cast<size_t>(ix)];
    e.onHours = onHours;
    e.onMin = onMin;
    e.offHours = offHours;
    e.offMin = offMin;
    e.phaseStartMs = millis();
    if (e.enabled) {
        e.needsMasterPush = true;
    }
    save();
    return true;
}

void RelayCycleConfig::tick() {
    const unsigned long now = millis();
    if (lastTickMs_ != 0 && (now - lastTickMs_) < 1000UL) {
        return;
    }
    lastTickMs_ = now;
    if (!MasterLink::linkOk()) {
        return;
    }
    for (size_t i = 0; i < count_; ++i) {
        Entry &e = entries_[i];
        if (!e.used || !e.needsMasterPush) {
            continue;
        }
        pushMaster(e);
    }
}
