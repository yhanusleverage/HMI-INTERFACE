#include "RulesEngine.h"
#include "RulesConfig.h"
#include "MasterLink.h"
#include "DataStore.h"
#include "SlaveInventory.h"

#include <math.h>
#include <cstring>

namespace {

unsigned long lastEvalMs_ = 0;
unsigned long lastSlavesReqMs_ = 0;

float sensorValue(RulesConfig::Sensor s) {
    DataStore &ds = DataStore::instance();
    switch (s) {
    case RulesConfig::Sensor::Ec:
        return ds.value(ParamId::Ec);
    case RulesConfig::Sensor::Temp:
        return ds.value(ParamId::TempAgua);
    case RulesConfig::Sensor::Orp:
        return ds.value(ParamId::Orp);
    case RulesConfig::Sensor::Do:
        return ds.value(ParamId::Do);
    case RulesConfig::Sensor::L1:
    case RulesConfig::Sensor::L2:
    case RulesConfig::Sensor::L3:
    case RulesConfig::Sensor::L4:
        /* Stub hasta UART de nivel RDWC; no dispara. */
        return NAN;
    case RulesConfig::Sensor::Ph:
    default:
        return ds.value(ParamId::Ph);
    }
}

bool sensorClauseMet(const RulesConfig::CondClause &c) {
    const float v = sensorValue(c.sensor);
    if (isnan(v)) {
        return false;
    }
    const float thr = c.threshold;
    switch (c.op) {
    case RulesConfig::Op::Gte:
        return v >= thr;
    case RulesConfig::Op::Lt:
        return v < thr;
    case RulesConfig::Op::Lte:
        return v <= thr;
    case RulesConfig::Op::Eq:
        return fabsf(v - thr) < 0.05f;
    case RulesConfig::Op::Gt:
    default:
        return v > thr;
    }
}

const SlaveInventory::Target *findTarget(bool local, const char *mac) {
    if (local) {
        const size_t li = SlaveInventory::localIndex();
        if (li == SIZE_MAX) {
            return nullptr;
        }
        return SlaveInventory::at(li);
    }
    if (!mac || !mac[0]) {
        return nullptr;
    }
    const size_t n = SlaveInventory::count();
    for (size_t i = 0; i < n; ++i) {
        const SlaveInventory::Target *t = SlaveInventory::at(i);
        if (t && !t->local && strncmp(t->mac, mac, SlaveInventory::kMacLen) == 0) {
            return t;
        }
    }
    return nullptr;
}

bool relayClauseMet(const RulesConfig::CondClause &c) {
    const SlaveInventory::Target *t = findTarget(c.relayLocal, c.relayMac);
    if (!t) {
        return false;
    }
    if (c.relayIndex >= SlaveInventory::kMaxRelays) {
        return false;
    }
    const bool on = t->relayOn[c.relayIndex];
    return c.relayWantOn ? on : !on;
}

bool clauseMet(const RulesConfig::CondClause &c) {
    if (c.kind == RulesConfig::CondKind::RelayState) {
        return relayClauseMet(c);
    }
    return sensorClauseMet(c);
}

bool ruleConditionMet(const RulesConfig::Rule &r) {
    if (r.condCount == 0) {
        return false;
    }
    if (r.logic == RulesConfig::Logic::Or) {
        for (uint8_t i = 0; i < r.condCount; ++i) {
            if (clauseMet(r.cond[i])) {
                return true;
            }
        }
        return false;
    }
    for (uint8_t i = 0; i < r.condCount; ++i) {
        if (!clauseMet(r.cond[i])) {
            return false;
        }
    }
    return true;
}

void execute(RulesConfig::Rule &r) {
    const char *action = (r.action == RulesConfig::Action::On) ? "on" : "off";
    const int dur = (r.action == RulesConfig::Action::On) ? static_cast<int>(r.durationSec) : 0;
    if (r.localTarget || strcmp(r.mac, "local") == 0) {
        MasterLink::sendRelayLocal(r.relay, action, dur);
    } else {
        MasterLink::sendRelaySlave(r.mac, r.relay, action, dur);
    }
    r.lastFireMs = millis();
}

void sortedEnabled(size_t *out, size_t *nOut) {
    size_t n = 0;
    for (size_t i = 0; i < RulesConfig::kMaxRules; ++i) {
        if (RulesConfig::at(i) && RulesConfig::at(i)->enabled) {
            out[n++] = i;
        }
    }
    for (size_t a = 0; a + 1 < n; ++a) {
        for (size_t b = a + 1; b < n; ++b) {
            const RulesConfig::Rule *ra = RulesConfig::at(out[a]);
            const RulesConfig::Rule *rb = RulesConfig::at(out[b]);
            if (!ra || !rb) {
                continue;
            }
            const uint8_t pa = static_cast<uint8_t>(ra->priority);
            const uint8_t pb = static_cast<uint8_t>(rb->priority);
            if (pb < pa || (pb == pa && out[b] < out[a])) {
                const size_t tmp = out[a];
                out[a] = out[b];
                out[b] = tmp;
            }
        }
    }
    *nOut = n;
}

}  // namespace

void RulesEngine::begin() {
    lastEvalMs_ = 0;
    lastSlavesReqMs_ = 0;
}

void RulesEngine::fire(size_t ruleIndex) {
    RulesConfig::Rule *r = RulesConfig::mutableAt(ruleIndex);
    if (!r) {
        return;
    }
    execute(*r);
}

void RulesEngine::tick() {
    const unsigned long now = millis();

    if (lastSlavesReqMs_ == 0 || (now - lastSlavesReqMs_) > 5000UL) {
        lastSlavesReqMs_ = now;
        if (MasterLink::linkOk()) {
            MasterLink::requestSlaves();
        }
    }

    if (lastEvalMs_ != 0 && (now - lastEvalMs_) < 1000UL) {
        return;
    }
    lastEvalMs_ = now;

    if (!MasterLink::linkOk()) {
        return;
    }

    size_t order[RulesConfig::kMaxRules];
    size_t n = 0;
    sortedEnabled(order, &n);

    for (size_t k = 0; k < n; ++k) {
        RulesConfig::Rule *r = RulesConfig::mutableAt(order[k]);
        if (!r || !r->enabled) {
            continue;
        }
        if (r->cooldownSec > 0 && r->lastFireMs != 0 &&
            (now - r->lastFireMs) < (r->cooldownSec * 1000UL)) {
            continue;
        }
        if (!ruleConditionMet(*r)) {
            continue;
        }
        execute(*r);
    }
}
