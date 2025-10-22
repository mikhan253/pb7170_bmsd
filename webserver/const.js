// ---------- Config / Texts ----------
const PACKNAMES_TEXT = [
    "EVE MB31 314Ah",
    "EVE MB31 314Ah",
] 

const STATEMACHINE_TEXT = [
    "WAIT_INIT",
    "INIT",
    "CONFIG",
    "WAIT_DIAG0",
    "DIAG0",
    "WAIT_DIAG1",
    "DIAG1",
    "WAIT_DIAG2",
    "DIAG2",
    "SANITY_CHECK",
    "RUN_WARN",
    "RUN",
    "ERROR",
    "DISABLED",
    "COUNT"
]

const SWALERTFLAGS_TEXT = [
    "HW Überstrom Laden",           //HW_CHARGE_OC
    "HW Überstrom Entladen",        //HW_DISCHARGE_OC
    "SW Überstrom Laden",           //SW_CHARGE_OC
    "SW Überstrom Entladen",        //SW_DISCHARGE_OC
    "Kurzschluss",                  //SHORT
    "Chipstatus",                   //CHIPSTATE_ERR
    "HW Übertemperatur",            //HW_OVERTEMP
    "HW Untertemperatur",           //HW_UNDERTEMP
    "Pack Übertemperatur",          //PACK_OVERTEMP
    "Pack Untertemperatur",         //PACK_UNDERTEMP
    "Temperaturdifferenz",          //TEMP_MISMATCH
    "Kommunikation",                //COMM_ERR
    "Diagnose",                     //DIAG_ERR
    "Pack Überspannung",            //PACK_OV
    "Pack Unterspannung",           //PACK_UV
    "Zelle Überspannung",           //CELL_OV
    "Zelle Unterspannung",          //CELL_UV
    "Zelle Differenzspannung",      //CELL_MISMATCH
    "Vorladefehler",                //PRECHARGE_FAIL
    "Stromwert abnormal",           //CURRENT_ABNORMAL
];
const MOS_BITS = ["Laden","Entladen","Vorladen"];
