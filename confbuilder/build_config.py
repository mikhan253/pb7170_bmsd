#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import json
import array
import math
import struct

DEBUG=2

ID = 0


# Load userconf.json
with open(f'pack{ID}.json', 'r') as file:
    configdata = json.load(file)



###################################################
## USER CONFIG 0x14-0x3D                         ##
###################################################
FILENAME=f'pack{ID}_userconf.bin'
print(FILENAME)

userconfig_addresses = array.array('B', [
    0x14, 0x15, 0x16, 0x17, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
    0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x33, 0x36, 0x37, 0x38, 0x39,
    0x3A, 0x3B, 0x3C, 0x3D
])
userconfig_blob = array.array('H', [0] * len(userconfig_addresses))

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
regname = "MOS_CTRL"
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
userconfig_blob[0] = data
######### MOS_SHUT_EN 0x15 #########
regname = "MOS_SHUT_EN"
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
userconfig_blob[1] = data

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

regname = "ALRT_EN0"
data = tmp & 0xFFFF
if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
userconfig_blob[2] = data
######### ALRT_EN1 0x17 #########
regname = "ALRT_EN1"
data = (tmp >> 16) & 0xFFFF
if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
userconfig_blob[3] = data

######### THR_OVCLR 0x1A #########
regname = "THR_OVCLR"
data = calcvalue(
    configdata["User Configuration"]["Cell Overvoltage Set/Clear [V]"][1],
    .8e-3,
    13,
    regname)
if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
userconfig_blob[4] = data

######### THR_OVSET 0x1B #########
regname = "THR_OVSET"
data = calcvalue(
    configdata["User Configuration"]["Cell Overvoltage Set/Clear [V]"][0],
    .8e-3,
    13,
    regname)
if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
userconfig_blob[5] = data

######### THR_UVCLR 0x1C #########
regname = "THR_UVCLR"
data = calcvalue(
    configdata["User Configuration"]["Cell Undervoltage Set/Clear [V]"][1],
    .8e-3,
    13,
    regname)
if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
userconfig_blob[6] = data

######### THR_UVSET 0x1D #########
regname = "THR_UVSET"
data = calcvalue(
    configdata["User Configuration"]["Cell Undervoltage Set/Clear [V]"][0],
    .8e-3,
    13,
    regname)
if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
userconfig_blob[7] = data

######### THR_VBLKL 0x1E #########
regname = "THR_VBLKL"
data = calcvalue(
    configdata["User Configuration"]["Pack Under/Overvoltage [V]"][0],
    40e-3,
    12,
    regname)
if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
userconfig_blob[8] = data

######### THR_VBLKH 0x1F #########
regname = "THR_VBLKH"
data = calcvalue(
    configdata["User Configuration"]["Pack Under/Overvoltage [V]"][1],
    40e-3,
    12,
    regname)
if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
userconfig_blob[9] = data

######### THR_MSMTCH 0x20 #########
regname = "THR_MSMTCH"
data = calcvalue(
    configdata["User Configuration"]["Max Cell Mismatch Voltage [V]"],
    1.6e-3,
    12,
    regname)
if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
userconfig_blob[10] = data

######### DLY_OVUV 0x21 #########
regname = "DLY_OVUV"
tmp = getelement(
    [1,2,3,4,5,6,7,8],
    configdata["User Configuration"]["Cell Overvoltage/Undervoltage protection delay [cnt]"],
    regname)
data = (tmp << 9) | (tmp << 3)
if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
userconfig_blob[11] = data

######### THR_TSH 0x22 #########
regname = "THR_TSH"
data = calcvalue(
    round((25437 - 59.17 * (configdata["User Configuration"]["Die Under/Overtemperature [degC]"][1] + 64.5)) / 16),
    1,
    12,
    regname)
if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
userconfig_blob[12] = data

######### THR_TSL 0x23 #########
regname = "THR_TSL"
data = calcvalue(
    round((25437 - 59.17 * (configdata["User Configuration"]["Die Under/Overtemperature [degC]"][0] + 64.5)) / 16),
    1,
    12,
    regname)
if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
userconfig_blob[13] = data

######### THR_AUXIN1OV, THR_AUXIN1UV - THR_AUXIN4OV, THR_AUXIN4UV   0x25-0x2B #########
for idx,val in enumerate(configdata["User Configuration"]["AUXIN Voltage Range [V]"]):
    regname = f"THR_AUXIN{idx+1}OV"
    data = calcvalue(
        val[1],
        1.6e-3,
        12,
        regname)
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconfig_blob[14+idx*2] = data

    regname = f"THR_AUXIN{idx+1}UV"
    data = calcvalue(
        val[0],
        1.6e-3,
        12,
        regname)
    if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
    userconfig_blob[15+idx*2] = data

