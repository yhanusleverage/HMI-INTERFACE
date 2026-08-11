#ifndef HYDRO_RANGES_H
#define HYDRO_RANGES_H

#include "DataStore.h"

namespace HydroRanges {
void applyDefaults(ParamConfig *cfgs);
ParamStatus evaluate(float value, const ParamConfig &cfg);
}

#endif
