#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import os
import io
import cbor2
import ctypes
import mmap
from bottle import Bottle, run, response, static_file
from dataobjects import GLOBAL_PDO_t, PACK_PDO_t
from ctypes import Structure, addressof, sizeof

# ====================================================
# Shared Memory Setup
# ====================================================
PDO_SHM_NAME = "/dev/shm/battery_pdo_shm"

if not os.path.exists(PDO_SHM_NAME):
    raise FileNotFoundError(f"Shared memory '{PDO_SHM_NAME}' not found.")

_pdo_shm_file = open(PDO_SHM_NAME, "rb")  # Readonly!
_pdo_shm_map = mmap.mmap(_pdo_shm_file.fileno(), 0, access=mmap.ACCESS_READ)

# Erste globale Struktur lesen, um Anzahl der Packs zu kennen
pdo_data_glob = GLOBAL_PDO_t.from_buffer_copy(_pdo_shm_map)

class PDO_BATTERY_SYSTEM_t(Structure):
    _fields_ = [
        ("glob", GLOBAL_PDO_t),
        ("pack", PACK_PDO_t * pdo_data_glob.numberOfPacks)
    ]

# ====================================================
# Ctypes -> Dict Hilfsfunktionen
# ====================================================
def struct_from_ctypes(name: str, ctypes_struct: type, depth=0, _cache=None) -> dict:
    """Erzeugt eine Python-Datenbeschreibung (Schema) aus einer ctypes.Structure."""
    if _cache is None:
        _cache = {}
    if ctypes_struct in _cache:
        return _cache[ctypes_struct]

    schema = {}
    for field_name, field_type in ctypes_struct._fields_:
        # Unterstruktur
        if issubclass(field_type, Structure):
            schema[field_name] = struct_from_ctypes(field_name, field_type, depth + 1, _cache)

        # Array
        elif hasattr(field_type, "_type_") and hasattr(field_type, "_length_"):
            elem_type = field_type._type_
            if issubclass(elem_type, Structure):
                schema[field_name] = [struct_from_ctypes(field_name, elem_type, depth + 1, _cache)
                                      for _ in range(field_type._length_)]
            else:
                schema[field_name] = [0 for _ in range(field_type._length_)]

        # Primitive Typen
        else:
            schema[field_name] = 0

    _cache[ctypes_struct] = schema
    return schema


def instance_from_ctypes(schema, c_obj):
    """Erzeugt ein Python-Objekt (dict/list) aus ctypes-Daten anhand eines Schemas."""
    if isinstance(schema, dict):
        return {k: instance_from_ctypes(v, getattr(c_obj, k)) for k, v in schema.items()}
    elif isinstance(schema, list):
        if len(schema) == 0:
            return []
        elem_schema = schema[0]
        return [instance_from_ctypes(elem_schema, elem) for elem in c_obj]
    else:
        return c_obj


def update_from_ctypes(obj, schema, c_obj):
    """Aktualisiert rekursiv bestehendes Python-Objekt (dict/list) aus ctypes-Daten."""
    if isinstance(obj, dict):
        for k, v in obj.items():
            if isinstance(v, (dict, list)):
                update_from_ctypes(v, schema[k], getattr(c_obj, k))
            else:
                obj[k] = getattr(c_obj, k)
    elif isinstance(obj, list):
        for i, v in enumerate(obj):
            if isinstance(v, (dict, list)):
                update_from_ctypes(v, schema[0], c_obj[i])
            else:
                obj[i] = c_obj[i]


# ====================================================
# Initiale Strukturen anlegen
# ====================================================
schema = struct_from_ctypes("pdo", PDO_BATTERY_SYSTEM_t)
pdo_data = PDO_BATTERY_SYSTEM_t.from_buffer_copy(_pdo_shm_map)
pdo_dict = instance_from_ctypes(schema, pdo_data)

print("Initialer Zustand geladen.")
print(f"  Packs: {pdo_data.glob.numberOfPacks}")

# ====================================================
# CBOR Encoder (optimiert)
# ====================================================
_encoder_buf = io.BytesIO()
_encoder = cbor2.CBOREncoder(_encoder_buf)

def fast_cbor_dumps(obj):
    """Schneller CBOR-Encoder mit wiederverwendetem Buffer"""
    _encoder_buf.seek(0)
    _encoder_buf.truncate(0)
    _encoder.encode(obj)
    return _encoder_buf.getvalue()


# ====================================================
# Bottle App
# ====================================================
app = Bottle()

@app.get("/api/bmsdata")
def bms_data():
    """Liest alle gültigen Packs aus Shared Memory und gibt sie als CBOR aus"""
    # Shared memory erneut auslesen → neue Werte
    ctypes.memmove(addressof(pdo_data), _pdo_shm_map[:sizeof(pdo_data)], sizeof(pdo_data))

    # Dictionary aktualisieren (in-place)
    update_from_ctypes(pdo_dict, schema, pdo_data)

    # CBOR-Antwort senden
    response.content_type = "application/cbor"
    return fast_cbor_dumps(pdo_dict)


# ====================================================
# Statische Dateien
# ====================================================
@app.get("/")
def index():
    return static_file("index.html", root=".")

@app.get("/<filepath:path>")
def server_static(filepath):
    return static_file(filepath, root=".")

# ====================================================
# Serverstart
# ====================================================
if __name__ == "__main__":
    run(app, host="0.0.0.0", server="bjoern", port=80, debug=False, reloader=False, quiet=False)
