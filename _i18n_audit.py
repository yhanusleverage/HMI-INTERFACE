# -*- coding: utf-8 -*-
from pathlib import Path
import re

h = Path(r"include/AppStrings.h").read_text(encoding="utf-8")
cpp = Path(r"src/data/AppStrings.cpp").read_text(encoding="utf-8")

enum = re.search(r"enum class Msg.*?\};", h, re.S).group(0)
msgs = []
for line in enum.splitlines():
    line = line.split("//")[0].strip().rstrip(",")
    if not line or line.startswith("enum") or line.startswith("}"):
        continue
    name = line.split("=")[0].strip()
    if name == "Count":
        continue
    msgs.append(name)
print("Msg count (excl Count):", len(msgs))


def extract_array(name):
    m = re.search(rf"const char \*const {name}\[\] = \{{(.*?)\n\}};", cpp, re.S)
    body = m.group(1)
    strings = re.findall(r'"((?:\\.|[^"\\])*)"', body)
    return strings


es, en, pt = extract_array("kEs"), extract_array("kEn"), extract_array("kPt")
print(f"kEs={len(es)} kEn={len(en)} kPt={len(pt)}")
assert len(es) == len(en) == len(pt) == len(msgs), (
    len(es),
    len(en),
    len(pt),
    len(msgs),
)

print("\n=== Identical ES=EN=PT ===")
for i, (a, b, c) in enumerate(zip(es, en, pt)):
    if a == b == c and any(ch.isalpha() for ch in a):
        print(f"{i:3d} Msg::{msgs[i]}: {a!r}")

print("\n=== ES==EN, PT differs (untranslated ES?) ===")
for i, (a, b, c) in enumerate(zip(es, en, pt)):
    if a == b and a != c and any(ch.isalpha() for ch in a):
        print(f"{i:3d} Msg::{msgs[i]}: ES/EN={a!r} PT={c!r}")

print("\n=== EN==PT, ES differs ===")
for i, (a, b, c) in enumerate(zip(es, en, pt)):
    if b == c and a != b and any(ch.isalpha() for ch in b):
        print(f"{i:3d} Msg::{msgs[i]}: EN/PT={b!r} ES={a!r}")

print("\n=== ES==PT, EN differs (Romance shared / OK?) ===")
for i, (a, b, c) in enumerate(zip(es, en, pt)):
    if a == c and a != b and any(ch.isalpha() for ch in a):
        print(f"{i:3d} Msg::{msgs[i]}: ES/PT={a!r} EN={b!r}")

print("\n=== Possible PT leak into ES (Malha/voce/estufa/etc) ===")
pt_pat = re.compile(
    r"Malha|voce|estufa|vazao|Configuracao|Homogene|toque |regras|Receita|Seguinte|Agressiv|Lig/Des|Apagar|Voltar|Monitoramento|faixas|Armado",
    re.I,
)
for i, (a, b, c) in enumerate(zip(es, en, pt)):
    if pt_pat.search(a) and a == c:
        print(f"{i:3d} Msg::{msgs[i]}: ES={a!r} EN={b!r}")

print("\n=== English leftovers in ES (Setup/Rules/Sensors/etc) ===")
en_words = [
    "Setup",
    "Sensors",
    "Rules",
    "Reservoir",
    "Backlight",
    "Units",
    "Restart",
    "About",
    "Display Readings",
    "Dosing Info",
    "Multi Dose",
    "Cloud",
    "Manual",
    "Batch",
    "Recirc",
    "INACTIVE",
    "DOSING",
    "Hold ON",
    "Hold OFF",
    "Coming soon",
]
for i, (a, b, c) in enumerate(zip(es, en, pt)):
    for w in en_words:
        if a == w or (w in a and a == b):
            print(f"{i:3d} Msg::{msgs[i]}: ES={a!r} EN={b!r} PT={c!r}")
            break

print("\n=== Empty ===")
for i, (a, b, c) in enumerate(zip(es, en, pt)):
    if a == "" or b == "" or c == "":
        print(i, msgs[i], repr(a), repr(b), repr(c))

# dump paired for neighbor check around suspicious indices
print("\n=== Neighbor dump around Setup block ===")
for i in range(115, 150):
    print(f"{i:3d} {msgs[i]:28s} | {es[i]!r:40s} | {en[i]!r:40s} | {pt[i]!r}")
