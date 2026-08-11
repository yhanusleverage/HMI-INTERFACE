#include "ReservoirConfig.h"
#include "MasterLink.h"
#include "Config.h"
#include <Preferences.h>

namespace {

constexpr const char *NS = PREF_NAMESPACE;

float volumeL_ = 50.0f;
float homoSec_ = 60.0f;
float nutGapSec_ = 3.0f;
float pulseMl_ = 2.0f;
float pulseGapSec_ = 2.0f;
float dosingDelaySec_ = 60.0f;
ReservoirConfig::DosingMode dosingMode_ = ReservoirConfig::DosingMode::Batch;
float autoEcIv_ = 30.0f;
bool autoEc_ = false;
float autoPhIv_ = 30.0f;
bool autoPh_ = false;
float maxStepEc_ = 50.0f; /* % */
float maxStepPh_ = 50.0f;
float dosingConstEc_ = 0.0f; /* 0 = master default / no override */
float dosingConstPh_ = 0.0f;
bool dosingArmed_ = false;
bool autoUiLocked_ = true;
bool consumoDiario_ = false;
bool consumoPh24h_ = false;
bool batchEdit_ = false;

float clampf(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

/** NVS legacy ≤1.0 = fracción; >1 = ya %. Snap 10, tope 100. */
float normalizeAggrPct(float raw) {
    float pct = (raw <= 1.0f) ? (raw * 100.0f) : raw;
    pct = clampf(pct, 0.0f, 100.0f);
    const int step = static_cast<int>((pct + 5.0f) / 10.0f) * 10;
    return static_cast<float>(clampf(static_cast<float>(step), 0.0f, 100.0f));
}

void persistAndSync() {
    if (batchEdit_) {
        return;
    }
    ReservoirConfig::save();
    ReservoirConfig::syncToMaster();
}

}  // namespace

void ReservoirConfig::begin() { load(); }

void ReservoirConfig::load() {
    Preferences prefs;
    if (!prefs.begin(NS, true)) {
        return;
    }

    if (prefs.isKey("res_L")) {
        volumeL_ = prefs.getFloat("res_L", 50.0f);
    } else if (prefs.isKey("res_gal")) {
        volumeL_ = prefs.getFloat("res_gal", 50.0f) * 3.785f;
    } else {
        volumeL_ = 50.0f;
    }

    if (prefs.isKey("res_homo_s")) {
        homoSec_ = prefs.getFloat("res_homo_s", 60.0f);
    } else if (prefs.isKey("res_mix")) {
        homoSec_ = prefs.getFloat("res_mix", 5.0f) * 60.0f;
    } else {
        homoSec_ = 60.0f;
    }

    if (prefs.isKey("res_nut_gap_s")) {
        nutGapSec_ = prefs.getFloat("res_nut_gap_s", 3.0f);
    } else if (prefs.isKey("res_delay")) {
        nutGapSec_ = prefs.getFloat("res_delay", 3.0f);
        if (nutGapSec_ > 60.0f) {
            nutGapSec_ = 3.0f;
        }
    } else {
        nutGapSec_ = 3.0f;
    }

    pulseMl_ = prefs.getFloat("res_pulse_ml", 2.0f);
    pulseGapSec_ = prefs.getFloat("res_pulse_gap_s", 2.0f);
    dosingDelaySec_ = prefs.getFloat("res_dose_delay_s", 60.0f);
    dosingMode_ = static_cast<DosingMode>(prefs.getUChar("res_dose_mode", 0));
    if (dosingMode_ > DosingMode::Recirculating) {
        dosingMode_ = DosingMode::Batch;
    }
    autoEcIv_ = prefs.getFloat("res_ec_iv_s", 30.0f);
    autoEc_ = prefs.getBool("res_auto_ec", false);
    autoPhIv_ = prefs.getFloat("res_ph_iv_s", 30.0f);
    autoPh_ = prefs.getBool("res_auto_ph", false);
    maxStepEc_ = normalizeAggrPct(prefs.getFloat("res_max_step_ec", 50.0f));
    maxStepPh_ = normalizeAggrPct(prefs.getFloat("res_max_step_ph", 50.0f));
    dosingConstEc_ = prefs.getFloat("res_const_ec", 0.0f);
    dosingConstPh_ = prefs.getFloat("res_const_ph", 0.0f);
    dosingArmed_ = prefs.getBool("res_dosing_arm", false);
    autoUiLocked_ = prefs.getBool("res_ui_lock", true);
    consumoDiario_ = prefs.getBool("res_cons_dia", false);
    consumoPh24h_ = prefs.getBool("res_cons_ph", false);
    prefs.end();
}

void ReservoirConfig::save() {
    Preferences prefs;
    if (!prefs.begin(NS, false)) {
        return;
    }
    prefs.putFloat("res_L", volumeL_);
    prefs.putFloat("res_homo_s", homoSec_);
    prefs.putFloat("res_nut_gap_s", nutGapSec_);
    prefs.putFloat("res_pulse_ml", pulseMl_);
    prefs.putFloat("res_pulse_gap_s", pulseGapSec_);
    prefs.putFloat("res_dose_delay_s", dosingDelaySec_);
    prefs.putUChar("res_dose_mode", static_cast<uint8_t>(dosingMode_));
    prefs.putFloat("res_ec_iv_s", autoEcIv_);
    prefs.putBool("res_auto_ec", autoEc_);
    prefs.putFloat("res_ph_iv_s", autoPhIv_);
    prefs.putBool("res_auto_ph", autoPh_);
    prefs.putFloat("res_max_step_ec", maxStepEc_);
    prefs.putFloat("res_max_step_ph", maxStepPh_);
    prefs.putFloat("res_const_ec", dosingConstEc_);
    prefs.putFloat("res_const_ph", dosingConstPh_);
    prefs.putBool("res_dosing_arm", dosingArmed_);
    prefs.putBool("res_ui_lock", autoUiLocked_);
    prefs.putBool("res_cons_dia", consumoDiario_);
    prefs.putBool("res_cons_ph", consumoPh24h_);
    prefs.end();
}

void ReservoirConfig::syncToMaster() { MasterLink::sendLoopControl(); }

void ReservoirConfig::beginBatch() { batchEdit_ = true; }

void ReservoirConfig::endBatch(bool commit) {
    batchEdit_ = false;
    if (commit) {
        save();
        syncToMaster();
    } else {
        load();
    }
}

float ReservoirConfig::volumeL() { return volumeL_; }
float ReservoirConfig::homoSec() { return homoSec_; }
float ReservoirConfig::nutrientGapSec() { return nutGapSec_; }
float ReservoirConfig::pulseMl() { return pulseMl_; }
float ReservoirConfig::pulseGapSec() { return pulseGapSec_; }
float ReservoirConfig::dosingDelaySec() { return dosingDelaySec_; }
ReservoirConfig::DosingMode ReservoirConfig::dosingMode() { return dosingMode_; }
float ReservoirConfig::autoEcIntervalSec() { return autoEcIv_; }
bool ReservoirConfig::autoEcEnabled() { return autoEc_; }
float ReservoirConfig::autoPhIntervalSec() { return autoPhIv_; }
bool ReservoirConfig::autoPhEnabled() { return autoPh_; }
float ReservoirConfig::maxStepEc() { return maxStepEc_; }
float ReservoirConfig::maxStepPh() { return maxStepPh_; }
float ReservoirConfig::dosingConstEc() { return dosingConstEc_; }
float ReservoirConfig::dosingConstPh() { return dosingConstPh_; }
bool ReservoirConfig::dosingArmed() { return dosingArmed_; }
bool ReservoirConfig::autoUiLocked() { return autoUiLocked_; }
bool ReservoirConfig::consumoDiarioEnabled() { return consumoDiario_; }
bool ReservoirConfig::consumoPh24hEnabled() { return consumoPh24h_; }

void ReservoirConfig::setVolumeL(float v) {
    volumeL_ = clampf(v, 1.0f, 10000.0f);
    persistAndSync();
}
void ReservoirConfig::setHomoSec(float v) {
    homoSec_ = clampf(v, 0.0f, 3600.0f);
    persistAndSync();
}
void ReservoirConfig::setNutrientGapSec(float v) {
    nutGapSec_ = clampf(v, 0.0f, 600.0f);
    persistAndSync();
}
void ReservoirConfig::setPulseMl(float v) {
    pulseMl_ = clampf(v, 0.1f, 50.0f);
    pulseMl_ = static_cast<float>(static_cast<int>(pulseMl_ * 10.0f + 0.5f)) / 10.0f;
    persistAndSync();
}
void ReservoirConfig::setPulseGapSec(float v) {
    pulseGapSec_ = clampf(v, 0.0f, 120.0f);
    persistAndSync();
}
void ReservoirConfig::setDosingDelaySec(float v) {
    dosingDelaySec_ = clampf(v, 0.0f, 600.0f);
    persistAndSync();
}
void ReservoirConfig::setDosingMode(DosingMode m) {
    dosingMode_ = m;
    persistAndSync();
}
void ReservoirConfig::setAutoEcIntervalSec(float v) {
    autoEcIv_ = clampf(v, 5.0f, 3600.0f);
    persistAndSync();
}
void ReservoirConfig::setAutoEcEnabled(bool on) {
    autoEc_ = on;
    persistAndSync();
}
void ReservoirConfig::setAutoPhIntervalSec(float v) {
    autoPhIv_ = clampf(v, 5.0f, 3600.0f);
    persistAndSync();
}
void ReservoirConfig::setAutoPhEnabled(bool on) {
    autoPh_ = on;
    persistAndSync();
}
void ReservoirConfig::setMaxStepEc(float pct) {
    maxStepEc_ = normalizeAggrPct(pct);
    persistAndSync();
}
void ReservoirConfig::setMaxStepPh(float pct) {
    maxStepPh_ = normalizeAggrPct(pct);
    persistAndSync();
}
void ReservoirConfig::setDosingConstEc(float v) {
    dosingConstEc_ = clampf(v, 0.0f, 10.0f);
    persistAndSync();
}
void ReservoirConfig::setDosingConstPh(float v) {
    dosingConstPh_ = clampf(v, 0.0f, 100.0f);
    persistAndSync();
}
void ReservoirConfig::setDosingArmed(bool on) {
    dosingArmed_ = on;
    persistAndSync();
}
void ReservoirConfig::setAutoUiLocked(bool on) {
    autoUiLocked_ = on;
    if (batchEdit_) {
        return;
    }
    save();
}

void ReservoirConfig::setConsumoDiarioEnabled(bool on) {
    consumoDiario_ = on;
    persistAndSync();
}

void ReservoirConfig::setConsumoPh24hEnabled(bool on) {
    consumoPh24h_ = on;
    persistAndSync();
}

bool ReservoirConfig::manualDoseAllowed() { return !dosingArmed_; }
