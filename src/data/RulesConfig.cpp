#include "RulesConfig.h"
#include <Preferences.h>
#include <cstdio>
#include <cstring>

namespace RulesConfig {
namespace {

Preferences prefs;
constexpr const char *kNs = "hmirules";
Rule rules_[kMaxRules]{};

void clearMac(char *mac) {
    mac[0] = '\0';
}

void resetClauseSensor(CondClause &c) {
    c.kind = CondKind::Sensor;
    c.sensor = Sensor::Ph;
    c.op = Op::Gt;
    c.threshold = 7.0f;
    c.relayLocal = true;
    clearMac(c.relayMac);
    c.relayIndex = 0;
    c.relayWantOn = true;
}

void resetClauseRelay(CondClause &c) {
    c.kind = CondKind::RelayState;
    c.sensor = Sensor::Ph;
    c.op = Op::Gt;
    c.threshold = 0.0f;
    c.relayLocal = false;
    clearMac(c.relayMac);
    c.relayIndex = 0;
    c.relayWantOn = true;
}

void clearRule(Rule &r) {
    r = {};
    r.used = false;
    r.enabled = false;
    r.name[0] = '\0';
    r.priority = Priority::Normal;
    r.condCount = 1;
    r.logic = Logic::And;
    resetClauseSensor(r.cond[0]);
    for (uint8_t i = 1; i < kMaxConds; ++i) {
        resetClauseSensor(r.cond[i]);
    }
    r.localTarget = false;
    clearMac(r.mac);
    r.relay = 0;
    r.action = Action::On;
    r.durationSec = 0;
    r.cooldownSec = 60;
    r.lastFireMs = 0;
}

void sanitizeName(char *dst, size_t n, const char *src) {
    if (!dst || n == 0) {
        return;
    }
    dst[0] = '\0';
    if (!src) {
        return;
    }
    while (*src == ' ' || *src == '\t') {
        ++src;
    }
    size_t j = 0;
    for (; src[j] && j + 1 < n; ++j) {
        char c = src[j];
        if (c == '|' || c < 32) {
            c = ' ';
        }
        dst[j] = c;
    }
    dst[j] = '\0';
    while (j > 0 && (dst[j - 1] == ' ' || dst[j - 1] == '\t')) {
        dst[--j] = '\0';
    }
}

void loadNameKey(size_t i) {
    char key[8];
    snprintf(key, sizeof(key), "rn%u", static_cast<unsigned>(i));
    String nm = prefs.getString(key, "");
    sanitizeName(rules_[i].name, kNameLen, nm.c_str());
}

void saveNameKey(size_t i) {
    char key[8];
    snprintf(key, sizeof(key), "rn%u", static_cast<unsigned>(i));
    if (!rules_[i].used || !rules_[i].name[0]) {
        prefs.remove(key);
    } else {
        prefs.putString(key, rules_[i].name);
    }
}

const char *sensorTag(Sensor s) {
    switch (s) {
    case Sensor::Ec:
        return "EC";
    case Sensor::Temp:
        return "T";
    case Sensor::Orp:
        return "ORP";
    case Sensor::Do:
        return "DO";
    case Sensor::L1:
        return "L1";
    case Sensor::L2:
        return "L2";
    case Sensor::L3:
        return "L3";
    case Sensor::L4:
        return "L4";
    case Sensor::Ph:
    default:
        return "pH";
    }
}

const char *opTag(Op op) {
    switch (op) {
    case Op::Gte:
        return ">=";
    case Op::Lt:
        return "<";
    case Op::Lte:
        return "<=";
    case Op::Eq:
        return "=";
    case Op::Gt:
    default:
        return ">";
    }
}

void formatClause(const CondClause &c, char *buf, size_t n) {
    if (c.kind == CondKind::RelayState) {
        char who[24];
        if (c.relayLocal) {
            snprintf(who, sizeof(who), "Master");
        } else if (c.relayMac[0]) {
            snprintf(who, sizeof(who), "Atlas");
        } else {
            snprintf(who, sizeof(who), "?");
        }
        snprintf(buf, n, "R%u %s %s", static_cast<unsigned>(c.relayIndex + 1), who,
                 c.relayWantOn ? "ON" : "OFF");
        return;
    }
    snprintf(buf, n, "%s%s%.1f", sensorTag(c.sensor), opTag(c.op),
             static_cast<double>(c.threshold));
}

/** Formato v2: v2|used|en|prio|logic|nc|local|mac|relay|act|dur|cd|c0|c1|c2
 *  cláusula: K:S:O:T:RL:RM:RI:WO
 *  Legacy v1 (sin v2): used|en|sens|op|thr|local|mac|relay|act|dur|cd [|prio] */
void macToStorage(const char *mac, char *out, size_t n) {
    if (!out || n == 0) {
        return;
    }
    if (!mac || !mac[0]) {
        strncpy(out, "-", n);
        out[n - 1] = '\0';
        return;
    }
    size_t j = 0;
    for (size_t i = 0; mac[i] && j + 1 < n; ++i) {
        out[j++] = (mac[i] == ':') ? '-' : mac[i];
    }
    out[j] = '\0';
}

void macFromStorage(const char *in, char *out, size_t n) {
    if (!out || n == 0) {
        return;
    }
    if (!in || !in[0] || strcmp(in, "-") == 0) {
        out[0] = '\0';
        return;
    }
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 1 < n; ++i) {
        out[j++] = (in[i] == '-') ? ':' : in[i];
    }
    out[j] = '\0';
}

bool parseClause(const char *tok, CondClause &c) {
    resetClauseSensor(c);
    if (!tok || !tok[0]) {
        return false;
    }
    int k = 0, s = 0, o = 0, rl = 1, ri = 0, wo = 1;
    float thr = 7.0f;
    char rmacStor[SlaveInventory::kMacLen] = {};
    if (sscanf(tok, "%d:%d:%d:%f:%d:%17[^:]:%d:%d", &k, &s, &o, &thr, &rl, rmacStor, &ri, &wo) < 4) {
        return false;
    }
    c.kind = (k == 1) ? CondKind::RelayState : CondKind::Sensor;
    if (s < 0 || s >= static_cast<int>(Sensor::Count)) {
        s = 0;
    }
    c.sensor = static_cast<Sensor>(s);
    if (o < 0 || o >= static_cast<int>(Op::Count)) {
        o = 0;
    }
    c.op = static_cast<Op>(o);
    c.threshold = thr;
    c.relayLocal = (rl != 0);
    macFromStorage(rmacStor, c.relayMac, SlaveInventory::kMacLen);
    if (ri < 0) {
        ri = 0;
    }
    if (ri > 7) {
        ri = 7;
    }
    c.relayIndex = static_cast<uint8_t>(ri);
    c.relayWantOn = (wo != 0);
    return true;
}

void appendClause(char *out, size_t n, const CondClause &c) {
    char piece[96];
    char macStor[SlaveInventory::kMacLen];
    macToStorage(c.relayMac, macStor, sizeof(macStor));
    snprintf(piece, sizeof(piece), "%u:%u:%u:%.3f:%u:%s:%u:%u",
             static_cast<unsigned>(c.kind), static_cast<unsigned>(c.sensor),
             static_cast<unsigned>(c.op), static_cast<double>(c.threshold),
             c.relayLocal ? 1u : 0u, macStor, static_cast<unsigned>(c.relayIndex),
             c.relayWantOn ? 1u : 0u);
    strncat(out, piece, n - strlen(out) - 1);
}

bool parseLegacyV1(const char *raw, Rule &r) {
    clearRule(r);
    int used = 0, en = 0, sens = 0, op = 0, local = 1, relay = 0, act = 0;
    int dur = 0;
    unsigned long cd = 60;
    float thr = 7.0f;
    char mac[SlaveInventory::kMacLen] = {};
    int n = sscanf(raw, "%d|%d|%d|%d|%f|%d|%17[^|]|%d|%d|%d|%lu", &used, &en, &sens, &op, &thr,
                   &local, mac, &relay, &act, &dur, &cd);
    if (n < 11) {
        return false;
    }
    int prio = static_cast<int>(Priority::Normal);
    const char *p = raw;
    for (int i = 0; i < 11 && p; ++i) {
        p = strchr(p, '|');
        if (p) {
            ++p;
        }
    }
    if (p && *p) {
        prio = atoi(p);
    }
    r.used = (used != 0);
    r.enabled = (en != 0);
    r.priority = static_cast<Priority>(
        (prio >= 0 && prio < static_cast<int>(Priority::Count)) ? prio
                                                               : static_cast<int>(Priority::Normal));
    r.condCount = 1;
    r.logic = Logic::And;
    resetClauseSensor(r.cond[0]);
    if (sens < 0 || sens >= static_cast<int>(Sensor::Count)) {
        sens = 0;
    }
    r.cond[0].sensor = static_cast<Sensor>(sens);
    if (op < 0 || op >= static_cast<int>(Op::Count)) {
        op = 0;
    }
    r.cond[0].op = static_cast<Op>(op);
    r.cond[0].threshold = thr;
    r.localTarget = (local != 0);
    strncpy(r.mac, mac, SlaveInventory::kMacLen - 1);
    r.mac[SlaveInventory::kMacLen - 1] = '\0';
    if (relay < 0) {
        relay = 0;
    }
    if (relay > 7) {
        relay = 7;
    }
    r.relay = static_cast<uint8_t>(relay);
    r.action = (act != 0) ? Action::Off : Action::On;
    if (dur < 0) {
        dur = 0;
    }
    if (dur > 3600) {
        dur = 3600;
    }
    r.durationSec = static_cast<uint16_t>(dur);
    r.cooldownSec = cd;
    if (r.cooldownSec > 86400u) {
        r.cooldownSec = 86400u;
    }
    return true;
}

bool parseV2(const char *raw, Rule &r) {
    clearRule(r);
    /* v2|used|en|prio|logic|nc|local|mac|relay|act|dur|cd|c0|c1|c2 */
    const char *p = raw;
    if (strncmp(p, "v2|", 3) != 0) {
        return false;
    }
    p += 3;
    auto next = [&](char *dest, size_t dn) -> bool {
        if (!p) {
            return false;
        }
        const char *bar = strchr(p, '|');
        size_t len = bar ? static_cast<size_t>(bar - p) : strlen(p);
        if (len >= dn) {
            len = dn - 1;
        }
        memcpy(dest, p, len);
        dest[len] = '\0';
        p = bar ? bar + 1 : nullptr;
        return true;
    };
    char tok[96];
    int used = 0, en = 0, prio = 2, logic = 0, nc = 1, local = 1, relay = 0, act = 0, dur = 0;
    unsigned long cd = 60;
    if (!next(tok, sizeof(tok))) {
        return false;
    }
    used = atoi(tok);
    if (!next(tok, sizeof(tok))) {
        return false;
    }
    en = atoi(tok);
    if (!next(tok, sizeof(tok))) {
        return false;
    }
    prio = atoi(tok);
    if (!next(tok, sizeof(tok))) {
        return false;
    }
    logic = atoi(tok);
    if (!next(tok, sizeof(tok))) {
        return false;
    }
    nc = atoi(tok);
    if (!next(tok, sizeof(tok))) {
        return false;
    }
    local = atoi(tok);
    if (!next(tok, sizeof(tok))) {
        return false;
    }
    strncpy(r.mac, tok, SlaveInventory::kMacLen - 1);
    r.mac[SlaveInventory::kMacLen - 1] = '\0';
    if (!next(tok, sizeof(tok))) {
        return false;
    }
    relay = atoi(tok);
    if (!next(tok, sizeof(tok))) {
        return false;
    }
    act = atoi(tok);
    if (!next(tok, sizeof(tok))) {
        return false;
    }
    dur = atoi(tok);
    if (!next(tok, sizeof(tok))) {
        return false;
    }
    cd = strtoul(tok, nullptr, 10);

    r.used = (used != 0);
    r.enabled = (en != 0);
    if (prio < 0 || prio >= static_cast<int>(Priority::Count)) {
        prio = static_cast<int>(Priority::Normal);
    }
    r.priority = static_cast<Priority>(prio);
    r.logic = (logic == 1) ? Logic::Or : Logic::And;
    if (nc < 1) {
        nc = 1;
    }
    if (nc > static_cast<int>(kMaxConds)) {
        nc = kMaxConds;
    }
    r.condCount = static_cast<uint8_t>(nc);
    r.localTarget = (local != 0);
    if (relay < 0) {
        relay = 0;
    }
    if (relay > 7) {
        relay = 7;
    }
    r.relay = static_cast<uint8_t>(relay);
    r.action = (act != 0) ? Action::Off : Action::On;
    if (dur < 0) {
        dur = 0;
    }
    if (dur > 3600) {
        dur = 3600;
    }
    r.durationSec = static_cast<uint16_t>(dur);
    r.cooldownSec = cd;
    if (r.cooldownSec > 86400u) {
        r.cooldownSec = 86400u;
    }

    for (uint8_t i = 0; i < kMaxConds; ++i) {
        if (i < r.condCount && next(tok, sizeof(tok))) {
            if (!parseClause(tok, r.cond[i])) {
                resetClauseSensor(r.cond[i]);
            }
        } else {
            resetClauseSensor(r.cond[i]);
        }
    }
    return true;
}

}  // namespace

