#ifndef DOSE_CHANNEL_H
#define DOSE_CHANNEL_H

#include "AppStrings.h"
#include <stdint.h>
#include <stdio.h>

/** 6 bombas peristálticas Master (índice 0..5; wire relayNumber 1..6 / channel R1..R6). */
constexpr uint8_t PUMP_RELAY_COUNT = 6;

enum class DoseChannel : uint8_t {
    R1 = 0,
    R2 = 1,
    R3 = 2,
    R4 = 3,
    R5 = 4,
    R6 = 5,
    Count = 6
};

inline uint8_t doseRelayNumber(DoseChannel ch) {
    return static_cast<uint8_t>(ch) + 1;
}

inline DoseChannel doseFromRelayNumber(uint8_t relay1to6) {
    if (relay1to6 < 1) {
        relay1to6 = 1;
    }
    if (relay1to6 > PUMP_RELAY_COUNT) {
        relay1to6 = PUMP_RELAY_COUNT;
    }
    return static_cast<DoseChannel>(relay1to6 - 1);
}

/** Valida relay 1..6; false si relay es 0 o inválido. */
inline bool tryDoseFromRelayNumber(uint8_t relay1to6, DoseChannel *out) {
    if (relay1to6 < 1 || relay1to6 > PUMP_RELAY_COUNT || !out) {
        return false;
    }
    *out = static_cast<DoseChannel>(relay1to6 - 1);
    return true;
}

/** Clave UART hacia master (protocolo: "R1".."R6"). */
inline const char *doseChannelKey(DoseChannel ch) {
    static const char *const keys[PUMP_RELAY_COUNT] = {"R1", "R2", "R3", "R4", "R5", "R6"};
    const uint8_t ix = static_cast<uint8_t>(ch);
    if (ix >= PUMP_RELAY_COUNT) {
        return "R1";
    }
    return keys[ix];
}

/** Etiqueta UI i18n: bomba1…bomba6 / pump1…pump6. */
inline const char *doseChannelLabel(DoseChannel ch) {
    static char labels[PUMP_RELAY_COUNT][16];
    const uint8_t ix = static_cast<uint8_t>(ch);
    if (ix >= PUMP_RELAY_COUNT) {
        snprintf(labels[0], sizeof(labels[0]), Strings::tr(Msg::PumpNameFmt), 1);
        return labels[0];
    }
    snprintf(labels[ix], sizeof(labels[ix]), Strings::tr(Msg::PumpNameFmt),
             static_cast<int>(ix + 1));
    return labels[ix];
}

/** Escribe bombaN / pumpN en buf (pump1to6 = 1..6). */
inline void dosePumpTag(uint8_t pump1to6, char *buf, size_t n) {
    if (!buf || n == 0) {
        return;
    }
    if (pump1to6 < 1) {
        pump1to6 = 1;
    }
    if (pump1to6 > PUMP_RELAY_COUNT) {
        pump1to6 = PUMP_RELAY_COUNT;
    }
    snprintf(buf, n, Strings::tr(Msg::PumpNameFmt), static_cast<int>(pump1to6));
}

#endif
