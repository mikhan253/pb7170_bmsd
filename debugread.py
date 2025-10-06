import mmap
import ctypes
import os
import time
import sys

# SHM Name und Konstanten
SHM_NAME = "/battery_pdo_shm"
MAX_BATTERY_PACKS = 10
NUM_CELLS = 16
NUM_NTC = 4

# -----------------------------
# Bitfeld-Strukturen für Flags
# -----------------------------
class SWAlertFlags(ctypes.LittleEndianStructure):
    _fields_ = [
        ("CHARGE_OC", ctypes.c_uint32, 1),
        ("DISCHARGE_OC", ctypes.c_uint32, 1),
        ("STATE_ERR", ctypes.c_uint32, 1),
        ("OVERTEMP", ctypes.c_uint32, 1),
        ("UNDERTEMP", ctypes.c_uint32, 1),
        ("COMM_ERR", ctypes.c_uint32, 1),
        ("reserved", ctypes.c_uint32, 26),
    ]

class HWStatusBits(ctypes.LittleEndianStructure):
    _fields_ = [
        ("STA_LODD", ctypes.c_uint32, 1),
        ("reserved1", ctypes.c_uint32, 1),
        ("CD", ctypes.c_uint32, 2),
        ("STA_SLEEP", ctypes.c_uint32, 1),
        ("STA_WAKE", ctypes.c_uint32, 1),
        ("BLSW_ON", ctypes.c_uint32, 1),
        ("ADC_ON", ctypes.c_uint32, 1),
        ("STA_CHGD", ctypes.c_uint32, 1),
        ("reserved2", ctypes.c_uint32, 1),
        ("SCH_CNT", ctypes.c_uint32, 4),
        ("reserved3", ctypes.c_uint32, 18)
    ]

class HWAlertFlagsBits(ctypes.LittleEndianStructure):
    _fields_ = [
        ("CHARGE_OC", ctypes.c_uint32, 1),
        ("DISCHARGE_OC", ctypes.c_uint32, 1),
        ("SHORT", ctypes.c_uint32, 1),
        ("BAL_TIMEOUT", ctypes.c_uint32, 1),
        ("BAL_UV", ctypes.c_uint32, 1),
        ("SCHED_END", ctypes.c_uint32, 1),
        ("TIM_END", ctypes.c_uint32, 1),
        ("WDT_OVF", ctypes.c_uint32, 1),
        ("EXT_PROT", ctypes.c_uint32, 1),
        ("PVDD_UVOV", ctypes.c_uint32, 1),
        ("CELL_UV", ctypes.c_uint32, 1),
        ("CELL_OV", ctypes.c_uint32, 1),
        ("LV", ctypes.c_uint32, 1),
        ("THERM_SD", ctypes.c_uint32, 1),
        ("CHGDD", ctypes.c_uint32, 1),
        ("LODD", ctypes.c_uint32, 1),
        ("SPI_CRC_ERR", ctypes.c_uint32, 1),
        ("MISMATCH", ctypes.c_uint32, 1),
        ("reserved1", ctypes.c_uint32, 4),
        ("TDIE_HI", ctypes.c_uint32, 1),
        ("TDIE_LO", ctypes.c_uint32, 1),
        ("reserved2", ctypes.c_uint32, 1),
        ("PACK_UV", ctypes.c_uint32, 1),
        ("PACK_OV", ctypes.c_uint32, 1),
        ("reserved3", ctypes.c_uint32, 2),
        ("AUX_OV", ctypes.c_uint32, 1),
        ("AUX_UV", ctypes.c_uint32, 1),
        ("reserved4", ctypes.c_uint32, 1),
        ("MEAS_DONE", ctypes.c_uint32, 1)
    ]

