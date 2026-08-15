#include "NutrientConfig.h"
#include "PumpConfig.h"
#include "AppStrings.h"
#include "DataStore.h"
#include "MasterLink.h"
#include "Config.h"
#include <Preferences.h>
#include <cstdio>
#include <cstring>

namespace {

constexpr const char *NS = PREF_NAMESPACE;

float ecLo_ = 1580.0f;
float ecHi_ = 1600.0f;
float phLo_ = 5.95f;
float phHi_ = 6.05f;
float recipeEcUs_ = 0.0f;

struct Item {
    char name[NUTRIENT_NAME_LEN];
    float mlPerL;
    uint8_t relayNumber;
};

Item items_[NUTRIENT_MAX] = {};
size_t count_ = 0;
uint8_t phUpRelay_ = 0;
uint8_t phDownRelay_ = 0;

float clampf(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

uint8_t clampRelayOrZero(uint8_t r) {
    if (r > PUMP_RELAY_COUNT) {
        return 0;
    }
    return r;
}

/** Quita etiqueta Manual si la bomba ya no tiene nutriente ni rol pH. */
void clearPumpLabelIfNoNutrient(uint8_t relayNumber) {
    if (relayNumber < 1 || relayNumber > PUMP_RELAY_COUNT) {
        return;
    }
    if (phUpRelay_ == relayNumber || phDownRelay_ == relayNumber) {
        return;
    }
    if (NutrientConfig::relaySharedByNutrient(relayNumber)) {
        return;
    }
    PumpConfig::setLabelLocal(doseFromRelayNumber(relayNumber), "");
}

/** Nutriente → Manual: si es el único en esa bomba, copia el nombre a PumpConfig. */
void mirrorNameToPumpIfSole(size_t ix) {
    if (ix >= count_ || items_[ix].name[0] == '\0') {
        return;
    }
    const uint8_t r = items_[ix].relayNumber;
    if (r < 1 || r > PUMP_RELAY_COUNT) {
        return;
    }
    size_t hits = 0;
    for (size_t i = 0; i < count_; ++i) {
        if (items_[i].relayNumber == r) {
            ++hits;
        }
    }
    if (hits == 1) {
        PumpConfig::setLabelLocal(doseFromRelayNumber(r), items_[ix].name);
    }
}

void persistAndSync() {
    NutrientConfig::save();
    NutrientConfig::syncToMaster();
}

void persistPhAndSync() {
    NutrientConfig::save();
    NutrientConfig::syncToMaster();
    NutrientConfig::syncPhRelaysToMaster();
}

}  // namespace

void NutrientConfig::begin() {
    load();
    syncBandsToStore();
}

void NutrientConfig::load() {
    count_ = 0;
    Preferences prefs;
    if (!prefs.begin(NS, true)) {
        return;
    }
    ecLo_ = prefs.getFloat("nec_lo", 1580.0f);
    ecHi_ = prefs.getFloat("nec_hi", 1600.0f);
    phLo_ = prefs.getFloat("nph_lo", 5.95f);
    phHi_ = prefs.getFloat("nph_hi", 6.05f);
    phUpRelay_ = clampRelayOrZero(prefs.getUChar("nph_up_r", 0));
    phDownRelay_ = clampRelayOrZero(prefs.getUChar("nph_dn_r", 0));
    recipeEcUs_ = clampf(prefs.getFloat("nrec_ec", 0.0f), 0.0f, 5000.0f);

    count_ = prefs.getUChar("nut_n", 0);
    if (count_ > NUTRIENT_MAX) {
        count_ = NUTRIENT_MAX;
    }
    for (size_t i = 0; i < count_; ++i) {
        char nk[12];
        char mk[12];
        char rk[12];
        snprintf(nk, sizeof(nk), "nut_%u_n", static_cast<unsigned>(i));
        snprintf(mk, sizeof(mk), "nut_%u_m", static_cast<unsigned>(i));
        snprintf(rk, sizeof(rk), "nut_%u_r", static_cast<unsigned>(i));
        String nm = prefs.getString(nk, "");
        strncpy(items_[i].name, nm.c_str(), NUTRIENT_NAME_LEN - 1);
        items_[i].name[NUTRIENT_NAME_LEN - 1] = '\0';
        if (items_[i].name[0] == '\0') {
            snprintf(items_[i].name, NUTRIENT_NAME_LEN, "Nut%u", static_cast<unsigned>(i + 1));
        }
        items_[i].mlPerL = prefs.getFloat(mk, 1.0f);
        items_[i].relayNumber = prefs.getUChar(rk, static_cast<uint8_t>(i + 1));
        if (items_[i].relayNumber < 1 || items_[i].relayNumber > PUMP_RELAY_COUNT) {
            items_[i].relayNumber = static_cast<uint8_t>(i + 1);
        }
    }
    prefs.end();

    if (ecHi_ < ecLo_) {
        const float t = ecLo_;
        ecLo_ = ecHi_;
        ecHi_ = t;
    }
    if (phHi_ < phLo_) {
        const float t = phLo_;
        phLo_ = phHi_;
        phHi_ = t;
    }
    /* Alinear etiquetas Manual (1:1) tras NVS. PumpConfig::begin() debe ir antes. */
    for (size_t i = 0; i < count_; ++i) {
        mirrorNameToPumpIfSole(i);
    }
    /* Dedupe legacy: un nutriente por bomba. */
    bool dirty = false;
    for (size_t i = 0; i < count_; ++i) {
        for (size_t j = 0; j < i; ++j) {
            if (items_[i].relayNumber == items_[j].relayNumber) {
                const uint8_t freeR = NutrientConfig::firstFreeRelay();
                if (freeR >= 1 && freeR <= PUMP_RELAY_COUNT) {
                    items_[i].relayNumber = freeR;
                } else {
                    /* Sin bomba libre: desasignar (0) — UI pedirá reasignar. */
                    items_[i].relayNumber = 0;
                }
                dirty = true;
                break;
            }
        }
    }
    clearPhRelayNutrientConflicts();
    if (dirty) {
        /* Re-persist sin recursión infinita: save() llama clearPh otra vez (ok). */
        Preferences prefs;
        if (prefs.begin(NS, false)) {
            prefs.putUChar("nut_n", static_cast<uint8_t>(count_));
            for (size_t i = 0; i < count_; ++i) {
                char rk[12];
                snprintf(rk, sizeof(rk), "nut_%u_r", static_cast<unsigned>(i));
                prefs.putUChar(rk, items_[i].relayNumber);
            }
            prefs.end();
        }
    }
    /* Sin seed A/B/CalMag: lista vacía si nut_n=0. */
}

void NutrientConfig::save() {
    Preferences prefs;
    if (!prefs.begin(NS, false)) {
        return;
    }
    prefs.putFloat("nec_lo", ecLo_);
    prefs.putFloat("nec_hi", ecHi_);
    prefs.putFloat("nph_lo", phLo_);
    prefs.putFloat("nph_hi", phHi_);
    prefs.putUChar("nph_up_r", phUpRelay_);
    prefs.putUChar("nph_dn_r", phDownRelay_);
    prefs.putFloat("nrec_ec", recipeEcUs_);
    prefs.putUChar("nut_n", static_cast<uint8_t>(count_));
    for (size_t i = 0; i < count_; ++i) {
        char nk[12];
        char mk[12];
        char rk[12];
        snprintf(nk, sizeof(nk), "nut_%u_n", static_cast<unsigned>(i));
        snprintf(mk, sizeof(mk), "nut_%u_m", static_cast<unsigned>(i));
        snprintf(rk, sizeof(rk), "nut_%u_r", static_cast<unsigned>(i));
        prefs.putString(nk, items_[i].name);
        prefs.putFloat(mk, items_[i].mlPerL);
        prefs.putUChar(rk, items_[i].relayNumber);
    }
    prefs.end();
    clearPhRelayNutrientConflicts();
}

float NutrientConfig::ecLow() { return ecLo_; }
float NutrientConfig::ecHigh() { return ecHi_; }
float NutrientConfig::phLow() { return phLo_; }
float NutrientConfig::phHigh() { return phHi_; }

void NutrientConfig::setEcLow(float v) {
    ecLo_ = clampf(v, 0.0f, 5000.0f);
    if (ecHi_ < ecLo_) {
        ecHi_ = ecLo_;
    }
    save();
    syncBandsToStore();
}

void NutrientConfig::setEcHigh(float v) {
    ecHi_ = clampf(v, 0.0f, 5000.0f);
    if (ecLo_ > ecHi_) {
        ecLo_ = ecHi_;
    }
    save();
    syncBandsToStore();
}

void NutrientConfig::setPhLow(float v) {
    phLo_ = clampf(v, 0.0f, 14.0f);
    if (phHi_ < phLo_) {
        phHi_ = phLo_;
    }
    save();
    syncBandsToStore();
}

void NutrientConfig::setPhHigh(float v) {
    phHi_ = clampf(v, 0.0f, 14.0f);
    if (phLo_ > phHi_) {
        phLo_ = phHi_;
    }
    save();
    syncBandsToStore();
}

void NutrientConfig::syncBandsToStore() {
    DataStore &store = DataStore::instance();
    ParamConfig ec = store.config(ParamId::Ec);
    ec.low = ecLo_;
    ec.high = ecHi_;
    ec.setpoint = (ecLo_ + ecHi_) * 0.5f;
    store.setConfig(ParamId::Ec, ec);

    ParamConfig ph = store.config(ParamId::Ph);
    ph.low = phLo_;
    ph.high = phHi_;
    ph.setpoint = (phLo_ + phHi_) * 0.5f;
    store.setConfig(ParamId::Ph, ph);
}

size_t NutrientConfig::listCount() { return count_; }

const char *NutrientConfig::listName(size_t ix) {
    if (ix >= count_) {
        return "";
    }
    return items_[ix].name;
}

float NutrientConfig::listMlPerL(size_t ix) {
    if (ix >= count_) {
        return 0.0f;
    }
    return items_[ix].mlPerL;
}

uint8_t NutrientConfig::listRelayNumber(size_t ix) {
    if (ix >= count_) {
        return 0;
    }
    return items_[ix].relayNumber;
}

bool NutrientConfig::relaySharedByNutrient(uint8_t relayNumber, size_t exceptIx) {
    if (relayNumber < 1 || relayNumber > PUMP_RELAY_COUNT) {
        return false;
    }
    for (size_t i = 0; i < count_; ++i) {
        if (i == exceptIx) {
            continue;
        }
        if (items_[i].relayNumber == relayNumber) {
            return true;
        }
    }
    return false;
}

bool NutrientConfig::relayInUse(uint8_t relayNumber, size_t exceptIx) {
    if (relayNumber < 1 || relayNumber > PUMP_RELAY_COUNT) {
        return false;
    }
    if (phUpRelay_ == relayNumber || phDownRelay_ == relayNumber) {
        return true;
    }
    return relaySharedByNutrient(relayNumber, exceptIx);
}

uint8_t NutrientConfig::firstFreeRelay() {
    for (uint8_t r = 1; r <= PUMP_RELAY_COUNT; ++r) {
        if (!relayInUse(r)) {
            return r;
        }
    }
    return 0;
}

uint8_t NutrientConfig::phUpRelay() { return phUpRelay_; }
uint8_t NutrientConfig::phDownRelay() { return phDownRelay_; }

void NutrientConfig::setPhUpRelay(uint8_t relay1to6OrZero) {
    const uint8_t r = clampRelayOrZero(relay1to6OrZero);
    if (r == 0) {
        phUpRelay_ = 0;
        persistPhAndSync();
        return;
    }
    if (relaySharedByNutrient(r)) {
        return;
    }
    if (phDownRelay_ == r) {
        phDownRelay_ = 0;
    }
    phUpRelay_ = r;
    persistPhAndSync();
    PumpConfig::setLabel(doseFromRelayNumber(r), Strings::tr(Msg::PhUpLabel));
}

void NutrientConfig::setPhDownRelay(uint8_t relay1to6OrZero) {
    const uint8_t r = clampRelayOrZero(relay1to6OrZero);
    if (r == 0) {
        phDownRelay_ = 0;
        persistPhAndSync();
        return;
    }
    if (relaySharedByNutrient(r)) {
        return;
    }
    if (phUpRelay_ == r) {
        phUpRelay_ = 0;
    }
    phDownRelay_ = r;
    persistPhAndSync();
    PumpConfig::setLabel(doseFromRelayNumber(r), Strings::tr(Msg::PhDownLabel));
}

void NutrientConfig::clearPhRelayNutrientConflicts() {
    bool changed = false;
    if (phUpRelay_ >= 1 && relaySharedByNutrient(phUpRelay_)) {
        phUpRelay_ = 0;
        changed = true;
    }
    if (phDownRelay_ >= 1 && relaySharedByNutrient(phDownRelay_)) {
        phDownRelay_ = 0;
        changed = true;
    }
    if (changed) {
        persistPhAndSync();
    }
}

bool NutrientConfig::listAdd(const char *name, float mlPerL) {
    if (count_ >= NUTRIENT_MAX) {
        return false;
    }
    uint8_t freeR = firstFreeRelay();
    if (freeR == 0) {
        /* Sin bomba libre (todas con nutriente o pH). */
        return false;
    }
    Item &it = items_[count_];
    memset(it.name, 0, sizeof(it.name));
    if (name && name[0]) {
        strncpy(it.name, name, NUTRIENT_NAME_LEN - 1);
    } else {
        snprintf(it.name, NUTRIENT_NAME_LEN, "Nut%u", static_cast<unsigned>(count_ + 1));
    }
    it.mlPerL = clampf(mlPerL, 0.0f, 100.0f);
    it.relayNumber = freeR;
    ++count_;
    persistAndSync();
    mirrorNameToPumpIfSole(count_ - 1);
    return true;
}

bool NutrientConfig::listSetName(size_t ix, const char *name) {
    if (ix >= count_ || !name) {
        return false;
    }
    strncpy(items_[ix].name, name, NUTRIENT_NAME_LEN - 1);
    items_[ix].name[NUTRIENT_NAME_LEN - 1] = '\0';
    if (items_[ix].name[0] == '\0') {
        snprintf(items_[ix].name, NUTRIENT_NAME_LEN, "Nut%u", static_cast<unsigned>(ix + 1));
    }
    persistAndSync();
    mirrorNameToPumpIfSole(ix);
    return true;
}

bool NutrientConfig::listSetMlPerL(size_t ix, float mlPerL) {
    if (ix >= count_) {
        return false;
    }
    items_[ix].mlPerL = clampf(mlPerL, 0.0f, 100.0f);
    items_[ix].mlPerL = static_cast<float>(static_cast<int>(items_[ix].mlPerL * 10.0f + 0.5f)) / 10.0f;
    persistAndSync();
    return true;
}

bool NutrientConfig::listSetRelayNumber(size_t ix, uint8_t relayNumber) {
    if (ix >= count_) {
        return false;
    }
    if (relayNumber < 1 || relayNumber > PUMP_RELAY_COUNT) {
        return false;
    }
    if (phUpRelay_ == relayNumber || phDownRelay_ == relayNumber) {
        return false;
    }
    if (relaySharedByNutrient(relayNumber, ix)) {
        return false;
    }
    const uint8_t oldRelay = items_[ix].relayNumber;
    items_[ix].relayNumber = relayNumber;
    persistAndSync();
    mirrorNameToPumpIfSole(ix);
    if (oldRelay != relayNumber) {
        clearPumpLabelIfNoNutrient(oldRelay);
    }
    return true;
}

bool NutrientConfig::listRemove(size_t ix) {
    if (ix >= count_) {
        return false;
    }
    const uint8_t oldRelay = items_[ix].relayNumber;
    for (size_t i = ix; i + 1 < count_; ++i) {
        items_[i] = items_[i + 1];
    }
    --count_;
    persistAndSync();
    clearPumpLabelIfNoNutrient(oldRelay);
    return true;
}

float NutrientConfig::recipeEcUs() { return recipeEcUs_; }

void NutrientConfig::setRecipeEcUs(float us) {
    recipeEcUs_ = clampf(us, 0.0f, 5000.0f);
    persistAndSync();
}

float NutrientConfig::totalMlPerL() {
    float sum = 0.0f;
    for (size_t i = 0; i < count_; ++i) {
        if (items_[i].mlPerL > 0.0f && items_[i].name[0] != '\0') {
            sum += items_[i].mlPerL;
        }
    }
    return sum;
}

float NutrientConfig::listProportion(size_t ix) {
    if (ix >= count_) {
        return 0.0f;
    }
    const float total = totalMlPerL();
    if (total <= 0.0f || items_[ix].mlPerL <= 0.0f) {
        return 0.0f;
    }
    return items_[ix].mlPerL / total;
}

float NutrientConfig::listProportionPct(size_t ix) {
    return listProportion(ix) * 100.0f;
}

float NutrientConfig::mlPerLForChannel(DoseChannel ch) {
    const uint8_t r = doseRelayNumber(ch);
    for (size_t i = 0; i < count_; ++i) {
        if (items_[i].relayNumber == r) {
            return items_[i].mlPerL;
        }
    }
    return 0.0f;
}

const char *NutrientConfig::nameForRelay(uint8_t relay1to6) {
    if (relay1to6 < 1 || relay1to6 > PUMP_RELAY_COUNT) {
        return nullptr;
    }
    for (size_t i = 0; i < count_; ++i) {
        if (items_[i].relayNumber == relay1to6 && items_[i].name[0] != '\0') {
            return items_[i].name;
        }
    }
    return nullptr;
}

void NutrientConfig::syncToMaster() { MasterLink::sendNutrientProportions(); }

void NutrientConfig::syncPhRelaysToMaster() { MasterLink::sendLoopControl(); }
