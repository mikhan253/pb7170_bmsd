#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import io
import cbor2
import ctypes
import mmap
from bottle import Bottle, run, response, static_file

# ====================================================
# System-Optimierungen
# ====================================================
os.environ["PYTHONOPTIMIZE"] = "1"      # Deaktiviert Assertions & Docstrings
os.environ["PYTHONUNBUFFERED"] = "1"    # Direkte I/O-Ausgabe
os.environ["PYTHONHASHSEED"] = "0"      # Deterministische Hashes

# ====================================================
# Shared Memory Definition
# ====================================================
class PACK_PDO_t(ctypes.Structure):
    _pack_ = 0
    _fields_ = [
        ("id", ctypes.c_uint32),
        ("stateMachine", ctypes.c_uint32),
        ("aliveCounter", ctypes.c_uint32),
        ("spiRetries", ctypes.c_uint32),
        ("swAlertFlags", ctypes.c_uint32),
        ("swWarningFlags", ctypes.c_uint32),
        ("hwStatus", ctypes.c_uint32),
        ("hwAlertFlags", ctypes.c_uint32),
        ("hwAlertState", ctypes.c_uint32),
        ("hwAlertCellUnderOvervoltage", ctypes.c_uint32),
        ("hwAlertAux", ctypes.c_uint32),
        ("hwBalancerTimer", ctypes.c_uint32),
        ("hwBalancerStatus", ctypes.c_uint32),
        ("mosfetStatus", ctypes.c_uint32),
        ("current", ctypes.c_float),
        ("fastCurrent", ctypes.c_float),
        ("cells", ctypes.c_float * 16),
        ("ntcTemperature", ctypes.c_float * 4),
        ("dieTemperature", ctypes.c_float),
        ("voltage", ctypes.c_float),
        ("pvddVoltage", ctypes.c_float),
        ("availableChargeCurrent", ctypes.c_float),
        ("availableDischargeCurrent", ctypes.c_float),
        ("availableCapacity", ctypes.c_float),
        ("totalCapacity", ctypes.c_float),
        ("stateOfCharge", ctypes.c_float),
        ("stateOfHealth", ctypes.c_float),
        ("cycleCount", ctypes.c_float),
    ]

# ====================================================
# Shared Memory Setup
# ====================================================
NUM_PACKS = 10
PACK_SIZE = ctypes.sizeof(PACK_PDO_t)
SHM_NAME = "/dev/shm/battery_pdo_shm"

if not os.path.exists(SHM_NAME):
    raise FileNotFoundError(f"Shared memory '{SHM_NAME}' not found.")
if os.path.getsize(SHM_NAME) != (PACK_SIZE * NUM_PACKS):
    raise ValueError("Shared memory size mismatch!")

_shm_file = open(SHM_NAME, "rb")  # Readonly!
_shm_map = mmap.mmap(_shm_file.fileno(), PACK_SIZE * NUM_PACKS, access=mmap.ACCESS_READ)

# ====================================================
# Hilfsfunktionen
# ====================================================
_PACK_FIELDS = [f[0] for f in PACK_PDO_t._fields_]

def read_pack(index: int) -> PACK_PDO_t:
    """Liest ein PACK_PDO_t aus der Shared Memory Map"""
    if not (0 <= index < NUM_PACKS):
        raise IndexError("Pack index out of range")
    offset = index * PACK_SIZE
    raw = _shm_map[offset: offset + PACK_SIZE]
    return PACK_PDO_t.from_buffer_copy(raw)

def pack_to_dict(pack: PACK_PDO_t) -> dict:
    """Konvertiert PACK_PDO_t in ein Python-Dict"""
    d = {}
    for name in _PACK_FIELDS:
        val = getattr(pack, name)
        d[name] = list(val) if isinstance(val, ctypes.Array) else val
    return d

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
    """Liest alle gültigen Packs und gibt sie als CBOR aus"""
    response.content_type = "application/cbor"

    data = {}
    for i in range(NUM_PACKS):
        pack = read_pack(i)
        if pack.id == 0:
            continue  # Überspringe leere oder inaktive Packs
        data[f"Pack {i+1}"] = pack_to_dict(pack)

    return fast_cbor_dumps(data)

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
    run(app, host="0.0.0.0", server="bjoern", port=80, debug=False, reloader=False, quiet=True)
