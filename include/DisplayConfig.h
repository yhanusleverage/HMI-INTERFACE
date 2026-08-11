#ifndef DISPLAY_CONFIG_H
#define DISPLAY_CONFIG_H

#include "DataStore.h"

/** Toggles Display Readings (NVS): si off, Central muestra "--". */
namespace DisplayConfig {
void begin();
void load();
void save();

bool showPh();
bool showEc();
bool showTemp();
bool showOrp();
bool showDo();

void setShowPh(bool on);
void setShowEc(bool on);
void setShowTemp(bool on);
void setShowOrp(bool on);
void setShowDo(bool on);

bool showParam(ParamId id);
}

#endif
