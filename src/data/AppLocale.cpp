#include "AppLocale.h"
#include "Config.h"
#include <Preferences.h>
#include <WiFi.h>
#include <sys/time.h>
#include <ctime>
#include <cstring>

namespace {

AppLang lang_ = AppLang::Es;
int16_t tzMin_ = -180;  // Brasilia / Sao Paulo
bool setupDone_ = false;

constexpr const char *NS = PREF_NAMESPACE;

struct TzPlace {
    int16_t min;
    const char *es;
    const char *en;
    const char *pt;
};

/**
 * LatAm-first + pocas globales. Offset fijo (sin DST automatico).
 * ASCII-safe para Montserrat.
 */
constexpr TzPlace kPlaces[] = {
    {-600, "Honolulu (Hawaii)", "Honolulu (Hawaii)", "Honolulu (Hawaii)"},
    {-480, "Los Angeles / Vancouver", "Los Angeles / Vancouver", "Los Angeles / Vancouver"},
    {-420, "Denver / Phoenix", "Denver / Phoenix", "Denver / Phoenix"},
    {-360, "Ciudad de Mexico / Chicago", "Mexico City / Chicago", "Cidade do Mexico / Chicago"},
    {-300, "New York / Bogota / Lima", "New York / Bogota / Lima", "Nova York / Bogota / Lima"},
    {-300, "Rio Branco / Acre", "Rio Branco / Acre", "Rio Branco / Acre"},
    {-240, "Manaus / Amazonas", "Manaus / Amazonas", "Manaus / Amazonas"},
    {-240, "Caracas / La Paz", "Caracas / La Paz", "Caracas / La Paz"},
    {-240, "Santiago / Asuncion", "Santiago / Asuncion", "Santiago / Assuncao"},
    {-180, "Brasilia / Sao Paulo", "Brasilia / Sao Paulo", "Brasilia / Sao Paulo"},
    {-180, "Buenos Aires / Montevideo", "Buenos Aires / Montevideo", "Buenos Aires / Montevideo"},
    {-120, "Fernando de Noronha", "Fernando de Noronha", "Fernando de Noronha"},
    {0, "Lisboa / Londres", "Lisbon / London", "Lisboa / Londres"},
    {60, "Madrid / Paris / Berlin", "Madrid / Paris / Berlin", "Madri / Paris / Berlim"},
    {120, "Atenas / Cairo", "Athens / Cairo", "Atenas / Cairo"},
    {180, "Moscou / Istambul", "Moscow / Istanbul", "Moscou / Istambul"},
    {330, "Mumbai / Nova Delhi", "Mumbai / New Delhi", "Mumbai / Nova Delhi"},
    {480, "Pekin / Singapur", "Beijing / Singapore", "Pequim / Singapura"},
    {540, "Toquio / Seul", "Tokyo / Seoul", "Toquio / Seul"},
    {600, "Sydney / Melbourne", "Sydney / Melbourne", "Sydney / Melbourne"},
};

constexpr int kPlaceCount = static_cast<int>(sizeof(kPlaces) / sizeof(kPlaces[0]));
constexpr int kDefaultIx = 9;  // Brasilia / Sao Paulo

bool readNow(struct tm *out) {
    time_t now = time(nullptr);
    if (now < 1700000000) {
        return false;
    }
    localtime_r(&now, out);
    return true;
}

const char *labelFor(const TzPlace &p) {
    switch (lang_) {
    case AppLang::En:
        return p.en;
    case AppLang::Pt:
        return p.pt;
    case AppLang::Es:
    default:
        return p.es;
    }
}

}  // namespace

void AppLocale::begin() {
    load();
    applyTzToSystem();
}

