#ifndef RESERVOIR_CONFIG_H
#define RESERVOIR_CONFIG_H

#include <stdint.h>

/** Parámetros de malha fechada (Auto EC / Auto pH) + reservorio. */
namespace ReservoirConfig {

/** Batch = const fija post-calib; Recirculating = learning con volumen estable. */
enum class DosingMode : uint8_t { Batch = 0, Recirculating = 1 };

void begin();
void load();
void save();
void syncToMaster();

/** Agrupa setters sin NVS/UART intermedios. endBatch(true)=save+sync; false=reload NVS. */
void beginBatch();
void endBatch(bool commit);

float volumeL();
float homoSec();
float nutrientGapSec();
float pulseMl();
float pulseGapSec();
float dosingDelaySec();
DosingMode dosingMode();
float autoEcIntervalSec();
bool autoEcEnabled();
float autoPhIntervalSec();
bool autoPhEnabled();
/** Agresividad Auto EC/pH en % (0…100, pasos de 10). UART: /100 → 0.0…1.0. */
float maxStepEc();
float maxStepPh();
float dosingConstEc();
float dosingConstPh();
/** Armado global Inactive|Dosing: sin esto Auto no se envía activo al master. */
bool dosingArmed();
/** Cadeado UI de Controle→Auto: solo bloquea edición; no afecta UART. */
bool autoUiLocked();
/** Avanzado: capa Consumo EC 24h sobre Auto EC (UART: consumoDiario). */
bool consumoDiarioEnabled();
/** Avanzado: capa Consumo pH 24h sobre Auto pH (UART: consumoPh24h). */
bool consumoPh24hEnabled();

void setVolumeL(float v);
void setHomoSec(float v);
void setNutrientGapSec(float v);
void setPulseMl(float v);
void setPulseGapSec(float v);
void setDosingDelaySec(float v);
void setDosingMode(DosingMode m);
void setAutoEcIntervalSec(float v);
void setAutoEcEnabled(bool on);
void setAutoPhIntervalSec(float v);
void setAutoPhEnabled(bool on);
void setMaxStepEc(float pct);
void setMaxStepPh(float pct);
void setDosingConstEc(float v);
void setDosingConstPh(float v);
void setDosingArmed(bool on);
void setAutoUiLocked(bool on);
void setConsumoDiarioEnabled(bool on);
void setConsumoPh24hEnabled(bool on);

/** false si Auto está armado — Manual Dose/Prime/Time no deben actuar. */
bool manualDoseAllowed();

}  // namespace ReservoirConfig

#endif
