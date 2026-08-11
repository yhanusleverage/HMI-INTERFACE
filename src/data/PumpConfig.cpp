#include "PumpConfig.h"
#include "Config.h"
#include "NutrientConfig.h"
#include "AppStrings.h"
#include <Preferences.h>
#include <cstdio>
#include <cstring>

namespace {

float flow_[PUMP_RELAY_COUNT] = {50.0f, 50.0f, 50.0f, 50.0f, 50.0f, 50.0f};
float totalMl_[PUMP_RELAY_COUNT] = {0, 0, 0, 0, 0, 0};
char label_[PUMP_RELAY_COUNT][PUMP_LABEL_LEN] = {};
constexpr const char *NS = PREF_NAMESPACE;

float clampf(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

}  // namespace

void PumpConfig::begin() { load(); }

void PumpConfig::load() {
    Preferences prefs;
    if (!prefs.begin(NS, true)) {
        return;
    }
    for (uint8_t i = 0; i < PUMP_RELAY_COUNT; ++i) {
        char key[12];
        snprintf(key, sizeof(key), "pf_%u", static_cast<unsigned>(i));
        flow_[i] = prefs.getFloat(key, 50.0f);
        snprintf(key, sizeof(key), "pt_%u", static_cast<unsigned>(i));
        totalMl_[i] = prefs.getFloat(key, 0.0f);
        snprintf(key, sizeof(key), "pn_%u", static_cast<unsigned>(i));
        String s = prefs.getString(key, "");
        memset(label_[i], 0, PUMP_LABEL_LEN);
        if (s.length() > 0) {
            strncpy(label_[i], s.c_str(), PUMP_LABEL_LEN - 1);
        }
    }
    prefs.end();
}

void PumpConfig::save() {
    Preferences prefs;
    if (!prefs.begin(NS, false)) {
        return;
    }
    for (uint8_t i = 0; i < PUMP_RELAY_COUNT; ++i) {
        char key[12];
        snprintf(key, sizeof(key), "pf_%u", static_cast<unsigned>(i));
        prefs.putFloat(key, flow_[i]);
        snprintf(key, sizeof(key), "pt_%u", static_cast<unsigned>(i));
        prefs.putFloat(key, totalMl_[i]);
        snprintf(key, sizeof(key), "pn_%u", static_cast<unsigned>(i));
        prefs.putString(key, label_[i]);
    }
    prefs.end();
}

float PumpConfig::flowMlPerMin(DoseChannel ch) {
    const uint8_t ix = static_cast<uint8_t>(ch);
    if (ix >= PUMP_RELAY_COUNT) {
        return 50.0f;
    }
    return flow_[ix];
}

void PumpConfig::setFlowMlPerMin(DoseChannel ch, float mlPerMin) {
    const uint8_t ix = static_cast<uint8_t>(ch);
    if (ix >= PUMP_RELAY_COUNT) {
        return;
    }
    flow_[ix] = clampf(mlPerMin, 1.0f, 500.0f);
    flow_[ix] = static_cast<float>(static_cast<int>(flow_[ix] * 10.0f + 0.5f)) / 10.0f;
    save();
}

void PumpConfig::applyCalibMeasure(DoseChannel ch, float measuredMl, float calibSec) {
    if (calibSec < 1.0f) {
        return;
    }
    setFlowMlPerMin(ch, measuredMl * (60.0f / calibSec));
}

const char *PumpConfig::label(DoseChannel ch) {
    const uint8_t ix = static_cast<uint8_t>(ch);
    if (ix >= PUMP_RELAY_COUNT) {
        return "";
    }
    return label_[ix];
}

void PumpConfig::setLabelLocal(DoseChannel ch, const char *name) {
    const uint8_t ix = static_cast<uint8_t>(ch);
    if (ix >= PUMP_RELAY_COUNT) {
        return;
    }
    memset(label_[ix], 0, PUMP_LABEL_LEN);
    if (name && name[0]) {
        strncpy(label_[ix], name, PUMP_LABEL_LEN - 1);
        label_[ix][PUMP_LABEL_LEN - 1] = '\0';
    }
    save();
}

void PumpConfig::setLabel(DoseChannel ch, const char *name) {
    setLabelLocal(ch, name);
    /* Espejo Manual → Nutriente solo si hay exactamente un nutriente en esa bomba. */
    const uint8_t relay = doseRelayNumber(ch);
    size_t hits = 0;
    size_t hitIx = SIZE_MAX;
    const size_t nItems = NutrientConfig::listCount();
    for (size_t i = 0; i < nItems; ++i) {
        if (NutrientConfig::listRelayNumber(i) == relay) {
            ++hits;
            hitIx = i;
        }
    }
    if (hits == 1 && hitIx != SIZE_MAX && label(ch)[0]) {
        NutrientConfig::listSetName(hitIx, label(ch));
    }
}

void PumpConfig::formatTitle(DoseChannel ch, char *buf, size_t n) {
    if (!buf || n == 0) {
        return;
    }
    const char *loc = label(ch);
    if (loc && loc[0]) {
        snprintf(buf, n, "%s", loc);
        return;
    }
    const uint8_t r = doseRelayNumber(ch);
    /* Un solo nutriente en la bomba → su nombre; si 0 o varios → bombaN. */
    size_t hits = 0;
    const char *only = nullptr;
    const size_t nItems = NutrientConfig::listCount();
    for (size_t i = 0; i < nItems; ++i) {
        if (NutrientConfig::listRelayNumber(i) == r) {
            ++hits;
            only = NutrientConfig::listName(i);
        }
    }
    if (hits == 1 && only && only[0]) {
        snprintf(buf, n, "%s", only);
        return;
    }
    snprintf(buf, n, Strings::tr(Msg::PumpNameFmt), static_cast<int>(r));
}

float PumpConfig::totalMl(DoseChannel ch) {
    const uint8_t ix = static_cast<uint8_t>(ch);
    if (ix >= PUMP_RELAY_COUNT) {
        return 0.0f;
    }
    return totalMl_[ix];
}

void PumpConfig::addDispensed(DoseChannel ch, float ml) {
    const uint8_t ix = static_cast<uint8_t>(ch);
    if (ix >= PUMP_RELAY_COUNT || ml <= 0.0f) {
        return;
    }
    totalMl_[ix] = clampf(totalMl_[ix] + ml, 0.0f, 1.0e7f);
    save();
}

void PumpConfig::resetTotal(DoseChannel ch) {
    const uint8_t ix = static_cast<uint8_t>(ch);
    if (ix >= PUMP_RELAY_COUNT) {
        return;
    }
    totalMl_[ix] = 0.0f;
    save();
}
