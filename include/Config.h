#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "BoardPins.h"

#define SYSTEM_NAME "HIDRO HMI"
#define FIRMWARE_VERSION "1.0.0-hmi"

#ifndef UART_BENCH
#define UART_BENCH 0
#endif

#ifndef DATA_SOURCE_SIM
#define DATA_SOURCE_SIM 1
#endif

#define UI_REFRESH_MS 200UL
#define SIM_TICK_MS 500UL
#define UART_LINK_TIMEOUT_MS 5000UL

#ifndef UART_LINK_DEBUG
#define UART_LINK_DEBUG 1
#endif
#ifndef UART_LINK_DEBUG_INTERVAL_MS
#define UART_LINK_DEBUG_INTERVAL_MS 5000UL
#endif

#define PREF_NAMESPACE "hidro_hmi"

#endif
