#ifndef DISPLAY_PREFS_H
#define DISPLAY_PREFS_H

#include "DataStore.h"

/** Qué lecturas mostrar en Monitoring (NVS). Off → "--". */
namespace DisplayPrefs {
void begin();
void load();
void save();

bool show(ParamId id);
void setShow(ParamId id, bool on);
}

#endif
