#ifndef APP_LOCALE_H
#define APP_LOCALE_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

enum class AppLang : uint8_t { Es = 0, En = 1, Pt = 2 };

/** Idioma + TZ (ciudad/region) + reloj vía NTP. */
namespace AppLocale {
void begin();
void load();
void save();

AppLang language();
void setLanguage(AppLang lang);
const char *languageName(AppLang lang);

/** Offset UTC en minutos (ej. -180). Persistido NVS `tz_min`. */
int16_t tzOffsetMin();
void setTzOffsetMin(int16_t min);
void applyTzToSystem();
void syncNtpIfOnline();

bool setupDone();
void setSetupDone(bool done);

void formatNow(char *buf, size_t len);
void formatNowClock(char *buf, size_t len);
void formatNowDate(char *buf, size_t len);

/** Nombre de ciudad/region para un offset (usuario promedio). */
void formatTzLabel(char *buf, size_t len, int16_t min);

/** Lista curada LatAm-first: ciudad/region → offset. */
int tzPresetCount();
int16_t tzPresetMin(int index);
/** Etiqueta legible segun idioma actual. */
const char *tzPresetLabel(int index);
/** Indice del preset que coincide con offset (o default Brasilia). */
int tzPresetIndexForMin(int16_t min);
}

#endif
