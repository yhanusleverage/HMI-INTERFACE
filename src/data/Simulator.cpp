#include "Simulator.h"
#include "DataStore.h"
#include "Config.h"
#include <math.h>

static unsigned long lastTickMs = 0;

void Simulator::begin() {
    lastTickMs = 0;
    DataStore::instance().setTelemetry(5.8f, 470.0f, 20.0f, 362.0f, 9.1f, DataSource::Sim);
}

void Simulator::tick() {
#if !DATA_SOURCE_SIM
    return;
#endif
    const unsigned long now = millis();
    if (lastTickMs != 0 && (now - lastTickMs) < SIM_TICK_MS) {
        return;
    }
    lastTickMs = now;

    DataStore &store = DataStore::instance();
    const float t = now / 1000.0f;
    const ParamConfig ph = store.config(ParamId::Ph);
    const ParamConfig ec = store.config(ParamId::Ec);
    const ParamConfig tw = store.config(ParamId::TempAgua);
    const ParamConfig orp = store.config(ParamId::Orp);
    const ParamConfig dox = store.config(ParamId::Do);

    const float phVal = ph.setpoint + 0.25f * sinf(t * 0.22f);
    const float ecVal = ec.setpoint + 40.0f * sinf(t * 0.13f + 1.0f);
    const float tempVal = tw.setpoint + 1.2f * sinf(t * 0.09f + 0.4f);
    const float orpVal = orp.setpoint + 25.0f * sinf(t * 0.11f + 0.7f);
    const float doVal = dox.setpoint + 0.6f * sinf(t * 0.15f + 1.2f);

    store.setTelemetry(phVal, ecVal, tempVal, orpVal, doVal, DataSource::Sim);
}
