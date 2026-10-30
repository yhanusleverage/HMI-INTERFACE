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
/** Copia el interruptor vivo del Master. No reenvía por UART. */
void adoptAutoFromMaster(bool ecOn, bool phOn);
/** Copia la malla que llegó de la web. Guarda NVS y no reenvía por UART. */
struct LoopMirror {
    bool hasMaxStepEc = false;
    float maxStepEcPct = 50.0f;
    bool hasMaxStepPh = false;
    float maxStepPhPct = 50.0f;
    bool hasConsumoEc = false;
    bool consumoEc = false;
    bool hasConsumoPh = false;
    bool consumoPh = false;
    bool hasEcIv = false;
    float ecIv = 30.0f;
    bool hasPhIv = false;
    float phIv = 30.0f;
    bool hasVolume = false;
    float volumeL = 20.0f;
    bool hasHomo = false;
    float homoSec = 60.0f;
    bool hasPulseMl = false;
    float pulseMl = 2.0f;
    bool hasPulseGap = false;
    float pulseGapSec = 2.0f;
    bool hasAutoEc = false;
    bool autoEc = false;
    bool hasAutoPh = false;
    bool autoPh = false;
};
void adoptLoopFromMaster(const LoopMirror& in);
/** true una vez tras un toque local de Auto EC o Auto pH. */
bool takeAutoPush();
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
