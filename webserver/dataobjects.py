from ctypes import *

class GLOBAL_PDO_t(Structure):
    _fields_ = [
        ("numberOfPacks", c_uint32),
        ("sync", c_uint32),
        ("voltage", c_float),
    ]
class PACK_PDO_t(Structure):
    _fields_ = [
        ("id", c_uint32),
        ("stateMachine", c_uint32),
        ("aliveCounter", c_uint32),
        ("spiRetries", c_uint32),
        ("swAlertFlags", c_uint32),
        ("swWarningFlags", c_uint32),
        ("hwStatus", c_uint32),
        ("hwAlertFlags", c_uint32),
        ("hwAlertState", c_uint32),
        ("hwAlertCellUnderOvervoltage", c_uint32),
        ("hwAlertAux", c_uint32),
        ("hwBalancerTimer", c_uint32),
        ("hwBalancerStatus", c_uint32),
        ("mosfetStatus", c_uint32),
        ("prechargeResistorI2t", c_float),
        ("current", c_float),
        ("fastCurrent", c_float),
        ("cells", (c_float * 16)),
        ("ntcTemperature", (c_float * 4)),
        ("dieTemperature", c_float),
        ("voltage", c_float),
        ("pvddVoltage", c_float),
        ("availableChargeCurrent", c_float),
        ("availableDischargeCurrent", c_float),
        ("availableCapacity", c_float),
        ("totalCapacity", c_float),
        ("stateOfCharge", c_float),
        ("stateOfHealth", c_float),
        ("cycleCount", c_float),
    ]
class PACK_SDO_t(Structure):
    _fields_ = [
        ("ChargeEnable", c_uint32),
        ("DischargeEnable", c_uint32),
        ("swAlertFlagsClear", c_uint32),
        ("ResetStateMachine", c_uint32),
    ]
