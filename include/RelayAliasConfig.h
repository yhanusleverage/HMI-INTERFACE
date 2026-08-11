#ifndef RELAY_ALIAS_CONFIG_H
#define RELAY_ALIAS_CONFIG_H

#include "SlaveInventory.h"
#include <stddef.h>
#include <stdint.h>

/** Alias de usuario para relés Atlas. Placeholders R1..R8.
 *  Sin inventario ESP-NOW se usa MAC placeholder `kPlaceholderMac` ("atlas").
 */
constexpr size_t RELAY_ALIAS_NAME_LEN = 16;
constexpr size_t RELAY_ALIAS_MAX = SlaveInventory::kMaxTargets * SlaveInventory::kMaxRelays;

namespace RelayAliasConfig {

/** MAC NVS cuando aún no hay slave ESP-NOW en inventario. */
constexpr const char *kPlaceholderMac = "atlas";

void begin();
void load();
void save();

/** Nombre guardado (puede ser vacío). */
const char *getName(const char *mac, uint8_t relayIndex);
bool setName(const char *mac, uint8_t relayIndex, const char *name);

/**
 * Etiqueta UI en buf:
 * - alias vacío + Atlas ESP-NOW → "Atlas · R3"
 * - alias vacío sin device → "R3"
 * - alias + device → "Atlas · Bomba"
 * - alias sin device → "Bomba"
 * Master local (bombas) no se etiqueta como Atlas.
 */
void displayLabel(char *buf, size_t n, const char *mac, uint8_t relayIndex);

/** Placeholder "R1".."R8" (relayIndex 0-based). */
void placeholder(char *buf, size_t n, uint8_t relayIndex);

/**
 * Copia alias no vacíos de kPlaceholderMac → realMac (si el destino no tiene nombre)
 * y borra entradas placeholder. No-op si realMac vacío o es el placeholder.
 */
void migratePlaceholderTo(const char *realMac);

}  // namespace RelayAliasConfig

#endif
