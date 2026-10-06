#ifndef RELAY_CYCLE_CONFIG_H
#define RELAY_CYCLE_CONFIG_H

#include "SlaveInventory.h"
#include <stddef.h>
#include <stdint.h>

/** Ciclo ON/OFF repetible por relé (p.ej. 12h ON + 12h OFF = 24h). */
constexpr size_t RELAY_CYCLE_MAX = SlaveInventory::kMaxTargets * SlaveInventory::kMaxRelays;

namespace RelayCycleConfig {

struct Entry {
    bool used;
    bool enabled;
    char mac[SlaveInventory::kMacLen];
    uint8_t relay; /* 0..7 */
    uint8_t onHours;  /* 0..23 */
    uint8_t onMin;    /* 0..59 */
    uint8_t offHours; /* 0..23 */
    uint8_t offMin;   /* 0..59 */
    /* Runtime (no NVS) */
    unsigned long phaseStartMs;
    bool lastWantOn;
    bool needsMasterPush;
};

void begin();
void load();
void save();

const Entry *get(const char *mac, uint8_t relay);
bool setEnabled(const char *mac, uint8_t relay, bool on);
bool setHours(const char *mac, uint8_t relay, uint8_t onHours, uint8_t onMin, uint8_t offHours,
              uint8_t offMin);

/** Reintenta el cycle/cycle_stop pendiente si el UART no estaba listo. */
void tick();

}  // namespace RelayCycleConfig

#endif
