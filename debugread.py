#!/usr/bin/env python3
import posix_ipc
import mmap
import struct
import ctypes

SHM_NAME = "/battery_pdo_shm"
MAX_BATTERY_PACKS = 10

# Definition der Struktur BATTERY_PDO_t in Python (möglichst 1:1 zu deinem C)
class PB7170_Statemachine_t(ctypes.c_uint32):
    pass

class BATTERY_PDO_t(ctypes.Structure):
    _fields_ = [
        ("ID", ctypes.c_uint32),
        ("Statemachine", ctypes.c_uint32),
        ("SPI_ErrorCount", ctypes.c_uint32),

        # SW Flags
        ("SW_AlertFlags", ctypes.c_uint32),

        # HW Status
        ("HW_Status", ctypes.c_uint32),
        ("HW_AlertFlags", ctypes.c_uint32),
        ("HW_AlertState", ctypes.c_uint32),
        ("HW_Alert_CellUnderOvervoltage", ctypes.c_uint32),
        ("HW_Alert_CellUndervoltage", ctypes.c_uint32),
        ("HW_AlertAux", ctypes.c_uint32),
        ("HW_BalanceTimer", ctypes.c_uint32),
        ("HW_BalanceStatus", ctypes.c_uint32),

        # MOSFETs / Balancer
        ("MOSFet_Status", ctypes.c_uint32),
        ("BalancerState", ctypes.c_uint32),

        # Ströme, Spannungen, Temperaturen
        ("Current", ctypes.c_float),
        ("FastCurrent", ctypes.c_float),
        ("V_Cells", ctypes.c_float * 16),
        ("NTC_Temps", ctypes.c_float * 4),
        ("DieTemp", ctypes.c_float),
        ("PackVoltage", ctypes.c_float),
        ("PVDDVoltage", ctypes.c_float),
        ("AvailableChargeCurrent", ctypes.c_float),
        ("AvailableDischargeCurrent", ctypes.c_float),

        ("Capacity", ctypes.c_float),
        ("SOC", ctypes.c_float),
        ("SOH", ctypes.c_float),
        ("CycleCount", ctypes.c_float),
    ]

# Größe eines Packs
PACK_SIZE = ctypes.sizeof(BATTERY_PDO_t)

def main():
    # Verbinden mit bestehendem Shared Memory
    try:
        shm = posix_ipc.SharedMemory(SHM_NAME, flags=0)  # read-only Zugriff
    except posix_ipc.ExistentialError:
        print(f"❌ Shared Memory '{SHM_NAME}' nicht gefunden.")
        return

    # mmap öffnen
    with mmap.mmap(shm.fd, PACK_SIZE * MAX_BATTERY_PACKS, mmap.MAP_SHARED, mmap.PROT_READ) as mm:
        # Datei-Deskriptor schließen (nicht mehr nötig nach mmap)
        shm.close_fd()

        # Daten von pack0 lesen
        data = mm[:PACK_SIZE]
        pack0 = BATTERY_PDO_t.from_buffer_copy(data)

        print("=== Inhalt von Pack 0 ===")
        print(f"ID: {pack0.ID}")
        print(f"Statemachine: {pack0.Statemachine}")
        print(f"SPI_ErrorCount: {pack0.SPI_ErrorCount}")
        print(f"SW_AlertFlags: 0x{pack0.SW_AlertFlags:08X}")
        print(f"HW_Status: 0x{pack0.HW_Status:08X}")
        print(f"HW_AlertFlags: 0x{pack0.HW_AlertFlags:08X}")
        print(f"HW_AlertState: 0x{pack0.HW_AlertState:08X}")
        print(f"HW_Alert_CellUnderOvervoltage: {pack0.HW_Alert_CellUnderOvervoltage}")
        print(f"HW_AlertAux: 0x{pack0.HW_AlertAux:04X}")
        print(f"HW_BalanceTimer: {pack0.HW_BalanceTimer}")
        print(f"HW_BalanceStatus: {pack0.HW_BalanceStatus}")
        print(f"MOSFet_Status: 0x{pack0.MOSFet_Status:08X}")
        print(f"BalancerState: {pack0.BalancerState}")
        print(f"Current: {pack0.Current:.3f} A")
        print(f"FastCurrent: {pack0.FastCurrent:.3f} A")
        print(f"PackVoltage: {pack0.PackVoltage:.3f} V")
        print(f"PVDDVoltage: {pack0.PVDDVoltage:.3f} V")
        print(f"Capacity: {pack0.Capacity:.1f} Ah")
        print(f"SOC: {pack0.SOC:.2f} %")
        print(f"SOH: {pack0.SOH:.2f} %")
        print(f"CycleCount: {pack0.CycleCount:.0f}")
        print(f"DieTemp: {pack0.DieTemp:.2f} °C")
        print("\nZellspannungen:")
        for i, v in enumerate(pack0.V_Cells):
            print(f"  Cell[{i:02d}]: {v:.3f} V")

        print("\nNTC Temperaturen:")
        for i, t in enumerate(pack0.NTC_Temps):
            print(f"  NTC[{i}]: {t:.2f} °C")

if __name__ == "__main__":
    main()
