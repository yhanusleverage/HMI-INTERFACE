#ifndef SLAVE_INVENTORY_H
#define SLAVE_INVENTORY_H

#include <Arduino.h>

/** Cache HMI del inventory UART `t:slaves` (Master local + Atlas ESP-NOW). */
namespace SlaveInventory {

constexpr size_t kMaxTargets = 5; /* 1 local + hasta 4 slaves */
constexpr size_t kMaxRelays = 8;
constexpr size_t kMacLen = 18;
constexpr size_t kNameLen = 24;
constexpr size_t kLockLabelLen = 24;

struct Target {
    char mac[kMacLen];
    char name[kNameLen];
    bool online;
    bool local; /* mac == "local" → Master (bombas); no es Atlas */
    uint8_t numRelays;
    bool relayOn[kMaxRelays];
    bool relayLocked[kMaxRelays];
    char lockReason[kMaxRelays][16];
    char lockLabel[kMaxRelays][kLockLabelLen];
};

void begin();
void clear();
/** Parsea JSON raíz con t=slaves y array slaves[]. */
void applyFromJson(const char *jsonLine);
size_t count();
const Target *at(size_t i);
/** Índice del target local Master, o SIZE_MAX si falta. */
size_t localIndex();
/** true si es slave ESP-NOW (Atlas / otros), no Master local. */
bool isEspNow(const Target *t);
/** Primer slave ESP-NOW, o SIZE_MAX. */
size_t firstEspNowIndex();
unsigned long lastUpdateMs();

bool isRelayLocked(const char *mac, uint8_t relay);
const char *relayLockLabel(const char *mac, uint8_t relay);

}  // namespace SlaveInventory

#endif
