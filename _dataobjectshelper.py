#!/usr/bin/env python3
import re
from pathlib import Path

HEADER_FILE = "dataobjects.h"

# --------------------------
# Welche Strukturen in welche Datei
# --------------------------
CONF_STRUCTS = ["GLOBAL_CONF_t","PACK_USERCONF_t","PACK_GENERALCONF_t","PACK_CALIBRATION_t"]
WEBSERVER_STRUCTS = ["GLOBAL_PDO_t","PACK_PDO_t","PACK_SDO_t"]

OUTPUT_FILES = {
    "conf/dataobjects.py": CONF_STRUCTS,
    "webserver/dataobjects.py": WEBSERVER_STRUCTS,
}

# --------------------------
# 1️⃣ #define Konstanten extrahieren
# --------------------------
def extract_defines(content: str):
    defines = {}
    for m in re.finditer(r'#define\s+([A-Za-z_]\w+)\s+([0-9]+)', content):
        defines[m.group(1)] = int(m.group(2))
    return defines

# --------------------------
# 2️⃣ typedef struct parsen, auch __attribute__((packed))
# --------------------------
def extract_all_typedef_structs(content: str):
    results = {}
    i = 0
    L = len(content)
    while True:
        m = re.search(r'\btypedef\s+struct\b', content[i:])
        if not m:
            break
        start_pos = i + m.start()
        packed = "__attribute__((packed))" in content[start_pos : start_pos + 80]

        brace_open = content.find('{', start_pos)
        if brace_open == -1:
            break
        j = brace_open + 1
        depth = 1
        while j < L and depth > 0:
            if content[j] == '{':
                depth += 1
            elif content[j] == '}':
                depth -= 1
            j += 1
        if depth != 0:
            break
        brace_close = j - 1
        semi = content.find(';', brace_close)
        if semi == -1:
            break
        tail = content[brace_close + 1 : semi]
        nm_m = re.search(r'([A-Za-z_]\w+)', tail)
        if nm_m:
            name = nm_m.group(1)
            body = content[brace_open + 1 : brace_close]
            results[name] = (body, packed)
        i = semi + 1
    return results

# --------------------------
# 3️⃣ Struktur Body parsen
# --------------------------
def parse_struct_body(struct_body: str):
    fields = []
    seen = set()
    lines = struct_body.splitlines()

    for line in lines:
        line = line.strip()
        if not line or line.startswith("//") or line.startswith("/*"):
            continue
        # Union überspringen, aber erste Variable aufnehmen
        if line.startswith("union"):
            m = re.search(r'(\w+)\s*;', line)
            if m:
                name = m.group(1)
                fields.append(("uint32_t", name))
            continue
        # Bitfield-Structs überspringen
        if ':' in line:
            continue
        # Normale Deklaration
        m = re.match(r'([\w\s\*]+?)\s+([A-Za-z_]\w*(?:\[[\w\d_]+\])?)\s*;', line)
        if m:
            typ = m.group(1).strip()
            name = m.group(2).strip()
            # Array erkennen
            array_match = re.match(r'([A-Za-z_]\w*)\s*\[\s*([\w\d_]+)\s*\]', name)
            if array_match:
                name = array_match.group(1)
                typ = f"{typ}[{array_match.group(2)}]"
            # Doppelte *_bits Felder überspringen
            if name.endswith("_bits") and name[:-5] in seen:
                continue
            fields.append((typ, name))
            seen.add(name)
    return fields

# --------------------------
# 4️⃣ CType Mapping
# --------------------------
CTYPE_MAP = {
    "uint32_t": "c_uint32",
    "int32_t": "c_int32",
    "float": "c_float",
    "double": "c_double",
    "uint16_t": "c_uint16",
    "int16_t": "c_int16",
    "uint8_t": "c_uint8",
    "int8_t": "c_int8",
    "EStateMachine_t": "c_uint32",
}

def map_to_ctype(typ: str, defines: dict):
    """
    Wandelt C-Typen in ctypes um, behandelt Arrays
    """
    array_match = re.match(r'(\w+)\[(\w+)\]', typ)
    if array_match:
        base = array_match.group(1)
        size_str = array_match.group(2)
        base_ctype = CTYPE_MAP.get(base, "c_uint32")
        if size_str in defines:
            size = defines[size_str]
        else:
            try:
                size = int(size_str)
            except ValueError:
                size = 1
        return f"({base_ctype} * {size})"
    return CTYPE_MAP.get(typ, "c_uint32")

# --------------------------
# 5️⃣ Klassen generieren
# --------------------------
def generate_ctypes_class(name, fields, defines, packed=False):
    out = [f"class {name}(Structure):"]
    if packed:
        out.append("    _pack_ = 1")
    out.append("    _fields_ = [")
    for typ, field in fields:
        ctype = map_to_ctype(typ, defines)
        out.append(f'        ("{field}", {ctype}),')
    out.append("    ]\n")
    return "\n".join(out)

# --------------------------
# 6️⃣ Main
# --------------------------
def main():
    header_path = Path(HEADER_FILE)
    if not header_path.exists():
        print(f"✗ Headerdatei nicht gefunden: {HEADER_FILE}")
        return

    content = header_path.read_text(encoding="utf-8")
    content = re.sub(r"//.*?\n", "\n", content)
    content = re.sub(r"/\*.*?\*/", "", content, flags=re.DOTALL)

    defines = extract_defines(content)
    structs = extract_all_typedef_structs(content)

    for out_file, struct_list in OUTPUT_FILES.items():
        text = "from ctypes import *\n\n"

        for name in struct_list:
            if name in structs:
                body, packed = structs[name]
                fields = parse_struct_body(body)
                text += generate_ctypes_class(name, fields, defines, packed)
                print(f"✓ {name} für {out_file}")
            else:
                print(f"✗ {name} nicht gefunden!")

        Path(out_file).parent.mkdir(parents=True, exist_ok=True)
        Path(out_file).write_text(text, encoding="utf-8")
        print(f"✓ {out_file} geschrieben\n")

if __name__ == "__main__":
    main()
