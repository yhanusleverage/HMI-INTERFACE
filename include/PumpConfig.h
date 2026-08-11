#ifndef PUMP_CONFIG_H
#define PUMP_CONFIG_H

#include "DoseChannel.h"
#include <stddef.h>

constexpr size_t PUMP_LABEL_LEN = 16;

/** Caudal calibrado, etiqueta y totalizador de bombas (NVS). */
namespace PumpConfig {
void begin();
void load();
void save();

float flowMlPerMin(DoseChannel ch);
void setFlowMlPerMin(DoseChannel ch, float mlPerMin);

/** Aplica medida tras corrida de calibSec segundos → ml/min. */
void applyCalibMeasure(DoseChannel ch, float measuredMl, float calibSec);

/** Etiqueta local de bomba (puede estar vacía). */
const char *label(DoseChannel ch);
/** Guarda etiqueta y, si hay un solo nutriente en esa bomba, espeja el nombre. */
void setLabel(DoseChannel ch, const char *name);
/** Solo NVS — sin espejo a nutrientes (evita reentrada). */
void setLabelLocal(DoseChannel ch, const char *name);

/** Título UI: label PumpConfig → nutriente (1:1) → bombaN. */
void formatTitle(DoseChannel ch, char *buf, size_t n);

float totalMl(DoseChannel ch);
void addDispensed(DoseChannel ch, float ml);
void resetTotal(DoseChannel ch);
}

#endif
