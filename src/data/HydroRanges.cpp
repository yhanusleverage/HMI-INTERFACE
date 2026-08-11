#include "HydroRanges.h"
#include <math.h>

namespace HydroRanges {

void applyDefaults(ParamConfig *cfgs) {
    cfgs[static_cast<size_t>(ParamId::Ph)] = {5.8f, 5.5f, 6.5f, 0.0f, 1.0f};
    cfgs[static_cast<size_t>(ParamId::Ec)] = {470.0f, 300.0f, 800.0f, 0.0f, 1.0f};
    cfgs[static_cast<size_t>(ParamId::TempAgua)] = {20.0f, 18.0f, 26.0f, 0.0f, 1.0f};
    cfgs[static_cast<size_t>(ParamId::Orp)] = {360.0f, 250.0f, 450.0f, 0.0f, 1.0f};
    cfgs[static_cast<size_t>(ParamId::Do)] = {9.0f, 6.0f, 12.0f, 0.0f, 1.0f};
}

ParamStatus evaluate(float value, const ParamConfig &cfg) {
    if (isnan(value)) {
        return ParamStatus::Unknown;
    }
    if (value < cfg.low) {
        return ParamStatus::Low;
    }
    if (value > cfg.high) {
        return ParamStatus::High;
    }
    return ParamStatus::Ok;
}

}