void initClauseSensor(CondClause &c) {
    resetClauseSensor(c);
}

void initClauseRelay(CondClause &c) {
    resetClauseRelay(c);
}

void begin() {
    for (size_t i = 0; i < kMaxRules; ++i) {
        clearRule(rules_[i]);
    }
    prefs.begin(kNs, false);
    load();
}

void load() {
    for (size_t i = 0; i < kMaxRules; ++i) {
        clearRule(rules_[i]);
        char key[8];
        snprintf(key, sizeof(key), "r%u", static_cast<unsigned>(i));
        String raw = prefs.getString(key, "");
        if (raw.length() == 0) {
            char nk[8];
            snprintf(nk, sizeof(nk), "rn%u", static_cast<unsigned>(i));
            prefs.remove(nk);
            continue;
        }
        Rule tmp{};
        if (raw.startsWith("v2|")) {
            if (!parseV2(raw.c_str(), tmp) || !tmp.used) {
                continue;
            }
        } else {
            if (!parseLegacyV1(raw.c_str(), tmp) || !tmp.used) {
                continue;
            }
        }
        rules_[i] = tmp;
        rules_[i].lastFireMs = 0;
        loadNameKey(i);
    }
}

void save() {
    for (size_t i = 0; i < kMaxRules; ++i) {
        char key[8];
        snprintf(key, sizeof(key), "r%u", static_cast<unsigned>(i));
        const Rule &r = rules_[i];
        if (!r.used) {
            prefs.remove(key);
            saveNameKey(i);
            continue;
        }
        char buf[480];
        snprintf(buf, sizeof(buf),
                 "v2|%d|%d|%u|%u|%u|%d|%s|%u|%d|%u|%lu", r.used ? 1 : 0, r.enabled ? 1 : 0,
                 static_cast<unsigned>(r.priority), static_cast<unsigned>(r.logic),
                 static_cast<unsigned>(r.condCount), r.localTarget ? 1 : 0, r.mac[0] ? r.mac : "",
                 static_cast<unsigned>(r.relay), (r.action == Action::Off) ? 1 : 0,
                 static_cast<unsigned>(r.durationSec), static_cast<unsigned long>(r.cooldownSec));
        for (uint8_t c = 0; c < r.condCount && c < kMaxConds; ++c) {
            strncat(buf, "|", sizeof(buf) - strlen(buf) - 1);
            appendClause(buf, sizeof(buf), r.cond[c]);
        }
        prefs.putString(key, buf);
        saveNameKey(i);
    }
}

