#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import random
import cbor2

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

SHM_NAME="/battery_pdo_shm"
SHM_SIZE=1880
# Shared Memory öffnen
fd = os.open("/dev/shm" + SHM_NAME, os.O_RDONLY)

# Memory Mapping erstellen
buf = mmap.mmap(fd, SHM_SIZE, mmap.MAP_SHARED, mmap.PROT_READ)

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