######### THR_CD 0x2C #########
regname = "THR_CD"

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
userconfig_blob[22] = data

######### PROT_AUTO 0x2D #########
regname = "PROT_AUTO"
data = (1 << 2)
if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
userconfig_blob[23] = data

######### PROT_CTRL 0x2E #########
regname = "PROT_CTRL"
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
userconfig_blob[24] = data

######### THR_3 0x33 #########
regname = "THR_3"
data = calcvalue(
    configdata["User Configuration"]["Cell Low Voltage Prohibit Charge [V]"] - 1.6384,
    25.6e-3,
    6,
    regname) << 4
if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
userconfig_blob[25] = data

######### CELL_EN 0x36 #########
regname = "CELL_EN"
data = 0xffff
if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
userconfig_blob[26] = data
######### AUX_EN 0x37 #########
regname = "AUX_EN"
data = 0x000f
if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
userconfig_blob[27] = data

######### VADC_MSR 0x38 #########
regname = "VADC_MSR"
data = getelement(
    [16,20,32,40],
    configdata["User Configuration"]["Shunt amplification fast current"],
    regname) << 11
data |= getelement(
    [-1,-1,4096,2048,1024,512,256,128],
    configdata["User Configuration"]["VADC Conversion Speed"],
    regname) << 0
if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
userconfig_blob[28] = data

######### CADC_MSR 0x39 #########
regname = "CADC_MSR"
data = getelement(
    [0,62.5,125,250],
    configdata["User Configuration"]["CADC Period [ms]"],
    regname) << 0
data |= (6 << 2)
if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
userconfig_blob[29] = data

######### SCHEDULE 0x3A #########
regname = "SCHEDULE"
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
userconfig_blob[30] = data

######### SLEEP_CTRL 0x3B #########
regname = "SLEEP_CTRL"
data = 1 << 4
if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
userconfig_blob[31] = data

######### ANA_CTRL 0x3C #########
regname = "ANA_CTRL"
data = (1 << 3) | (1 << 2) | (1 << 1) | 1
if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
userconfig_blob[32] = data

######### GPIO 0x3D #########
regname = "GPIO"
data = (3 << 5) | (3 << 2)
if DEBUG>=2: print(regname.ljust(18),format(data, '016b'))
userconfig_blob[33] = data


#print("->", ' '.join(format(x, '#06x') for x in userconfig_blob))
#with open(FILENAME, 'wb') as datei:
#    userconfig_blob.tofile(datei)

with open(FILENAME, 'wb') as f:
    print("-> ", end='')
    for addr, value in zip(userconfig_addresses, userconfig_blob):
        print(f"{addr:02X}:{value:04X} ", end='')
        f.write(struct.pack('<BH', addr, value))
    print()
###################################################
## NTC POLYNOM                                   ##
###################################################
FILENAME=f'pack{ID}_ntcpolynom.bin'
print(FILENAME)

ntcpolynom_blob = array.array('f',configdata["Hardware Configuration"]["NTC Polynom"])

print("->", ' '.join(f"{x:.3e}" for x in ntcpolynom_blob))
with open(FILENAME, 'wb') as datei:
    ntcpolynom_blob.tofile(datei)

###################################################
## CURRENT FACTORS                               ##
###################################################

FILENAME=f'pack{ID}_currentfactors.bin'
print(FILENAME)
currentfactors_blob = array.array('f', [
    #CADC
    250 / (int(configdata["User Configuration"]["CADC Period [ms]"]/62.5)*8000) * 1e-3 / configdata["Hardware Configuration"]["Shunt Resistance"] ,
    #VADC Strom
    200e-6 / configdata["User Configuration"]["Shunt amplification fast current"] / configdata["Hardware Configuration"]["Shunt Resistance"]
    ])

print("->", ' '.join(f"{x:.3e}" for x in currentfactors_blob))
with open(FILENAME, 'wb') as datei:
    currentfactors_blob.tofile(datei)

###################################################
## BALANCER SETUP                                ##
###################################################

FILENAME=f'pack{ID}_balancer.bin'
print(FILENAME)
balancer_blob = array.array('f', [
    configdata["Battery Configuration"]["Balancer Cell Startvoltage [V]"],
    configdata["Battery Configuration"]["Balancer Cell max Deltavoltage [V]"],
    ])

print("->", ' '.join(f"{x:.3e}" for x in balancer_blob))
with open(FILENAME, 'wb') as datei:
    balancer_blob.tofile(datei)