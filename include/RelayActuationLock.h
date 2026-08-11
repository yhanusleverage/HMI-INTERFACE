#ifndef RELAY_ACTUATION_LOCK_H
#define RELAY_ACTUATION_LOCK_H

#include "SlaveInventory.h"
#include <stddef.h>
#include <stdint.h>

/** Dueño activo por relé Atlas — evita pelea Ciclo / Timer / ON-OFF manual. */
namespace RelayActuationLock {

enum class Owner : uint8_t {
    Idle = 0,
    Ciclo,
    Timer,
    ManualPulse,
};

void begin();

Owner get(const char *mac, uint8_t relay);
void set(const char *mac, uint8_t relay, Owner owner);

/** Ciclo tick: false si Timer o ManualPulse tiene el relé. */
bool allowsCiclo(const char *mac, uint8_t relay);

/** UI: true si Timer corre en ese relé. */
bool timerActive(const char *mac, uint8_t relay);

void clearAll();

}  // namespace RelayActuationLock

#endif
