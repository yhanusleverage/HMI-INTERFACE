#include "RelayAliasConfig.h"
#include "AppStrings.h"
#include "Config.h"
#include <Preferences.h>
#include <cstdio>
#include <cstring>

namespace {

constexpr const char *NS = PREF_NAMESPACE;

struct Entry {
    char mac[SlaveInventory::kMacLen];
    uint8_t relay;
    char name[RELAY_ALIAS_NAME_LEN];
};

Entry entries_[RELAY_ALIAS_MAX] = {};
size_t count_ = 0;

void copyStr(char *dst, size_t n, const char *src) {
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

int findIx(const char *mac, uint8_t relay) {
    if (!mac || mac[0] == '\0') {
        return -1;
    }
    for (size_t i = 0; i < count_; ++i) {
        if (entries_[i].relay == relay && strcmp(entries_[i].mac, mac) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

const SlaveInventory::Target *findTarget(const char *mac) {
    if (!mac) {
        return nullptr;
    }
    const size_t n = SlaveInventory::count();
    for (size_t i = 0; i < n; ++i) {
        const SlaveInventory::Target *t = SlaveInventory::at(i);
        if (t && strcmp(t->mac, mac) == 0) {
            return t;
        }
    }
    return nullptr;
}

}  // namespace

void RelayAliasConfig::begin() { load(); }

void RelayAliasConfig::load() {
    count_ = 0;
    memset(entries_, 0, sizeof(entries_));
    Preferences prefs;
    if (!prefs.begin(NS, true)) {
        return;
    }
    count_ = prefs.getUChar("ra_n", 0);
    if (count_ > RELAY_ALIAS_MAX) {
        count_ = RELAY_ALIAS_MAX;
    }
    for (size_t i = 0; i < count_; ++i) {
        char key[12];
        snprintf(key, sizeof(key), "ra_%u", static_cast<unsigned>(i));
        String blob = prefs.getString(key, "");
        Entry &e = entries_[i];
        memset(&e, 0, sizeof(e));
        /* mac|relay|name */
        const char *s = blob.c_str();
        const char *p1 = strchr(s, '|');
        if (!p1) {
            continue;
        }
        size_t macLen = static_cast<size_t>(p1 - s);
        if (macLen >= sizeof(e.mac)) {
            macLen = sizeof(e.mac) - 1;
        }
        memcpy(e.mac, s, macLen);
        e.mac[macLen] = '\0';
        const char *p2 = strchr(p1 + 1, '|');
        unsigned r = 0;
        if (p2) {
            sscanf(p1 + 1, "%u", &r);
            copyStr(e.name, sizeof(e.name), p2 + 1);
        } else {
            sscanf(p1 + 1, "%u", &r);
        }
        if (r > 7) {
            r = 7;
        }
        e.relay = static_cast<uint8_t>(r);
    }
    prefs.end();
}

void RelayAliasConfig::save() {
    Preferences prefs;
    if (!prefs.begin(NS, false)) {
        return;
    }
    prefs.putUChar("ra_n", static_cast<uint8_t>(count_));
    for (size_t i = 0; i < count_; ++i) {
        char key[12];
        char blob[64];
        snprintf(key, sizeof(key), "ra_%u", static_cast<unsigned>(i));
        snprintf(blob, sizeof(blob), "%s|%u|%s", entries_[i].mac,
                 static_cast<unsigned>(entries_[i].relay), entries_[i].name);
        prefs.putString(key, blob);
    }
    prefs.end();
}

void RelayAliasConfig::placeholder(char *buf, size_t n, uint8_t relayIndex) {
    if (!buf || n == 0) {
        return;
    }
    if (relayIndex > 7) {
        relayIndex = 7;
    }
    snprintf(buf, n, "R%u", static_cast<unsigned>(relayIndex + 1));
}

const char *RelayAliasConfig::getName(const char *mac, uint8_t relayIndex) {
    const int ix = findIx(mac, relayIndex);
    if (ix < 0) {
        return "";
    }
    return entries_[static_cast<size_t>(ix)].name;
}

bool RelayAliasConfig::setName(const char *mac, uint8_t relayIndex, const char *name) {
    if (!mac || mac[0] == '\0' || relayIndex > 7) {
        return false;
    }
    char cleaned[RELAY_ALIAS_NAME_LEN];
    copyStr(cleaned, sizeof(cleaned), name ? name : "");
    /* Trim leading spaces */
    const char *p = cleaned;
    while (*p == ' ') {
        ++p;
    }
    if (p != cleaned) {
        memmove(cleaned, p, strlen(p) + 1);
    }

    int ix = findIx(mac, relayIndex);
    if (cleaned[0] == '\0') {
        if (ix < 0) {
            return true;
        }
        /* Remove entry */
        for (size_t j = static_cast<size_t>(ix) + 1; j < count_; ++j) {
            entries_[j - 1] = entries_[j];
        }
        --count_;
        save();
        return true;
    }

    if (ix < 0) {
        if (count_ >= RELAY_ALIAS_MAX) {
            return false;
        }
        ix = static_cast<int>(count_++);
        copyStr(entries_[static_cast<size_t>(ix)].mac, sizeof(entries_[0].mac), mac);
        entries_[static_cast<size_t>(ix)].relay = relayIndex;
    }
    copyStr(entries_[static_cast<size_t>(ix)].name, sizeof(entries_[0].name), cleaned);
    save();
    return true;
}

void RelayAliasConfig::displayLabel(char *buf, size_t n, const char *mac, uint8_t relayIndex) {
    if (!buf || n == 0) {
        return;
    }
    char ph[8];
    placeholder(ph, sizeof(ph), relayIndex);
    const char *alias = getName(mac, relayIndex);
    const SlaveInventory::Target *t = findTarget(mac);
    const char *dev = nullptr;
    char atlasBuf[SlaveInventory::kNameLen];
    if (SlaveInventory::isEspNow(t)) {
        if (t->name[0] && strcmp(t->name, "Master") != 0 && strncmp(t->name, "Slave", 5) != 0) {
            dev = t->name;
        } else {
            snprintf(atlasBuf, sizeof(atlasBuf), "%s", Strings::tr(Msg::DeviceAtlas));
            dev = atlasBuf;
        }
    }

    if (alias && alias[0] != '\0') {
        if (dev) {
            snprintf(buf, n, "%s · %s", dev, alias);
        } else if (mac && strcmp(mac, kPlaceholderMac) == 0) {
            snprintf(buf, n, "%s · %s", Strings::tr(Msg::DeviceAtlas), alias);
        } else {
            snprintf(buf, n, "%s", alias);
        }
        return;
    }
    if (dev) {
        snprintf(buf, n, "%s · %s", dev, ph);
    } else if (mac && strcmp(mac, kPlaceholderMac) == 0) {
        snprintf(buf, n, "%s · %s", Strings::tr(Msg::DeviceAtlas), ph);
    } else {
        snprintf(buf, n, "%s", ph);
    }
}

void RelayAliasConfig::migratePlaceholderTo(const char *realMac) {
    if (!realMac || realMac[0] == '\0' || strcmp(realMac, kPlaceholderMac) == 0) {
        return;
    }
    bool changed = false;
    for (uint8_t r = 0; r < 8; ++r) {
        const int phIx = findIx(kPlaceholderMac, r);
        if (phIx < 0) {
            continue;
        }
        char name[RELAY_ALIAS_NAME_LEN];
        copyStr(name, sizeof(name), entries_[static_cast<size_t>(phIx)].name);

        for (size_t j = static_cast<size_t>(phIx) + 1; j < count_; ++j) {
            entries_[j - 1] = entries_[j];
        }
        --count_;
        changed = true;

        if (name[0] == '\0') {
            continue;
        }
        if (findIx(realMac, r) >= 0) {
            continue; /* destino ya tiene nombre */
        }
        if (count_ >= RELAY_ALIAS_MAX) {
            continue;
        }
        Entry &e = entries_[count_++];
        memset(&e, 0, sizeof(e));
        copyStr(e.mac, sizeof(e.mac), realMac);
        e.relay = r;
        copyStr(e.name, sizeof(e.name), name);
    }
    if (changed) {
        save();
    }
}
