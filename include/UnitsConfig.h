#ifndef UNITS_CONFIG_H
#define UNITS_CONFIG_H

#include <stddef.h>
#include <stdint.h>

enum class EcUnit : uint8_t { Us = 0, Ppm500 = 1, Ppm640 = 2 };
enum class TempUnit : uint8_t { Celsius = 0, Fahrenheit = 1 };

/** Unidades Display (NVS) — Central formatea según flags. */
namespace UnitsConfig {
void begin();
void load();
void save();

EcUnit ecUnit();
TempUnit tempUnit();
void setEcUnit(EcUnit u);
void setTempUnit(TempUnit u);

/** Valor interno EC siempre en uS. */
void formatEc(float us, char *buf, size_t n);
/** Valor interno temp siempre en °C. */
void formatTemp(float celsius, char *buf, size_t n);
const char *ecUnitSuffix();
const char *tempUnitSuffix();
}

#endif
