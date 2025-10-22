from ctypes import *

class GLOBAL_CONF_t(Structure):
    _fields_ = [
        ("numberOfPacks", c_uint32),
        ("diagWireBreakDelta", c_uint32),
        ("prechargeDeltaVoltage", c_float),
    ]
class PACK_USERCONF_t(Structure):
    _pack_ = 1
    _fields_ = [
        ("address", c_uint8),
        ("data", c_uint16),
    ]
class PACK_GENERALCONF_t(Structure):
    _fields_ = [
        ("batteryNominalCapacity", c_float),
        ("batteryNominalResistance", c_float),
        ("bmsMaxCurrent", c_float),
        ("bmsMaxCurrentReduced", c_float),
        ("balancerStartVoltage", c_float),
        ("balancerDiffVoltage", c_float),
        ("cadcCurrentFactor", c_float),
        ("vadcCurrentFactor", c_float),
        ("prechargeResistorMaxI2t", c_float),
        ("prechargeResistorI2tDecay", c_float),
        ("ntcPolynom", (c_float * 11)),
        ("currentTableTemperature", (c_float * 10)),
        ("currentTableChargeCurrent", (c_float * 10)),
        ("currentTableDischargeCurrent", (c_float * 10)),
        ("ocvTableSOC", (c_float * 11)),
        ("ocvTableVoltage", (c_float * 11)),
    ]
class PACK_CALIBRATION_t(Structure):
    _fields_ = [
        ("cadcOffset", c_float),
        ("vadcOffset", c_float),
        ("ntcOffset", (c_float * 4)),
        ("cellOffset", (c_float * 16)),
        ("pvddOffset", c_float),
        ("tdieOffset", c_float),
        ("cadcGain", c_float),
        ("vadcGain", c_float),
        ("ntcGain", (c_float * 4)),
        ("cellGain", (c_float * 16)),
        ("pvddGain", c_float),
        ("tdieGain", c_float),
    ]
