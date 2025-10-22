#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import json
import array
import math
import struct
import os
from dataobjects import GLOBAL_CONF_t, PACK_USERCONF_t, PACK_GENERALCONF_t, PACK_CALIBRATION_t
import ctypes

DEBUG=0

ID = 0
totalpacks = 0
for ID in range(16):

    if not os.path.exists(f"pack{ID}.json"):
        break

    with open(f'pack{ID}.json', 'r') as file:
        configdata = json.load(file)
    print(f'pack{ID}.json')
    totalpacks += 1


    ###################################################
    ## USER CONFIG 0x14-0x3D                         ##
    ###################################################
    FILENAME=f'pack{ID}_userconf.bin'
    print("->",FILENAME)

    userconf = []

    def setbits(allowed_values, values, start_bit, registername):
        ret = 0
        if not all(value in allowed_values for value in values):
            raise ValueError(registername + ": Ungültiger Wert gefunden")
        for i,val in enumerate(allowed_values,start_bit):
            ret |= int(val in values) << i
        return ret

    def calcvalue(value, factor, maxbits, registername):
        tmp = round(value / factor, 3)
        if tmp % 1 != 0:
            raise ValueError(f"{registername} muss ganzzahlig sein, ist aber: {tmp}")
        tmp = int(tmp)
        if not (0 <= tmp <= 2**maxbits - 1):	
            raise ValueError(f"{registername} ausserhalb von Bereich: {tmp}")
        return tmp

    def getelement(allowed_values, value, registername):
        if value not in allowed_values:
            raise ValueError(registername + ": Ungültiger Wert gefunden")
        return allowed_values.index(value)

    ######### MOS_CTRL 0x14 #########
    regname = "MOS_CTRL"; addr = 0x14
    data = int(configdata["User Configuration"]["Charger Detection"]) << 11
    data |= int(configdata["User Configuration"]["Load Detection"]) << 2
    data |= setbits(
        [
            'Watchdog',
            'Cell Overvoltage',
            'Cell Undervoltage',
            'External Protection',
            'Discharge Overcurrent',
            'Short Circuit',
            'Charge Overcurrent',
            'Pack Low Voltage',
        ],
        configdata["User Configuration"]["Precharge Mosfet"],
        3, 
        regname)
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])
    ######### MOS_SHUT_EN 0x15 #########
    regname = "MOS_SHUT_EN"; addr = 0x15
    data = setbits(
        [
            'Watchdog',
            'Cell Overvoltage',
            'External Protection',
            'Charge Overcurrent',
            'Pack Low Voltage',
        ],
        configdata["User Configuration"]["Charge Mosfet"],
        0, 
        regname)
    data |= setbits(
        [
            'Watchdog',
            'Cell Undervoltage',
            'External Protection',
            'Discharge Overcurrent',
            'Short Circuit',
        ],
        configdata["User Configuration"]["Discharge Mosfet"],
        5, 
        regname)
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    ######### ALRT_EN0 0x16 #########
    tmp = setbits(
        [
            "Charge Overcurrent",
            "Discharge Overcurrent",
            "Short Circuit",
            "Balancer Timeout",
            "Balancer Undervoltage",
            "Schedule done",
            "Timer done",
            "Watchdog",
            "External Protection",
            "PVDD Under/Overvoltage",
            "Cell Undervoltage",
            "Cell Overvoltage",
            "Pack Low Voltage",
            "reserved_0",
            "Charge Detection",
            "Load Detection",
            "SPI CRC error",
            "Cell Mismatch",
            "reserved_1",
            "reserved_2",
            "reserved_3",
            "reserved_4",
            "Die Overtemperature",
            "Die Undertemperature",
            "Pack Undervoltage",
            "Pack Overvoltage",
            "reserved_5",
            "reserved_6",
            "AUXIN Undervoltage",
            "AUXIN Overvoltage",
            "reserved_7",
            "Measurement done",
        ],
        configdata["User Configuration"]["Alert"],
        0, 
        "ALRT_EN0 & ALRT_EN1")

    regname = "ALRT_EN0"; addr=0x16
    data = tmp & 0xFFFF
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])
    ######### ALRT_EN1 0x17 #########
    regname = "ALRT_EN1"; addr=0x17
    data = (tmp >> 16) & 0xFFFF
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    ######### THR_OVCLR 0x1A #########
    regname = "THR_OVCLR"; addr=0x1a
    data = calcvalue(
        configdata["User Configuration"]["Cell Overvoltage Set/Clear [V]"][1],
        .8e-3,
        13,
        regname)
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    ######### THR_OVSET 0x1B #########
    regname = "THR_OVSET"; addr=0x1b
    data = calcvalue(
        configdata["User Configuration"]["Cell Overvoltage Set/Clear [V]"][0],
        .8e-3,
        13,
        regname)
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    ######### THR_UVCLR 0x1C #########
    regname = "THR_UVCLR"; addr=0x1c
    data = calcvalue(
        configdata["User Configuration"]["Cell Undervoltage Set/Clear [V]"][1],
        .8e-3,
        13,
        regname)
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    ######### THR_UVSET 0x1D #########
    regname = "THR_UVSET"; addr=0x1d
    data = calcvalue(
        configdata["User Configuration"]["Cell Undervoltage Set/Clear [V]"][0],
        .8e-3,
        13,
        regname)
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    ######### THR_VBLKL 0x1E #########
    regname = "THR_VBLKL"; addr=0x1e
    data = calcvalue(
        configdata["User Configuration"]["Pack Under/Overvoltage [V]"][0],
        40e-3,
        12,
        regname)
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    ######### THR_VBLKH 0x1F #########
    regname = "THR_VBLKH"; addr=0x1f
    data = calcvalue(
        configdata["User Configuration"]["Pack Under/Overvoltage [V]"][1],
        40e-3,
        12,
        regname)
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    ######### THR_MSMTCH 0x20 #########
    regname = "THR_MSMTCH"; addr=0x20
    data = calcvalue(
        configdata["User Configuration"]["Max Cell Mismatch Voltage [V]"],
        1.6e-3,
        12,
        regname)
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    ######### DLY_OVUV 0x21 #########
    regname = "DLY_OVUV"; addr=0x21
    tmp = getelement(
        [1,2,3,4,5,6,7,8],
        configdata["User Configuration"]["Cell Overvoltage/Undervoltage protection delay [cnt]"],
        regname)
    data = (tmp << 9) | (tmp << 3)
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    ######### THR_TSH 0x22 #########
    regname = "THR_TSH"; addr=0x22
    data = calcvalue(
        round((25437 - 59.17 * (configdata["User Configuration"]["Die Under/Overtemperature [degC]"][1] + 64.5)) / 16),
        1,
        12,
        regname)
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    ######### THR_TSL 0x23 #########
    regname = "THR_TSL"; addr=0x23
    data = calcvalue(
        round((25437 - 59.17 * (configdata["User Configuration"]["Die Under/Overtemperature [degC]"][0] + 64.5)) / 16),
        1,
        12,
        regname)
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    ######### THR_AUXIN1OV, THR_AUXIN1UV - THR_AUXIN4OV, THR_AUXIN4UV   0x24-0x2B #########
    for idx,val in enumerate(configdata["User Configuration"]["AUXIN Voltage Range [V]"]):
        regname = f"THR_AUXIN{idx+1}OV"
        data = calcvalue(
            val[1],
            1.6e-3,
            12,
            regname)
        if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
        userconf.append([0x24+idx*2, data])

        regname = f"THR_AUXIN{idx+1}UV"
        data = calcvalue(
            val[0],
            1.6e-3,
            12,
            regname)
        if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
        userconf.append([0x25+idx*2, data])

    ######### THR_CD 0x2C #########
    regname = "THR_CD"; addr=0x2c

    tmp = [
    configdata["User Configuration"]["Discharge Overcurrent [A]"]  * configdata["Hardware Configuration"]["Shunt Resistance"] * 1e3,
    configdata["User Configuration"]["Short Circuit Current [A]"]  * configdata["Hardware Configuration"]["Shunt Resistance"] * 1e3,
    configdata["User Configuration"]["Charge Overcurrent [A]"]     * configdata["Hardware Configuration"]["Shunt Resistance"] * 1e3
    ]
    data = (getelement(
        range(10,210,10),
        tmp[0],
        regname) - 1) << 0
    data |= (getelement(
        range(20,420,20),
        tmp[1],
        regname) - 1) << 5
    data |= (getelement(
        range(10,210,10),
        tmp[2],
        regname) - 1) << 10
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    ######### PROT_AUTO 0x2D #########
    regname = "PROT_AUTO"; addr=0x2d
    data = (1 << 2)
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    ######### PROT_CTRL 0x2E #########
    regname = "PROT_CTRL"; addr=0x2e
    data = getelement(
        [10,20,40,80,160,320,640,1280],
        configdata["User Configuration"]["Discharge overcurrent protection delay time [ms]"],
        regname) << 0
    data |= getelement(
        [0,4,8,12,16,20,24,28,32,64,128,129,256,320,384,448],
        configdata["User Configuration"]["Discharge Short protection delay time [us]"],
        regname) << 3
    data |= getelement(
        [10,20,40,80,160,320,640,1280],
        configdata["User Configuration"]["Charge overcurrent protection delay time [ms]"],
        regname) << 10
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    ######### THR_3 0x33 #########
    regname = "THR_3"; addr=0x33
    data = calcvalue(
        configdata["User Configuration"]["Cell Low Voltage Prohibit Charge [V]"] - 1.6384,
        25.6e-3,
        6,
        regname) << 4
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    ######### CELL_EN 0x36 #########
    regname = "CELL_EN"; addr=0x36
    data = 0xffff
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])
    ######### AUX_EN 0x37 #########
    regname = "AUX_EN"; addr=0x37
    data = 0x000f
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    ######### VADC_MSR 0x38 #########
    regname = "VADC_MSR"; addr=0x38
    data = getelement(
        [16,20,32,40],
        configdata["User Configuration"]["Shunt amplification fast current"],
        regname) << 11
    data |= getelement(
        [-1,-1,4096,2048,1024,512,256,128],
        configdata["User Configuration"]["VADC Conversion Speed"],
        regname) << 0
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    ######### CADC_MSR 0x39 #########
    regname = "CADC_MSR"; addr=0x39
    data = getelement(
        [0,62.5,125,250],
        configdata["User Configuration"]["CADC Period [ms]"],
        regname) << 0
    data |= (6 << 2)
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    ######### SCHEDULE 0x3A #########
    regname = "SCHEDULE"; addr=0x3a
    data = int(configdata["User Configuration"]["Scheduler"]["Enable"]) << 0
    data |= getelement(
        [0,1,2,4],
        configdata["User Configuration"]["Scheduler"]["Current Measurement Period"],
        regname) << 14
    data |= getelement(
        [0,1,2,4],
        configdata["User Configuration"]["Scheduler"]["AUXIN Measurement Period"],
        regname) << 12
    data |= getelement(
        [0,1,2,4],
        configdata["User Configuration"]["Scheduler"]["Die Temperature Measurement Period"],
        regname) << 10
    data |= getelement(
        [0,1,2,4],
        configdata["User Configuration"]["Scheduler"]["VPDD Measurement Period"],
        regname) << 8
    data |= getelement(
        [0,1,2,4,8,16,32],
        configdata["User Configuration"]["Scheduler"]["SCAN Period"],
        regname) << 1
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    ######### SLEEP_CTRL 0x3B #########
    regname = "SLEEP_CTRL"; addr=0x3b
    data = 1 << 4
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    ######### ANA_CTRL 0x3C #########
    regname = "ANA_CTRL"; addr=0x3c
    data = (1 << 3) | (1 << 2) | (1 << 1) | 1
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    ######### GPIO 0x3D #########
    regname = "GPIO"; addr=0x3d
    data = (3 << 5) | (3 << 2)
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconf.append([addr, data])

    #############################
    with open(FILENAME, 'wb') as f:
        if DEBUG>=1: print("-> ", end='')
        for addr, value in userconf:
            if DEBUG>=1: print(f"{addr:02X}:{value:04X} ", end='')
            f.write(struct.pack('<BH', addr, value))
        f.write(struct.pack('<BH', 0, 0))
        if DEBUG>=1: print()

    ###################################################
    ## GENERAL CONFIGURATION                         ##
    ###################################################
    FILENAME=f'pack{ID}_generalconf.bin'
    print("->",FILENAME)
    generalconf = PACK_GENERALCONF_t()
    generalconf.batteryNominalCapacity = configdata["Battery Configuration"]["Nominal Capacity [Ah]"]
    generalconf.batteryNominalResistance = configdata["Battery Configuration"]["Internal Resistance [Ohm]"]
    generalconf.bmsMaxCurrent = configdata["Hardware Configuration"]["Max Current [A]"]
    generalconf.bmsMaxCurrentReduced = configdata["Hardware Configuration"]["Max Current reduced [A]"]
    generalconf.balancerStartVoltage = configdata["Battery Configuration"]["Balancer Cell Startvoltage [V]"]
    generalconf.balancerDiffVoltage = configdata["Battery Configuration"]["Balancer Cell max Deltavoltage [V]"]
    generalconf.cadcCurrentFactor = 250 / (int(configdata["User Configuration"]["CADC Period [ms]"]/62.5)*8000) * 1e-3 / configdata["Hardware Configuration"]["Shunt Resistance"]
    generalconf.vadcCurrentFactor = 200e-6 / configdata["User Configuration"]["Shunt amplification fast current"] / configdata["Hardware Configuration"]["Shunt Resistance"]
    generalconf.prechargeResistorMaxI2t = configdata["Hardware Configuration"]["Precharge Resistor I2t"]
    generalconf.prechargeResistorI2tDecay = configdata["Hardware Configuration"]["Precharge Resistor I2t Decay"]

    if len(configdata["Hardware Configuration"]["NTC Polynom"]) == 11:
        for i in range(11):
            generalconf.ntcPolynom[i] = configdata["Hardware Configuration"]["NTC Polynom"][i]
    else:
        raise ValueError("NTC Polynom nicht 10. Ordnung")

    if len(configdata["Battery Configuration"]["Temperature CurrentTable [dC]"]) > 10:
        raise ValueError("Mehr als 10 Elemente in Temperature CurrentTable [dC]")
    if len(configdata["Battery Configuration"]["Max Charge CurrentTable [A]"]) > 10:
        raise ValueError("Mehr als 10 Elemente in Max Charge CurrentTable [A]")
    if len(configdata["Battery Configuration"]["Max Discharge CurrentTable [A]"]) > 10:
        raise ValueError("Mehr als 10 Elemente in Max Discharge CurrentTable [A]")
    if len(configdata["Battery Configuration"]["Temperature CurrentTable [dC]"]) != len(configdata["Battery Configuration"]["Max Charge CurrentTable [A]"]) != len(configdata["Battery Configuration"]["Max Discharge CurrentTable [A]"]):
        raise ValueError("Temperaturtabelle hat unterschiedlich lange Elemente")
    for i in range(10):
        if i >= len(configdata["Battery Configuration"]["Temperature CurrentTable [dC]"]):
            i = -1
        generalconf.currentTableTemperature[i] = configdata["Battery Configuration"]["Temperature CurrentTable [dC]"][i]
        generalconf.currentTableChargeCurrent[i] = configdata["Battery Configuration"]["Max Charge CurrentTable [A]"][i]
        generalconf.currentTableDischargeCurrent[i] = configdata["Battery Configuration"]["Max Discharge CurrentTable [A]"][i]

    if len(configdata["Battery Configuration"]["SOC OCV-Table"]) > 11:
        raise ValueError("Mehr als 11 Elemente in SOC OCV-Table")
    if len(configdata["Battery Configuration"]["Voltage OCV-Table [V]"]) > 11:
        raise ValueError("Mehr als 11 Elemente in Voltage OCV-Table")
    if len(configdata["Battery Configuration"]["SOC OCV-Table"]) != len(configdata["Battery Configuration"]["Voltage OCV-Table [V]"]):
        raise ValueError("SOC OCV-Tabelle hat unterschiedlich lange Elemente")
    for i in range(11):
        if i >= len(configdata["Battery Configuration"]["SOC OCV-Table"]):
            i = -1
        generalconf.ocvTableSOC[i] = configdata["Battery Configuration"]["SOC OCV-Table"][i]
        generalconf.ocvTableVoltage[i] = configdata["Battery Configuration"]["Voltage OCV-Table [V]"][i]

    with open(FILENAME, 'wb') as datei:
        datei.write(ctypes.string_at(ctypes.byref(generalconf), ctypes.sizeof(generalconf)))

    ###################################################
    ## CURRENT FACTORS                               ##
    ###################################################

    FILENAME=f'pack{ID}_calibration.bin'
    print("->",FILENAME)
    calibration = PACK_CALIBRATION_t()

    calibration.cadcOffset = 0
    calibration.vadcOffset = 0
    calibration.pvddOffset = 0
    calibration.tdieOffset = 0
    calibration.cadcGain = 1
    calibration.vadcGain = 1
    calibration.pvddGain = 1
    calibration.tdieGain = 1
    for i in range(4):
        calibration.ntcOffset[i] = 0
        calibration.ntcGain[i] = 0
    for i in range(16):
        calibration.cellOffset[i] = 0
        calibration.cellGain[i] = 0

    with open(FILENAME, 'wb') as datei:
        datei.write(ctypes.string_at(ctypes.byref(calibration), ctypes.sizeof(calibration)))

print("GLOBAL.json?")
FILENAME='global.bin'
print("->",FILENAME)
# GLOBAL_CONF_t Beispiel
global_conf = GLOBAL_CONF_t()
global_conf.numberOfPacks = 2
global_conf.diagWireBreakDelta = 200
global_conf.prechargeDeltaVoltage = 1
# In Datei schreiben
with open(FILENAME, "wb") as datei:
    datei.write(ctypes.string_at(ctypes.byref(global_conf), ctypes.sizeof(global_conf)))

print(f"{totalpacks} Konfiguration erstellt")

