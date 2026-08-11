#ifndef RULES_CONFIG_H
#define RULES_CONFIG_H

#include <Arduino.h>
#include "SlaveInventory.h"

/**
 * Reglas HMI NVS — condiciones 1–3 (Sensor | Relé ON/OFF) con AND/OR plano.
 * Alineado con HIDROWAVE composite + relay_state (subset).
 */
namespace RulesConfig {

constexpr size_t kMaxRules = 8;
constexpr uint8_t kMaxConds = 3;
constexpr size_t kNameLen = 20;

enum class Sensor : uint8_t {
    Ph = 0,
    Ec = 1,
    Temp = 2,
    Orp = 3,
    Do = 4,
    L1 = 5,
    L2 = 6,
    L3 = 7,
    L4 = 8,
    Count = 9
};

enum class Op : uint8_t {
    Gt = 0,
    Gte = 1,
    Lt = 2,
    Lte = 3,
    Eq = 4,
    Count = 5
};

enum class Priority : uint8_t {
    Critical = 0,
    High = 1,
    Normal = 2,
    Low = 3,
    Count = 4
};

enum class Action : uint8_t { On = 0, Off = 1 };

enum class CondKind : uint8_t { Sensor = 0, RelayState = 1 };
enum class Logic : uint8_t { And = 0, Or = 1 };

struct CondClause {
    CondKind kind;
    Sensor sensor;
    Op op;
    float threshold;
    bool relayLocal;
    char relayMac[SlaveInventory::kMacLen];
    uint8_t relayIndex;
    bool relayWantOn;
};

struct Rule {
    bool used;
    bool enabled;
    char name[kNameLen];
    Priority priority;
    CondClause cond[kMaxConds];
    uint8_t condCount; /* 1..3 */
    Logic logic;
    bool localTarget; /* acción: false = Atlas ESP-NOW (cargas); true legacy Master */
    char mac[SlaveInventory::kMacLen];
    uint8_t relay;
    Action action;
    uint16_t durationSec;
    uint32_t cooldownSec;
    unsigned long lastFireMs;
};

void begin();
void load();
void save();

size_t count();
const Rule *at(size_t i);
Rule *mutableAt(size_t i);

size_t addDefault();
bool removeAt(size_t i);
void setEnabled(size_t i, bool on);
bool setName(size_t i, const char *name);

void cycleTarget(size_t i);
void cycleRelay(size_t i);
void cyclePriority(size_t i);
void cycleAction(size_t i);
void adjustDurationSec(size_t i, int delta);
void adjustCooldownSec(size_t i, int delta);

/** Operan sobre cond[0] si es Sensor (compat UI legacy helpers). */
void cycleSensor(size_t i);
void cycleOp(size_t i);
void adjustThreshold(size_t i, float delta);

void initClauseSensor(CondClause &c);
void initClauseRelay(CondClause &c);
bool addClause(size_t ruleIx);
bool removeClause(size_t ruleIx);
void setLogic(size_t ruleIx, Logic logic);

void formatSummary(size_t i, char *buf, size_t n);

}  // namespace RulesConfig

#endif
