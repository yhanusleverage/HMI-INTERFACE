#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <stddef.h>
#include <stdint.h>

/**
 * Credenciales WiFi STA + scan + conexión.
 * Sin softAP.
 */
namespace WifiConfig {

constexpr size_t SSID_MAX = 33;
constexpr size_t PASS_MAX = 64;
constexpr int SCAN_MAX = 16;

struct Network {
    char ssid[SSID_MAX];
    int32_t rssi;
    bool open;
};

bool begin();
bool configured();
void getSsid(char *out, size_t len);
void getPass(char *out, size_t len);
bool save(const char *ssid, const char *pass);
bool clear();

bool startScan();
int pollScan(Network *out, int maxN);

/** Arranca STA con credenciales NVS (o tras save). */
bool connectSaved();
void loop();  // poll estado; dispara NTP al conectar
bool isConnected();
/** 0=idle 1=connecting 2=connected 3=fail */
uint8_t linkState();

}  // namespace WifiConfig

#endif