size_t count() {
    size_t n = 0;
    for (size_t i = 0; i < kMaxRules; ++i) {
        if (rules_[i].used) {
            ++n;
        }
    }
    return n;
}

const Rule *at(size_t i) {
    if (i >= kMaxRules || !rules_[i].used) {
        return nullptr;
    }
    return &rules_[i];
}

Rule *mutableAt(size_t i) {
    if (i >= kMaxRules || !rules_[i].used) {
        return nullptr;
    }
    return &rules_[i];
}

size_t addDefault() {
    for (size_t i = 0; i < kMaxRules; ++i) {
        if (rules_[i].used) {
            continue;
        }
        clearRule(rules_[i]);
        rules_[i].used = true;
        rules_[i].enabled = false;
        const size_t aix = SlaveInventory::firstEspNowIndex();
        if (aix != SIZE_MAX) {
            const SlaveInventory::Target *t = SlaveInventory::at(aix);
            if (t) {
                rules_[i].localTarget = false;
                strncpy(rules_[i].mac, t->mac, SlaveInventory::kMacLen - 1);
                rules_[i].mac[SlaveInventory::kMacLen - 1] = '\0';
            }
        }
        save();
        return i;
    }
    return SIZE_MAX;
}

bool removeAt(size_t i) {
    if (i >= kMaxRules || !rules_[i].used) {
        return false;
    }
    clearRule(rules_[i]);
    save();
    return true;
}

