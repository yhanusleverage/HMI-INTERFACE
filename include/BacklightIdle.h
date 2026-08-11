#ifndef BACKLIGHT_IDLE_H
#define BACKLIGHT_IDLE_H

#include <Arduino.h>

/**
 * Política backlight HMI:
 * - AlwaysOn / AutoOff (60 s) / ForcedOff
 * - Auto-off solo sin alarma de proceso (pH·EC·ORP·DO BAJO/ALTO)
 * - Alarma → fuerza ON; al limpiar, deja ON (ForcedOff migra a AlwaysOn)
 * - Primer toque con BL apagado = solo wake (no click)
 */
namespace BacklightIdle {

enum class Mode : uint8_t { AlwaysOn = 0, AutoOff = 1, ForcedOff = 2 };

void begin();
void tick();

Mode mode();
void setMode(Mode m);

bool lit();
uint32_t idleTimeoutMs();

/** true = tragar el gesto (wake-only). Llamar desde touchRead. */
bool onTouchSample(bool pressed);

/** Debug / UI: aplica lit respetando alarma. */
void setLit(bool on);

bool processAlarmActive();

}  // namespace BacklightIdle

#endif
