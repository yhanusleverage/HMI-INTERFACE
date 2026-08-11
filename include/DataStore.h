#ifndef DATA_STORE_H
#define DATA_STORE_H

#include <Arduino.h>

enum class ParamId : uint8_t {
    Ph = 0,
    Ec = 1,
    TempAgua = 2,
    Orp = 3,
    Do = 4,
    Count = 5
};

enum class ParamStatus : uint8_t { Ok = 0, Low = 1, High = 2, Unknown = 3 };

enum class DataSource : uint8_t { Sim = 0, Live = 1 };

struct ParamConfig {
    float setpoint;
    float low;
    float high;
    float calibOffset;
    float calibScale;
};

struct TelemetrySnapshot {
    float ph;
    float ec;
    float tempAgua;
    float orp;
    float doMgL;
    unsigned long updatedMs;
    DataSource source;
    bool linkOk;
};

class DataStore {
public:
    static DataStore &instance();

    void begin();
    void loadPrefs();
    void savePrefs();

    TelemetrySnapshot snapshot() const;
    void setTelemetry(float ph, float ec, float tempAgua, float orp, float doMgL, DataSource source);
    void setLinkOk(bool ok);

    ParamConfig config(ParamId id) const;
    void setConfig(ParamId id, const ParamConfig &cfg);
    ParamStatus status(ParamId id) const;
    float value(ParamId id) const;
    const char *name(ParamId id) const;
    const char *unit(ParamId id) const;
    const char *labelWithUnit(ParamId id) const;

private:
    DataStore() = default;
    TelemetrySnapshot tel_{};
    ParamConfig cfgs_[static_cast<size_t>(ParamId::Count)]{};
};

#endif