void setEnabled(size_t i, bool on) {
    Rule *r = mutableAt(i);
    if (!r) {
        return;
    }
    r->enabled = on;
    save();
}

bool setName(size_t i, const char *name) {
    Rule *r = mutableAt(i);
    if (!r) {
        return false;
    }
    sanitizeName(r->name, kNameLen, name);
    save();
    return true;
}

void cycleTarget(size_t i) {
    Rule *r = mutableAt(i);
    if (!r) {
        return;
    }
    /* Solo ciclar slaves ESP-NOW (Atlas); no Master local. */
    const size_t n = SlaveInventory::count();
    size_t first = SlaveInventory::firstEspNowIndex();
    if (first == SIZE_MAX) {
        r->localTarget = false;
        clearMac(r->mac);
        save();
        return;
    }

    size_t idx = SIZE_MAX;
    if (!r->localTarget && r->mac[0]) {
        for (size_t s = 0; s < n; ++s) {
            const SlaveInventory::Target *t = SlaveInventory::at(s);
            if (t && SlaveInventory::isEspNow(t) &&
                strncmp(t->mac, r->mac, SlaveInventory::kMacLen) == 0) {
                idx = s;
                break;
            }
        }
    }

    size_t next = (idx == SIZE_MAX) ? first : (idx + 1);
    while (next < n && !SlaveInventory::isEspNow(SlaveInventory::at(next))) {
        ++next;
    }
    if (next >= n) {
        next = first;
    }
    const SlaveInventory::Target *t = SlaveInventory::at(next);
    r->localTarget = false;
    if (t) {
        strncpy(r->mac, t->mac, SlaveInventory::kMacLen - 1);
        r->mac[SlaveInventory::kMacLen - 1] = '\0';
    }
    save();
}

