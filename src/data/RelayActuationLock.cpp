#include "RelayActuationLock.h"
#include <cstring>

namespace {

constexpr size_t kMax = SlaveInventory::kMaxTargets * SlaveInventory::kMaxRelays;

struct Slot {
    char mac[SlaveInventory::kMacLen];
    uint8_t relay;
    RelayActuationLock::Owner owner;
    bool used;
};

Slot slots_[kMax];
size_t count_ = 0;

int findIx(const char *mac, uint8_t relay) {
    if (!mac || mac[0] == '\0' || relay > 7) {
        return -1;
    }
    for (size_t i = 0; i < count_; ++i) {
        if (slots_[i].used && slots_[i].relay == relay && strcmp(slots_[i].mac, mac) == 0) {
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
    if (count_ >= kMax) {
        return -1;
    }
    Slot &s = slots_[count_];
    memset(&s, 0, sizeof(s));
    strncpy(s.mac, mac, sizeof(s.mac) - 1);
    s.relay = relay;
    s.owner = RelayActuationLock::Owner::Idle;
    s.used = true;
    ix = static_cast<int>(count_);
    ++count_;
    return ix;
}

}  // namespace

namespace RelayActuationLock {

void begin() {
    count_ = 0;
    memset(slots_, 0, sizeof(slots_));
}

Owner get(const char *mac, uint8_t relay) {
    const int ix = findIx(mac, relay);
    if (ix < 0) {
        return Owner::Idle;
    }
    return slots_[static_cast<size_t>(ix)].owner;
}

void set(const char *mac, uint8_t relay, Owner owner) {
    if (!mac || mac[0] == '\0') {
        return;
    }
    const int ix = ensureIx(mac, relay);
    if (ix < 0) {
        return;
    }
    slots_[static_cast<size_t>(ix)].owner = owner;
    if (owner == Owner::Idle) {
        /* Compactar slot idle opcional — mantener entrada para lookup rápido. */
    }
}

bool allowsCiclo(const char *mac, uint8_t relay) {
    return get(mac, relay) != Owner::Timer;
}

bool timerActive(const char *mac, uint8_t relay) {
    return get(mac, relay) == Owner::Timer;
}

void clearAll() {
    count_ = 0;
    memset(slots_, 0, sizeof(slots_));
}

}  // namespace RelayActuationLock