class HWAlertStateBits(ctypes.LittleEndianStructure):
    _fields_ = [
        ("CHARGE_OC", ctypes.c_uint32, 1),
        ("DISCHARGE_OC", ctypes.c_uint32, 1),
        ("SHORT", ctypes.c_uint32, 1),
        ("CELL_UV", ctypes.c_uint32, 1),
        ("CELL_OV", ctypes.c_uint32, 1),
        ("PVDD_UVOV", ctypes.c_uint32, 1),
        ("reserved1", ctypes.c_uint32, 8),
        ("RESET", ctypes.c_uint32, 1),
        ("SLEEP", ctypes.c_uint32, 1),
        ("VREF", ctypes.c_uint32, 1),
        ("LVMUX", ctypes.c_uint32, 1),
        ("AVDD", ctypes.c_uint32, 1),
        ("DVDD", ctypes.c_uint32, 1),
        ("MISMATCH", ctypes.c_uint32, 1),
        ("TDIE_LO", ctypes.c_uint32, 1),
        ("TDIE_HI", ctypes.c_uint32, 1),
        ("SPI_CRC_ERR", ctypes.c_uint32, 1),
        ("EEPROM_CRC_ERR", ctypes.c_uint32, 1),
        ("PACK_UV", ctypes.c_uint32, 1),
        ("PACK_OV", ctypes.c_uint32, 1),
        ("reserved2", ctypes.c_uint32, 4),
        ("CLOCK_ABNORMAL", ctypes.c_uint32, 1)
    ]

class HWAuxAlertBits(ctypes.LittleEndianStructure):
    _fields_ = [
        ("AUXIN1_OV", ctypes.c_uint32, 1),
        ("AUXIN2_OV", ctypes.c_uint32, 1),
        ("AUXIN3_OV", ctypes.c_uint32, 1),
        ("AUXIN4_OV", ctypes.c_uint32, 1),
        ("reserved1", ctypes.c_uint32, 4),
        ("AUXIN1_UV", ctypes.c_uint32, 1),
        ("AUXIN2_UV", ctypes.c_uint32, 1),
        ("AUXIN3_UV", ctypes.c_uint32, 1),
        ("AUXIN4_UV", ctypes.c_uint32, 1),
        ("reserved2", ctypes.c_uint32, 20)
    ]

class MOSFetStatusBits(ctypes.LittleEndianStructure):
    _fields_ = [
        ("PRECHARGE", ctypes.c_uint32, 1),
        ("CHARGE", ctypes.c_uint32, 1),
        ("DISCHARGE", ctypes.c_uint32, 1),
        ("reserved", ctypes.c_uint32, 29)
    ]

# -----------------------------
# BATTERY_PDO_t Struktur
# -----------------------------
class BATTERY_PDO(ctypes.LittleEndianStructure):
    _fields_ = [
        ("ID", ctypes.c_uint32),
        ("Statemachine", ctypes.c_uint32),
        ("AliveCounter", ctypes.c_uint32),
        ("SPI_Retries", ctypes.c_uint32),
        ("SW_AlertFlags", SWAlertFlags),
        ("SW_WarningFlags", ctypes.c_uint32),
        ("HW_Status", HWStatusBits),
        ("HW_AlertFlags", HWAlertFlagsBits),
        ("HW_AlertState", HWAlertStateBits),
        ("HW_Alert_CellUnderOvervoltage", ctypes.c_uint32),
        ("HW_AlertAux", HWAuxAlertBits),
        ("HW_BalanceTimer", ctypes.c_uint32),
        ("HW_BalanceStatus", ctypes.c_uint32),
        ("MOSFet_Status", MOSFetStatusBits),
        ("BalancerState", ctypes.c_uint32),
        ("Current", ctypes.c_float),
        ("FastCurrent", ctypes.c_float),
        ("V_Cells", ctypes.c_float * NUM_CELLS),
        ("NTC_Temperatures", ctypes.c_float * NUM_NTC),
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

# -----------------------------
# Hilfsfunktion zum Bitfeld-Ausdruck
# -----------------------------
def print_bits(name, bitstruct, fields_per_row=4, field_width=20):
    print(f"{name}:")
    count = 0
    for field_info in bitstruct._fields_:
        field_name = field_info[0]
        if field_name.startswith("reserved"):
            continue
        print(f"  {field_name}: {getattr(bitstruct, field_name)}".ljust(field_width), end="")
        count += 1
        if count == fields_per_row:
            print()
            count = 0
    if count != 0:
        print()

# -----------------------------
# Prüfen des Arguments
# -----------------------------
if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} <pack_number (0-{MAX_BATTERY_PACKS-1})>")
    sys.exit(1)

