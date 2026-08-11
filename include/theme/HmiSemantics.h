#ifndef HMI_SEMANTICS_H
#define HMI_SEMANTICS_H

#include "DataStore.h"
#include "AppStrings.h"
#include "theme/AppTheme.h"

/**
 * Semántica High Performance HMI:
 * - OK / normal → neutro (no verde dominante)
 * - Warn / Alarm → color solo en badge y borde
 * - SIM / LIVE / UART → canal distinto al de proceso
 */
namespace HmiSemantics {

inline const char *statusText(ParamStatus st) {
    switch (st) {
    case ParamStatus::Ok:
        return Strings::tr(Msg::StatusOk);
    case ParamStatus::Low:
        return Strings::tr(Msg::StatusLow);
    case ParamStatus::High:
        return Strings::tr(Msg::StatusHigh);
    default:
        return "--";
    }
}

/** Color del texto de estado (badge). OK = muted; alarma = rojo. */
inline lv_color_t statusLabel(ParamStatus st) {
    switch (st) {
    case ParamStatus::Ok:
        return AppTheme::muted();
    case ParamStatus::Low:
    case ParamStatus::High:
        return AppTheme::alarm();
    default:
        return AppTheme::muted();
    }
}

/** Borde de celda. OK = cyan Nuravine; alarma = rojo. */
inline lv_color_t statusBorder(ParamStatus st) {
    switch (st) {
    case ParamStatus::Ok:
        return AppTheme::gridLine();
    case ParamStatus::Low:
    case ParamStatus::High:
        return AppTheme::alarm();
    default:
        return AppTheme::muted();
    }
}

inline lv_coord_t statusBorderWidth(ParamStatus st) {
    switch (st) {
    case ParamStatus::Low:
    case ParamStatus::High:
        return AppTheme::BORDER_ALERT;
    default:
        return AppTheme::BORDER_IDLE;
    }
}

inline lv_color_t sourceBadge(DataSource src) {
    return src == DataSource::Sim ? AppTheme::simBadge() : AppTheme::liveBadge();
}

inline const char *sourceText(DataSource src) {
    return src == DataSource::Sim ? "SIM" : "LIVE";
}

/** Enlace UART: OK neutro; fallo = warn (no mezclar con alarma de sensor). */
inline lv_color_t linkLabel(bool ok) {
    return ok ? AppTheme::muted() : AppTheme::warn();
}

}  // namespace HmiSemantics

#endif
