#ifndef MASTER_WIFI_DRAFT_H
#define MASTER_WIFI_DRAFT_H

#include <stddef.h>

/** Estado temporal wizard 4/5 → 5/5 (red + perfil cloud). */
namespace MasterWifiDraft {

constexpr size_t kEmailMax = 48;
constexpr size_t kNameMax = 32;
constexpr size_t kLocMax = 32;

void clear();
void clearNetwork();

void setNetwork(const char *ssid, const char *pass);
bool hasNetwork();
const char *ssid();
const char *pass();

void setProfile(const char *email, const char *deviceName, const char *location);
const char *email();
const char *deviceName();
const char *location();

/**
 * Precarga desde Master sys_info (solo rellena campos vacíos del draft).
 * Si hay SSID+pass y HMI aún sin WifiConfig, guarda y conecta STA HMI (NTP).
 */
void applyFromMaster(const char *ssid, const char *pass, const char *email,
                     const char *deviceName, const char *location);

/** WifiConfig::save + MasterLink::sendWifiConfig. false si no hay red o falla NVS. */
bool commitProvision();

}  // namespace MasterWifiDraft

#endif
