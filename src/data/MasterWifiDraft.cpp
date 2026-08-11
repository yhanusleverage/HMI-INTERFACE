#include "MasterWifiDraft.h"
#include "MasterLink.h"
#include "WifiConfig.h"

#include <cstring>

namespace MasterWifiDraft {

static char ssid_[WifiConfig::SSID_MAX] = {};
static char pass_[WifiConfig::PASS_MAX] = {};
static char email_[kEmailMax] = {};
static char name_[kNameMax] = {};
static char loc_[kLocMax] = {};
static bool hasNetwork_ = false;

static void copyStr(char *dst, size_t n, const char *src) {
    if (!dst || n == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
}

void clear() {
    ssid_[0] = '\0';
    pass_[0] = '\0';
    email_[0] = '\0';
    name_[0] = '\0';
    loc_[0] = '\0';
    hasNetwork_ = false;
}

void clearNetwork() {
    ssid_[0] = '\0';
    pass_[0] = '\0';
    hasNetwork_ = false;
}

void setNetwork(const char *ssid, const char *pass) {
    copyStr(ssid_, sizeof(ssid_), ssid);
    copyStr(pass_, sizeof(pass_), pass);
    hasNetwork_ = ssid_[0] != '\0';
}

bool hasNetwork() { return hasNetwork_; }

const char *ssid() { return ssid_; }

const char *pass() { return pass_; }

void setProfile(const char *email, const char *deviceName, const char *location) {
    copyStr(email_, sizeof(email_), email);
    copyStr(name_, sizeof(name_), deviceName);
    copyStr(loc_, sizeof(loc_), location);
}

const char *email() { return email_; }

const char *deviceName() { return name_; }

const char *location() { return loc_; }

bool commitProvision() {
    if (!hasNetwork_) {
        return false;
    }
    if (!WifiConfig::save(ssid_, pass_)) {
        return false;
    }
    MasterLink::clearWifiConfigAck();
    MasterLink::sendWifiConfig(ssid_, pass_, name_[0] ? name_ : nullptr, email_[0] ? email_ : nullptr,
                               loc_[0] ? loc_ : nullptr);
    return true;
}

}  // namespace MasterWifiDraft