try:
    pack_index = int(sys.argv[1])
    if not (0 <= pack_index < MAX_BATTERY_PACKS):
        raise ValueError
except ValueError:
    print(f"Pack number must be between 0 and {MAX_BATTERY_PACKS-1}")
    sys.exit(1)

# -----------------------------
# Shared Memory öffnen readonly
# -----------------------------
fd = os.open("/dev/shm" + SHM_NAME, os.O_RDONLY)
shm_size = ctypes.sizeof(BATTERY_PDO) * MAX_BATTERY_PACKS
buf = mmap.mmap(fd, shm_size, mmap.MAP_SHARED, mmap.PROT_READ)
os.close(fd)

while True:
    # -----------------------------
    # pack auslesen
    # -----------------------------
    start = ctypes.sizeof(BATTERY_PDO) * pack_index
    end = start + ctypes.sizeof(BATTERY_PDO)
    pack_buf = buf[start:end]
    pack = BATTERY_PDO.from_buffer_copy(pack_buf)

    # -----------------------------
    # pack Variablen ausgeben
    # -----------------------------
    print(f"Pack{pack_index} ID: {pack.ID}")
    print(f"Statemachine: {pack.Statemachine}")
    print(f"AliveCounter: {pack.AliveCounter}")
    print(f"SPI_Retries: {pack.SPI_Retries}")

    print_bits("SW_AlertFlags", pack.SW_AlertFlags)
    print(f"SW_WarningFlags: {pack.SW_WarningFlags:032b}")
    print_bits("HW_Status", pack.HW_Status)
    print_bits("HW_AlertFlags", pack.HW_AlertFlags)
    print_bits("HW_AlertState", pack.HW_AlertState)
    print(f"HW_Alert_CellUnderOvervoltage: {pack.HW_Alert_CellUnderOvervoltage:032b}")
    print_bits("HW_AlertAux", pack.HW_AlertAux)
    print(f"HW_BalanceTimer: {pack.HW_BalanceTimer}")
    print(f"HW_BalanceStatus: {pack.HW_BalanceStatus:016b}")
    print_bits("MOSFet_Status", pack.MOSFet_Status)

    print(f"BalancerState: {pack.BalancerState}")
    print(f"Current: {pack.Current:.3f}")
    print(f"FastCurrent: {pack.FastCurrent:.3f}")
    print(f"V_Cells: ", end="")
    for i in range(NUM_CELLS):
        print(f"{pack.V_Cells[i]:.3f}", end=", " if i < NUM_CELLS - 1 else "\n")
    print(f"NTC_Temperatures: ", end="")
    for i in range(NUM_NTC):
        print(f"{pack.NTC_Temperatures[i]:.3f}", end=", " if i < NUM_NTC - 1 else "\n")
    print(f"DieTemp: {pack.DieTemp:.3f}")
    print(f"PackVoltage: {pack.PackVoltage:.3f}")
    print(f"PVDDVoltage: {pack.PVDDVoltage:.3f}")
    print(f"AvailableChargeCurrent: {pack.AvailableChargeCurrent:.3f}")
    print(f"AvailableDischargeCurrent: {pack.AvailableDischargeCurrent:.3f}")
    print(f"Capacity: {pack.Capacity:.3f}")
    print(f"SOC: {pack.SOC:.3f}")
    print(f"SOH: {pack.SOH:.3f}")
    print(f"CycleCount: {pack.CycleCount:.3f}")
    time.sleep(1)
    os.system('clear')

buf.close()
