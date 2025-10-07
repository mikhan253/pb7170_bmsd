#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from bottle import Bottle, run, response, request, static_file
import json, random, time, threading

app = Bottle()

# === Simulationseinstellungen ===
PACK_COUNT = 5   # bis zu 10 möglich
PACK_NAMES = [f"Pack {i+1} (EVE MB31 314Ah)" for i in range(PACK_COUNT)]

# === Fehlerbits-Beispieltexte ===
ERROR_BITS = [
    "Chip Übertemperatur", "Chip Untertemperatur", "Zellfehler 1", "Zellfehler 2",
    "Kommunikationsfehler", "Spannungsabweichung", "Überstrom", "Isolationsfehler"
]

# === Simulationsdaten ===
bms_data = {}

def init_data():
    for name in PACK_NAMES:
        bms_data[name] = {
            "Alrt": 0,
            "Warn": 0,
            "CellOV": 0,
            "CellUV": 0,
            "CellBal": 0,
            "MOS": 0b011,  # Laden + Entladen aktiv
            "Curr": 0.0,
            "FCurr": 0.0,
            "Cell": [3.2 + random.uniform(-0.05, 0.05) for _ in range(16)],
            "NTC": [25 + random.uniform(-3, 3) for _ in range(4)],
            "Die": 30.0,
            "PVDD": 50.0,
            "Volt": 51.2,
            "MaxCHCurr": 100.0,
            "MaxDSCurr": 200.0,
            "TotCap": 310.0,
            "SOC": random.uniform(30, 90),
            "SOH": 98.5,
            "Cyc": random.randint(1, 50),
        }

init_data()

# === Hintergrundsimulation ===
def simulate_values():
    while True:
        for name, d in bms_data.items():
            # kleine Schwankungen
            d["Curr"] = random.uniform(-10, 10)
            d["Volt"] = sum(d["Cell"]) / len(d["Cell"]) * 16
            d["SOC"] = max(0, min(100, d["SOC"] + random.uniform(-0.1, 0.1)))
            d["Cyc"] += random.uniform(0, 0.01)
            # zufällige Fehler setzen/löschen
            if random.random() < 0.02:
                bit = 1 << random.randint(0, len(ERROR_BITS)-1)
                d["Alrt"] ^= bit
        time.sleep(1)

threading.Thread(target=simulate_values, daemon=True).start()

# === API: Daten abrufen ===
@app.route("/api/bmsdata")
def get_data():
    response.content_type = "application/json"
    return json.dumps(bms_data, indent=4)

# === API: Befehle empfangen ===
@app.post("/api/cmd")
def handle_command():
    cmd = request.json.get("cmd")
    pack = request.json.get("pack")
    print(f"[CMD] {cmd} für {pack if pack else 'alle Packs'}")

    if cmd == "all_on":
        for d in bms_data.values():
            d["MOS"] |= 0b011
    elif cmd == "all_off":
        for d in bms_data.values():
            d["MOS"] = 0
    elif cmd == "clear_errors":
        for d in bms_data.values():
            d["Alrt"] = 0
            d["Warn"] = 0
    elif cmd == "charge_on" and pack in bms_data:
        bms_data[pack]["MOS"] |= 0b001
    elif cmd == "charge_off" and pack in bms_data:
        bms_data[pack]["MOS"] &= ~0b001
    elif cmd == "discharge_on" and pack in bms_data:
        bms_data[pack]["MOS"] |= 0b010
    elif cmd == "discharge_off" and pack in bms_data:
        bms_data[pack]["MOS"] &= ~0b010

    response.content_type = "application/json"
    return json.dumps({"status": "ok", "cmd": cmd, "pack": pack})

# === Frontend (HTML) ===
@app.route('/')
def index():
    return static_file("index.html", root=".")

# === Startserver ===
if __name__ == "__main__":
    print("Starte BMS Bottle Server auf http://localhost:80 ...")
    run(app, host="0.0.0.0", port=80, debug=False, reloader=False)