void AppLocale::load() {
    Preferences prefs;
    if (!prefs.begin(NS, true)) {
        lang_ = AppLang::Es;
        tzMin_ = -180;
        setupDone_ = false;
        return;
    }
    const uint8_t v = prefs.getUChar("lang", static_cast<uint8_t>(AppLang::Es));
    if (v == static_cast<uint8_t>(AppLang::En)) {
        lang_ = AppLang::En;
    } else if (v == static_cast<uint8_t>(AppLang::Pt)) {
        lang_ = AppLang::Pt;
    } else {
        lang_ = AppLang::Es;
    }
    tzMin_ = static_cast<int16_t>(prefs.getInt("tz_min", -180));
    setupDone_ = prefs.getBool("setup_done", false);
    prefs.end();
}

void AppLocale::save() {
    Preferences prefs;
    if (!prefs.begin(NS, false)) {
        return;
    }
    prefs.putUChar("lang", static_cast<uint8_t>(lang_));
    prefs.putInt("tz_min", static_cast<int32_t>(tzMin_));
    prefs.putBool("setup_done", setupDone_);
    prefs.end();
}

AppLang AppLocale::language() { return lang_; }

void AppLocale::setLanguage(AppLang lang) {
    lang_ = lang;
    save();
}

const char *AppLocale::languageName(AppLang lang) {
    switch (lang) {
    case AppLang::En:
        return "English";
    case AppLang::Pt:
        return "Portugues";
    case AppLang::Es:
    default:
        return "Espanol";
    }
}

int16_t AppLocale::tzOffsetMin() { return tzMin_; }

void AppLocale::setTzOffsetMin(int16_t min) {
    tzMin_ = min;
    save();
    applyTzToSystem();
}

void AppLocale::applyTzToSystem() {
    const long gmt = static_cast<long>(tzMin_) * 60L;
    configTime(gmt, 0, "pool.ntp.org", "time.google.com");
}

void AppLocale::syncNtpIfOnline() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }
    applyTzToSystem();
}

bool AppLocale::setupDone() { return setupDone_; }

void AppLocale::setSetupDone(bool done) {
    setupDone_ = done;
    save();
}

int AppLocale::tzPresetCount() { return kPlaceCount; }

int16_t AppLocale::tzPresetMin(int index) {
    if (index < 0 || index >= kPlaceCount) {
        return kPlaces[kDefaultIx].min;
    }
    return kPlaces[index].min;
}

const char *AppLocale::tzPresetLabel(int index) {
    if (index < 0 || index >= kPlaceCount) {
        return labelFor(kPlaces[kDefaultIx]);
    }
    return labelFor(kPlaces[index]);
}

int AppLocale::tzPresetIndexForMin(int16_t min) {
    for (int i = 0; i < kPlaceCount; ++i) {
        if (kPlaces[i].min == min) {
            return i;
        }
    }
    return -1;
}

void AppLocale::formatTzLabel(char *buf, size_t len, int16_t min) {
    if (!buf || len == 0) {
        return;
    }
    const int ix = tzPresetIndexForMin(min);
    if (ix < 0) {
        const int h = min / 60;
        const int m = abs(min % 60);
        if (m == 0) {
            snprintf(buf, len, "UTC%+d", h);
        } else {
            snprintf(buf, len, "UTC%+d:%02d", h, m);
        }
        return;
    }
    strncpy(buf, tzPresetLabel(ix), len - 1);
    buf[len - 1] = '\0';
}

void AppLocale::formatNow(char *buf, size_t len) {
    struct tm t {};
    if (!readNow(&t)) {
        snprintf(buf, len, "----/--/-- --:--");
        return;
    }
    snprintf(buf, len, "%04d-%02d-%02d %02d:%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min);
}

void AppLocale::formatNowClock(char *buf, size_t len) {
    struct tm t {};
    if (!readNow(&t)) {
        snprintf(buf, len, "--:--");
        return;
    }
    snprintf(buf, len, "%02d:%02d", t.tm_hour, t.tm_min);
}

void AppLocale::formatNowDate(char *buf, size_t len) {
    struct tm t {};
    if (!readNow(&t)) {
        snprintf(buf, len, "----/--/--");
        return;
    }
    snprintf(buf, len, "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
}
