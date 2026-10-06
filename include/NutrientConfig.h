#ifndef NUTRIENT_CONFIG_H
#define NUTRIENT_CONFIG_H

#include "DoseChannel.h"
#include <stddef.h>
#include <stdint.h>

/** Máx. 6 nutrientes. Un nutriente por bomba (relé 1..6); sin compartir con pH. */
constexpr size_t NUTRIENT_MAX = PUMP_RELAY_COUNT;
constexpr size_t NUTRIENT_NAME_LEN = 16;

/**
 * Plan nutricional proporcional (HIDROWAVE):
 * name + ml/L + relay 1..6; proportion = ml / Σ ml.
 * Cada bomba como máximo un nutriente (y no pH Up/Down).
 */
namespace NutrientConfig {

void begin();
void load();
void save();

/** Sustituye la receta (ml/L por relé) y guarda NVS. No manda UART. */
void replaceQuantities(const char *const *names, const float *mlPerL, const uint8_t *relays,
                       size_t n);

float ecLow();
float ecHigh();
float phLow();
float phHigh();

void setEcLow(float v);
void setEcHigh(float v);
void setPhLow(float v);
void setPhHigh(float v);

void syncBandsToStore();

size_t listCount();
const char *listName(size_t ix);
float listMlPerL(size_t ix);
uint8_t listRelayNumber(size_t ix);

bool listAdd(const char *name, float mlPerL);
bool listSetName(size_t ix, const char *name);
bool listSetMlPerL(size_t ix, float mlPerL);
bool listSetRelayNumber(size_t ix, uint8_t relayNumber);
bool listRemove(size_t ix);

/** Primer relé 1..6 sin nutriente ni pH, o 0 si todos ocupados. */
uint8_t firstFreeRelay();
bool relayInUse(uint8_t relayNumber, size_t exceptIx = SIZE_MAX);
/** True si otro nutriente (≠ exceptIx) ya usa ese relé. */
bool relaySharedByNutrient(uint8_t relayNumber, size_t exceptIx = SIZE_MAX);

/** Relés Master 1..6 para Auto pH (0 = sin asignar). */
uint8_t phUpRelay();
uint8_t phDownRelay();
void setPhUpRelay(uint8_t relay1to6OrZero);
void setPhDownRelay(uint8_t relay1to6OrZero);
/** Si phUp/phDown caen en bomba con nutriente (NVS legacy), los pone a 0. */
void clearPhRelayNutrientConflicts();

float totalMlPerL();
float listProportion(size_t ix);
float listProportionPct(size_t ix);

/** EC de etiqueta para 1 L de la receta (µS). 0 = no definida. Master: baseDose. */
float recipeEcUs();
void setRecipeEcUs(float us);

/** ml/L del nutriente asignado a ese canal bomba (0 si ninguno). */
float mlPerLForChannel(DoseChannel ch);

/** Nombre del nutriente en ese relé, o nullptr si no hay. */
const char *nameForRelay(uint8_t relay1to6);

void syncToMaster();
void syncPhRelaysToMaster();

}

#endif
