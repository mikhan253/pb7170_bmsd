#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import random
import cbor2
import ctypes
import mmap
import os
import struct

from bottle import Bottle, run, response, static_file

# ====================================================
# System-Optimierungen
# ====================================================
os.environ["PYTHONOPTIMIZE"] = "1"      # Deaktiviert Assertions & Docstrings
os.environ["PYTHONUNBUFFERED"] = "1"    # Direkte I/O-Ausgabe
os.environ["PYTHONHASHSEED"] = "0"      # Deterministische Hashes

# ====================================================
# Shmem öffnen
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
    
print(ctypes.sizeof(PACK_PDO_t) * 10)
shm_name = "/dev/shm/battery_pdo_shm"
if not os.path.exists(shm_name):
    raise FileNotFoundError(f"Shared memory '{shm_name}' not found.")
#if os.path.getsize(shm_name) != (ctypes.sizeof(PACK_PDO_t) * 10):
#    raise ValueError(f"Shared memory size mismatch!")
def print_pack(pack):
    for name, _ in pack._fields_:
        value = getattr(pack, name)
        # Arrays etwas schöner ausgeben
        if isinstance(value, ctypes.Array):
            value = list(value)
        print(f"{name}: {value}")

_shm_file = open(shm_name, "rb")  # Readonly!
_shm_map = mmap.mmap(_shm_file.fileno(), ctypes.sizeof(PACK_PDO_t) * 10, access=mmap.ACCESS_READ)

offset = 0 * ctypes.sizeof(PACK_PDO_t)
raw = _shm_map[offset : offset + ctypes.sizeof(PACK_PDO_t)]
print_pack(PACK_PDO_t.from_buffer_copy(raw))


quit()


# ====================================================
# Bottle App
# ====================================================
app = Bottle()

# ====================================================
# Dummy BMS-Daten
# ====================================================
def generate_pack(name):
    cells = [round(random.uniform(3.0, 4.2), 3) for _ in range(16)]
    ntcs = [round(random.uniform(20, 40), 1) for _ in range(4)]
    return {
        "Volt": round(sum(cells), 3),
        "Curr": round(random.uniform(0, 10), 3),
        "Cell": cells,
        "NTC": ntcs,
        "SOC": round(random.uniform(0, 100), 1),
        "SOH": round(random.uniform(90, 100), 1),
        "Alrt": random.randint(0, 255),
        "Warn": random.randint(0, 255),
        "MOS": random.randint(0, 7),
        "CellOV": 0,
        "CellUV": 0,
        "CellBal": random.randint(0, 0xFFFF),
        "Die": round(random.uniform(20, 40), 3),
        "PVDD": round(random.uniform(12, 14), 3),
        "Cyc": random.randint(0, 100)
    }

# ====================================================
# API Endpoint für CBOR (Punkt 2)
# ====================================================
@app.get("/api/bmsdata")
def bms_data():
    response.content_type = "application/cbor"
    data = {f"Pack {i+1}": generate_pack(f"Pack {i+1}") for i in range(10)}
    out = cbor2.dumps(data, canonical=True)
    return out

# ====================================================
# Bereitstellung index.html
# ====================================================
@app.get("/")
def index():
    return static_file("index.html", root=".")

# ====================================================
# Statische Dateien (CSS/JS)
# ====================================================
@app.get("/<filepath:path>")
def server_static(filepath):
    return static_file(filepath, root=".")

# ====================================================
# Serverstart
# ====================================================
if __name__ == "__main__":
    run(app, host="0.0.0.0", server='bjoern', port=80, debug=False, reloader=False, quiet=True)
