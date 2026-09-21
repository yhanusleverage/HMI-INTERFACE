#ifndef FACTORY_RESET_H
#define FACTORY_RESET_H

/**
 * Borra datos de usuario en NVS del HMI (idioma, fuso, setup, WiFi…)
 * y reinicia el display. Opcionalmente manda factory_reset al Master
 * (limpia hydro_system — no erase flash).
 */
namespace FactoryReset {
void wipeAndReboot(bool alsoResetMaster = true);
}

#endif
