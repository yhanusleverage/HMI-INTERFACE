#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "BoardPins.h"

#define SYSTEM_NAME "HIDRO HMI"
#define FIRMWARE_VERSION "1.0.0-hmi"

#ifndef DATA_SOURCE_SIM
#define DATA_SOURCE_SIM 1
#endif

#define UI_REFRESH_MS 200UL
#define SIM_TICK_MS 500UL
#define UART_LINK_TIMEOUT_MS 5000UL

#define PREF_NAMESPACE "hidro_hmi"

#endif