void cycleSensor(size_t i) {
    Rule *r = mutableAt(i);
    if (!r || r->condCount == 0) {
        return;
    }
    CondClause &c = r->cond[0];
    if (c.kind != CondKind::Sensor) {
        return;
    }
    uint8_t v = static_cast<uint8_t>(c.sensor) + 1;
    if (v >= static_cast<uint8_t>(Sensor::Count)) {
        v = 0;
    }
    c.sensor = static_cast<Sensor>(v);
    save();
}

void cycleOp(size_t i) {
    Rule *r = mutableAt(i);
    if (!r || r->condCount == 0) {
        return;
    }
    CondClause &c = r->cond[0];
    if (c.kind != CondKind::Sensor) {
        return;
    }
    uint8_t v = static_cast<uint8_t>(c.op) + 1;
    if (v >= static_cast<uint8_t>(Op::Count)) {
        v = 0;
    }
    c.op = static_cast<Op>(v);
    save();
}

void cycleRelay(size_t i) {
    Rule *r = mutableAt(i);
    if (!r) {
        return;
    }
    r->relay = static_cast<uint8_t>((r->relay + 1) % 8);
    save();
}

void cyclePriority(size_t i) {
    Rule *r = mutableAt(i);
    if (!r) {
        return;
    }
    uint8_t v = static_cast<uint8_t>(r->priority) + 1;
    if (v >= static_cast<uint8_t>(Priority::Count)) {
        v = 0;
    }
    r->priority = static_cast<Priority>(v);
    save();
}

