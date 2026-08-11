#ifndef FACTORY_RESET_H
#define FACTORY_RESET_H

/**
 * Borra datos de usuario en NVS (idioma, fuso, setup, WiFi, niveles/calib)
 * y reinicia. Equivalente practico a "erase flash" de datos de app:
 * el firmware sigue; el HMI vuelve al wizard de primera vez.
 */
namespace FactoryReset {
void wipeAndReboot();
}

#endif
