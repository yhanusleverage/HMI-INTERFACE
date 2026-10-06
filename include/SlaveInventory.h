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
/** Un relé confirmado por UART `t:relay`. */
void applyRelayBit(const char *mac, uint8_t relay, bool on);
/** true si ese relé tuvo confirmación después de afterMs. */
bool confirmedBit(const char *mac, uint8_t relay, unsigned long afterMs, bool *onOut);
size_t count();
const Target *at(size_t i);
/** Índice del target local Master, o SIZE_MAX si falta. */
size_t localIndex();
/** true si es slave ESP-NOW (Atlas / otros), no Master local. */
bool isEspNow(const Target *t);
/** Primer Atlas online; si ninguno, el primer ESP-NOW. SIZE_MAX si no hay. */
size_t firstEspNowIndex();
unsigned long lastUpdateMs();

bool isRelayLocked(const char *mac, uint8_t relay);
const char *relayLockLabel(const char *mac, uint8_t relay);

}  // namespace SlaveInventory

#endif