void adjustThreshold(size_t i, float delta) {
    Rule *r = mutableAt(i);
    if (!r || r->condCount == 0) {
        return;
    }
    CondClause &c = r->cond[0];
    if (c.kind != CondKind::Sensor) {
        return;
    }
    c.threshold += delta;
    if (c.threshold < -1000.0f) {
        c.threshold = -1000.0f;
    }
    if (c.threshold > 10000.0f) {
        c.threshold = 10000.0f;
    }
    save();
}

void cycleAction(size_t i) {
    Rule *r = mutableAt(i);
    if (!r) {
        return;
    }
    r->action = (r->action == Action::On) ? Action::Off : Action::On;
    save();
}

void adjustDurationSec(size_t i, int delta) {
    Rule *r = mutableAt(i);
    if (!r) {
        return;
    }
    int v = static_cast<int>(r->durationSec) + delta;
    if (v < 0) {
        v = 0;
    }
    if (v > 3600) {
        v = 3600;
    }
    r->durationSec = static_cast<uint16_t>(v);
    save();
}

void adjustCooldownSec(size_t i, int delta) {
    Rule *r = mutableAt(i);
    if (!r) {
        return;
    }
    long v = static_cast<long>(r->cooldownSec) + delta;
    if (v < 0) {
        v = 0;
    }
    if (v > 86400) {
        v = 86400;
    }
    r->cooldownSec = static_cast<uint32_t>(v);
    save();
}

bool addClause(size_t ruleIx) {
    Rule *r = mutableAt(ruleIx);
    if (!r || r->condCount >= kMaxConds) {
        return false;
    }
    resetClauseSensor(r->cond[r->condCount]);
    ++r->condCount;
    save();
    return true;
}

bool removeClause(size_t ruleIx) {
    Rule *r = mutableAt(ruleIx);
    if (!r || r->condCount <= 1) {
        return false;
    }
    --r->condCount;
    save();
    return true;
}

void setLogic(size_t ruleIx, Logic logic) {
    Rule *r = mutableAt(ruleIx);
    if (!r) {
        return;
    }
    r->logic = logic;
    save();
}

void formatSummary(size_t i, char *buf, size_t n) {
    const Rule *r = at(i);
    if (!r || !buf || n == 0) {
        if (buf && n) {
            buf[0] = '\0';
        }
        return;
    }
    char left[96] = {};
    for (uint8_t c = 0; c < r->condCount; ++c) {
        char piece[40];
        formatClause(r->cond[c], piece, sizeof(piece));
        if (c == 0) {
            strncpy(left, piece, sizeof(left) - 1);
        } else {
            const char *op = (r->logic == Logic::Or) ? " OR " : " AND ";
            strncat(left, op, sizeof(left) - strlen(left) - 1);
            strncat(left, piece, sizeof(left) - strlen(left) - 1);
        }
    }
    const char *prio =
        (r->priority == Priority::Critical)   ? "CRIT"
        : (r->priority == Priority::High)     ? "HIGH"
        : (r->priority == Priority::Low)      ? "LOW"
                                              : "NORM";
    char who[20];
    if (r->localTarget) {
        snprintf(who, sizeof(who), "Master");
    } else if (r->mac[0]) {
        snprintf(who, sizeof(who), "%s", "Atlas");
    } else {
        snprintf(who, sizeof(who), "?");
    }
    if (r->name[0]) {
        snprintf(buf, n, "%s · %s → R%u %s %s [%s]%s", r->name, left,
                 static_cast<unsigned>(r->relay + 1), who,
                 (r->action == Action::On) ? "ON" : "OFF", prio, r->enabled ? "" : " ·off");
    } else {
        snprintf(buf, n, "%s → R%u %s %s [%s]%s", left, static_cast<unsigned>(r->relay + 1), who,
                 (r->action == Action::On) ? "ON" : "OFF", prio, r->enabled ? "" : " ·off");
    }
}

}  // namespace RulesConfig
