#include "SlaveInventory.h"
#include <ArduinoJson.h>
#include <cstring>

namespace {

SlaveInventory::Target targets_[SlaveInventory::kMaxTargets];
size_t count_ = 0;
unsigned long lastMs_ = 0;

void copyStr(char *dst, size_t n, const char *src) {
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

const SlaveInventory::Target *findByMac(const char *mac) {
    if (!mac || !mac[0]) {
        return nullptr;
    }
    for (size_t i = 0; i < count_; ++i) {
        if (strcmp(targets_[i].mac, mac) == 0) {
            return &targets_[i];
        }
    }
    return nullptr;
}

}  // namespace

void SlaveInventory::begin() { clear(); }

void SlaveInventory::clear() {
    count_ = 0;
    lastMs_ = 0;
    memset(targets_, 0, sizeof(targets_));
}

void SlaveInventory::applyFromJson(const char *jsonLine) {
    if (!jsonLine) {
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, jsonLine)) {
        return;
    }
    JsonArray arr = doc["slaves"].as<JsonArray>();
    if (arr.isNull()) {
        return;
    }

    size_t n = 0;
    for (JsonObject o : arr) {
        if (n >= kMaxTargets) {
            break;
        }
        Target &t = targets_[n];
        memset(&t, 0, sizeof(t));
        copyStr(t.mac, sizeof(t.mac), o["mac"] | "");
        copyStr(t.name, sizeof(t.name), o["name"] | "");
        t.online = o["online"] | false;
        t.local = o["local"] | (strcmp(t.mac, "local") == 0);
        int nr = o["numRelays"] | (o["num_relays"] | 8);
        if (nr < 1) {
            nr = 1;
        }
        if (nr > static_cast<int>(kMaxRelays)) {
            nr = static_cast<int>(kMaxRelays);
        }
        t.numRelays = static_cast<uint8_t>(nr);
        JsonArray rs = o["relays"].as<JsonArray>();
        for (int i = 0; i < nr; ++i) {
            t.relayOn[i] = false;
            t.relayLocked[i] = false;
            t.lockReason[i][0] = '\0';
            t.lockLabel[i][0] = '\0';
            if (rs.isNull() || i >= static_cast<int>(rs.size())) {
                continue;
            }
            JsonVariant v = rs[i];
            if (v.is<JsonObject>()) {
                JsonObject ro = v.as<JsonObject>();
                t.relayOn[i] = (ro["on"] | 0) != 0;
                t.relayLocked[i] = ro["locked"] | false;
                copyStr(t.lockReason[i], sizeof(t.lockReason[i]), ro["lock_reason"] | "");
                copyStr(t.lockLabel[i], sizeof(t.lockLabel[i]), ro["lock_label"] | "");
            } else {
                t.relayOn[i] = (v.as<int>() != 0);
            }
        }
        if (t.mac[0] == '\0') {
            continue;
        }
        ++n;
    }
    count_ = n;
    lastMs_ = millis();
}

size_t SlaveInventory::count() { return count_; }

const SlaveInventory::Target *SlaveInventory::at(size_t i) {
    if (i >= count_) {
        return nullptr;
    }
    return &targets_[i];
}

size_t SlaveInventory::localIndex() {
    for (size_t i = 0; i < count_; ++i) {
        if (targets_[i].local) {
            return i;
        }
    }
    return SIZE_MAX;
}

bool SlaveInventory::isEspNow(const Target *t) {
    if (!t || !t->mac[0]) {
        return false;
    }
    if (t->local || strcmp(t->mac, "local") == 0) {
        return false;
    }
    return true;
}

size_t SlaveInventory::firstEspNowIndex() {
    for (size_t i = 0; i < count_; ++i) {
        if (isEspNow(&targets_[i])) {
            return i;
        }
    }
    return SIZE_MAX;
}

unsigned long SlaveInventory::lastUpdateMs() { return lastMs_; }

bool SlaveInventory::isRelayLocked(const char *mac, uint8_t relay) {
    const Target *t = findByMac(mac);
    if (!t || relay >= kMaxRelays || relay >= t->numRelays) {
        return false;
    }
    return t->relayLocked[relay];
}

const char *SlaveInventory::relayLockLabel(const char *mac, uint8_t relay) {
    const Target *t = findByMac(mac);
    if (!t || relay >= kMaxRelays) {
        return "";
    }
    return t->lockLabel[relay];
}
