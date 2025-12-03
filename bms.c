#include <stdint.h>
#include <float.h>
#include <stdio.h>
#include <syslog.h>

#include "globalconst.h"
#include "spi.h"
#include "dataobjects.h"
#include "aux.c"

#define PACK_PDO g_PackPdoData[id]
#define PACK_GENERALCONFIG g_PackGeneralConfig[id]
#define PACK_PDO_SWALERTFLAG_BITS g_PackPdoData[id].swAlertFlags_bits
#define PACK_PDO_HWALERTFLAG_BITS g_PackPdoData[id].hwAlertFlags_bits
#define PACK_PDO_HWALERTSTATE_BITS g_PackPdoData[id].hwAlertState_bits
#define PACK_SDO g_PackSdoData[id]

static uint32_t diagLock = 0;
static uint16_t diagData[NUMBER_OF_CELLS * 3];

static inline void AFESafeMode() {
    spi_AFEWriteRegister(0x13, 0); // Alle MOSFETs aus
    spi_AFEWriteRegister(0x0c, 0); // Alle Balancer aus
}

static inline void AFEDiagUnlock() {
    spi_AFEWriteRegister(0x43,0xa8);
}

static inline void AFEDiagPullUp() {
    spi_AFEWriteRegister(0x50,0xffff);
    spi_AFEWriteRegister(0x51,0x0000);
    spi_AFEWriteRegister(0x52,0x0004);
}

static inline void AFEDiagPullDown() {
    spi_AFEWriteRegister(0x50,0x0000);
    spi_AFEWriteRegister(0x51,0xffff);
    spi_AFEWriteRegister(0x52,0x0001);
}

static inline void AFEDiagClearLock() {
    spi_AFEWriteRegister(0x50,0x0000);
    spi_AFEWriteRegister(0x51,0x0000);
    spi_AFEWriteRegister(0x52,0x0000);
    spi_AFEWriteRegister(0x43,0x0);
}

static inline void AFEClearAllErrors() {
    spi_AFEWriteRegister(0x02,0xFFFF);
    spi_AFEWriteRegister(0x03,0xFFFF);
    spi_AFEWriteRegister(0x05,0xC000);
}

static inline void AFEWatchdogEnable() {
    spi_AFEWriteRegister(0x11,0x20B); //3sek
}

/**********************************************************************************************************
 * Berechne Stromlimits und I2t Wert Vorladung (UNITTEST)
 **********************************************************************************************************/
static void CalculateParametersAndLimits(int id) {
    // Mosfetabhängige Lade/Entladeströme
    float chargeCurrent = PACK_PDO.mosfetStatus_bits.CHARGE && PACK_PDO.mosfetStatus_bits.DISCHARGE
        ? PACK_GENERALCONFIG->bmsMaxCurrent             //Beide Ein, voller Strom
        : PACK_GENERALCONFIG->bmsMaxCurrentReduced;     //Einer Aus, reduzierter Strom
    float dischargeCurrent = -chargeCurrent;

    // Temperaturabhängige Lade/Entladeströme
    float temperature=floatMinVal(PACK_PDO.ntcTemperature,4);
    int tableId;
    for(tableId = GENERALCONF_CURRENTTABLE_SIZE - 1; tableId > 0; tableId--)
        if(temperature >= PACK_GENERALCONFIG->currentTableTemperature[tableId])
            break;
    if (chargeCurrent > PACK_GENERALCONFIG->currentTableChargeCurrent[tableId])
        chargeCurrent = PACK_GENERALCONFIG->currentTableChargeCurrent[tableId];
    if (dischargeCurrent < PACK_GENERALCONFIG->currentTableDischargeCurrent[tableId])
        dischargeCurrent = PACK_GENERALCONFIG->currentTableDischargeCurrent[tableId];

    PACK_PDO.availableChargeCurrent = chargeCurrent;
    PACK_PDO.availableDischargeCurrent = dischargeCurrent;

    // Energiemodell Vorladewiderstand
    if(PACK_PDO.mosfetStatus_bits.PRECHARGE && !(PACK_PDO.mosfetStatus_bits.CHARGE && PACK_PDO.mosfetStatus_bits.DISCHARGE))
        PACK_PDO.prechargeResistorI2t += PACK_PDO.current * PACK_PDO.current * (CYCLE_TIME_MS * 1e-3);
    else
        if(PACK_PDO.prechargeResistorI2t > 0) {
            PACK_PDO.prechargeResistorI2t -= PACK_GENERALCONFIG->prechargeResistorI2tDecay;
            if(PACK_PDO.prechargeResistorI2t < 0)
                PACK_PDO.prechargeResistorI2t = 0;
        }
}

static void ErrorHandler(int id) {
    // HW Teil
    PACK_PDO_SWALERTFLAG_BITS.HW_CHARGE_OC = 
            PACK_PDO_HWALERTFLAG_BITS.CHARGE_OC;
    PACK_PDO_SWALERTFLAG_BITS.HW_DISCHARGE_OC = 
            PACK_PDO_HWALERTFLAG_BITS.DISCHARGE_OC;
    PACK_PDO_SWALERTFLAG_BITS.SHORT |= 
            PACK_PDO_HWALERTFLAG_BITS.SHORT;
    PACK_PDO_SWALERTFLAG_BITS.CHIPSTATE_ERR |= 
            PACK_PDO_HWALERTFLAG_BITS.WDT_OVF |
            PACK_PDO_HWALERTFLAG_BITS.EXT_PROT |
            PACK_PDO_HWALERTSTATE_BITS.RESET |
            PACK_PDO_HWALERTSTATE_BITS.SLEEP |
            PACK_PDO_HWALERTSTATE_BITS.VREF |
            PACK_PDO_HWALERTSTATE_BITS.LVMUX |
            PACK_PDO_HWALERTSTATE_BITS.AVDD |
            PACK_PDO_HWALERTSTATE_BITS.DVDD |
            PACK_PDO_HWALERTSTATE_BITS.EEPROM_CRC_ERR |
            PACK_PDO_HWALERTSTATE_BITS.CLOCK_ABNORMAL;
    PACK_PDO_SWALERTFLAG_BITS.HW_OVERTEMP |=
            PACK_PDO_HWALERTFLAG_BITS.THERM_SD |
            PACK_PDO_HWALERTFLAG_BITS.TDIE_HI;
    PACK_PDO_SWALERTFLAG_BITS.HW_UNDERTEMP |=
            PACK_PDO_HWALERTFLAG_BITS.TDIE_LO;
    PACK_PDO_SWALERTFLAG_BITS.COMM_ERR |=
            PACK_PDO_HWALERTFLAG_BITS.SPI_CRC_ERR |
            PACK_PDO_HWALERTSTATE_BITS.SPI_CRC_ERR;
    PACK_PDO_SWALERTFLAG_BITS.DIAG_ERR |=
            PACK_PDO_HWALERTFLAG_BITS.AUX_OV |
            PACK_PDO_HWALERTFLAG_BITS.AUX_UV |
            PACK_PDO_HWALERTFLAG_BITS.LV |
            PACK_PDO_HWALERTFLAG_BITS.PVDD_UVOV;
    PACK_PDO_SWALERTFLAG_BITS.PACK_OV |=
            PACK_PDO_HWALERTFLAG_BITS.PACK_OV;
    PACK_PDO_SWALERTFLAG_BITS.PACK_UV |=
            PACK_PDO_HWALERTFLAG_BITS.PACK_UV;
    PACK_PDO_SWALERTFLAG_BITS.CELL_OV |=
            PACK_PDO_HWALERTFLAG_BITS.CELL_OV;
    PACK_PDO_SWALERTFLAG_BITS.CELL_UV |=
            PACK_PDO_HWALERTFLAG_BITS.CELL_UV;
    PACK_PDO_SWALERTFLAG_BITS.CELL_MISMATCH |=
            PACK_PDO_HWALERTFLAG_BITS.MISMATCH;

    // SW Teil
    if(PACK_PDO.current > PACK_PDO.availableChargeCurrent)
        PACK_PDO_SWALERTFLAG_BITS.SW_CHARGE_OC = 1;
    if(PACK_PDO.current < PACK_PDO.availableDischargeCurrent)
        PACK_PDO_SWALERTFLAG_BITS.SW_DISCHARGE_OC = 1;
    if(floatMaxVal(PACK_PDO.ntcTemperature,4) > PACK_GENERALCONFIG->currentTableTemperature[GENERALCONF_CURRENTTABLE_SIZE-1])
        PACK_PDO_SWALERTFLAG_BITS.PACK_OVERTEMP = 1;
    if(floatMinVal(PACK_PDO.ntcTemperature,4) < PACK_GENERALCONFIG->currentTableTemperature[0])
        PACK_PDO_SWALERTFLAG_BITS.PACK_UNDERTEMP = 1;
    if(PACK_PDO.prechargeResistorI2t > PACK_GENERALCONFIG->prechargeResistorMaxI2t)
        PACK_PDO_SWALERTFLAG_BITS.PRECHARGE_FAIL = 1;
}

/**********************************************************************************************************
 * Setzt Lade Entlade und Vorlade Mosfets (UNITTEST)
 **********************************************************************************************************/
static void MosControl(int id) {
    uint16_t mosVal = 0;
    uint8_t errorAll =
        PACK_PDO_SWALERTFLAG_BITS.SHORT ||
        PACK_PDO_SWALERTFLAG_BITS.CHIPSTATE_ERR ||
        PACK_PDO_SWALERTFLAG_BITS.HW_OVERTEMP ||
        PACK_PDO_SWALERTFLAG_BITS.HW_UNDERTEMP ||
        PACK_PDO_SWALERTFLAG_BITS.PACK_OVERTEMP ||
        PACK_PDO_SWALERTFLAG_BITS.PACK_UNDERTEMP ||
        PACK_PDO_SWALERTFLAG_BITS.TEMP_MISMATCH ||
        PACK_PDO_SWALERTFLAG_BITS.COMM_ERR ||
        PACK_PDO_SWALERTFLAG_BITS.DIAG_ERR ||
        PACK_PDO_SWALERTFLAG_BITS.CELL_MISMATCH ||
        PACK_PDO_SWALERTFLAG_BITS.PRECHARGE_FAIL ||
        PACK_PDO_SWALERTFLAG_BITS.CURRENT_ABNORMAL;
    uint8_t errorCharge =
        PACK_PDO_SWALERTFLAG_BITS.HW_CHARGE_OC ||
        PACK_PDO_SWALERTFLAG_BITS.SW_CHARGE_OC ||
        PACK_PDO_SWALERTFLAG_BITS.PACK_OV ||
        PACK_PDO_SWALERTFLAG_BITS.CELL_OV;
    uint8_t errorDischarge =
        PACK_PDO_SWALERTFLAG_BITS.HW_DISCHARGE_OC ||
        PACK_PDO_SWALERTFLAG_BITS.SW_DISCHARGE_OC ||
        PACK_PDO_SWALERTFLAG_BITS.PACK_UV ||
        PACK_PDO_SWALERTFLAG_BITS.CELL_UV;
    uint8_t allowCharge    = PACK_SDO.ChargeEnable && !(errorAll || errorCharge);
    uint8_t allowDischarge = PACK_SDO.DischargeEnable && !(errorAll || errorDischarge);

    if(PACK_PDO.mosfetStatus_bits.PRECHARGE && (__builtin_fabsf(PACK_PDO.voltage - g_GlobalPdoData->voltage) <= g_GlobalConfig.prechargeDeltaVoltage)) {
        if (allowDischarge) mosVal |= (1 << 0);
        if (allowCharge)    mosVal |= (1 << 1);
    }

    if (allowDischarge)
        mosVal |= (PACK_PDO.mosfetStatus_bits.DISCHARGE ? (1 << 0) : (1 << 2));
    if (allowCharge)
        mosVal |= (PACK_PDO.mosfetStatus_bits.CHARGE ? (1 << 1) : (1 << 2));

    spi_AFEWriteRegister(0x13, mosVal);
}

static void AFEWriteUser(int id) {
    spi_AFEWriteRegister(0x45,0x95); // USER Unlock
    uint32_t i=0;
    while (g_PackUserConfig[id][i].address > 0) {
        spi_AFEWriteRegister(
            g_PackUserConfig[id][i].address,
            g_PackUserConfig[id][i].data);
        i++;
    }
    spi_AFEWriteRegister(0x45,0x00); // USER Lock
}

static uint32_t AFEVerifyUser(int id) {
    uint32_t i=0;
    while (g_PackUserConfig[id][i].address > 0) {
        uint32_t start = i;
        uint32_t len = 1;

        // Suche, wie viele aufeinanderfolgende Register wir haben (max. 32)
        while (g_PackUserConfig[id][start + len].address ==
               g_PackUserConfig[id][start + len - 1].address + 1 &&
               len < 32 &&
               g_PackUserConfig[id][start + len].address > 0)
            len++;

        // Lese den ganzen Block auf einmal
        uint16_t readback[32]; // max Blockgröße
        spi_AFEReadRegister(
                g_PackUserConfig[id][start].address,
                readback,
                len
            );

        // Vergleiche jedes Register im Block
        for (uint32_t j = 0; j < len; j++)
            if (g_PackUserConfig[id][start + j].data != readback[j])
                return 1;

        i += len; // Nächster Block
    }
    return 0;
}

static uint32_t AFEInit(int id) {
    AFEWriteUser(id);
    if(AFEVerifyUser(id))
        return 1;
    spi_AFEWriteRegister(0x05,0x4000); //Clear RESET Flag
    spi_AFEWriteRegister(0x0d,31); //Setup Balancer
    return 0;
}

static void AFEReadData(int id) {
    uint16_t data[28];

    spi_AFEReadRegister(0x01, data, 16);
    PACK_PDO.hwStatus = data[0];
    PACK_PDO.hwAlertFlags = (data[2] << 16) | data[1];
    PACK_PDO.hwAlertState = (data[5] << 16) | data[4];
    PACK_PDO.hwAlertCellUnderOvervoltage = (data[7] << 16) | data[6];
    PACK_PDO.hwAlertAux = data[9];
    PACK_PDO.hwBalancerTimer = data[14];
    PACK_PDO.hwBalancerStatus = data[15];

    spi_AFEReadRegister(0x84, data, 28);
    PACK_PDO.current = (float)((int16_t)data[0]) * PACK_GENERALCONFIG->cadcCurrentFactor;
    PACK_PDO.voltage = (float)data[1] * 1.6e-3;
    PACK_PDO.pvddVoltage = (float)data[2] * 2.5e-3;
    for(int i=0; i < NUMBER_OF_CELLS; i++)
        PACK_PDO.cells[i] = (float)data[3 + i] * 100e-6;
    for(int i=0; i < 4; i++)
        PACK_PDO.ntcTemperature[i] = NtcToTemperature((float)data[20 + i], PACK_GENERALCONFIG->ntcPolynom, 11);
    PACK_PDO.dieTemperature = (float)(25437 - data[26]) / 59.17 - 64.5;
    PACK_PDO.fastCurrent = (float)(data[27] & 0x7fff) * PACK_GENERALCONFIG->vadcCurrentFactor;
    if (data[27] & 0x8000)
        PACK_PDO.fastCurrent = -PACK_PDO.fastCurrent;
}

static uint32_t AFEWireDiag(uint16_t *data)
{
    for(int i=NUMBER_OF_CELLS; i<=32;i+=NUMBER_OF_CELLS)
        for(int j=0; j<NUMBER_OF_CELLS;j++)
        {
            int32_t delta;
            delta = data[j] - data[i+j];
            if(delta < 0)
                delta = -delta;
            if((delta >= g_GlobalConfig.diagWireBreakDelta))
                return 1; //Kabelbruch erkannt
        }
    return 0;
}

static uint32_t AFEBalancer (int id) {
    float avg_vcell=floatAvgVal(PACK_PDO.cells,NUMBER_OF_CELLS);
    
    uint32_t new_balance = 0;
    for(int i=0; i<NUMBER_OF_CELLS; i++)
        if((PACK_PDO.cells[i] - avg_vcell) > PACK_GENERALCONFIG->balancerDiffVoltage)
            new_balance |= (1 << i);

    spi_AFEWriteRegister(0x0c,new_balance);
    spi_AFEWriteRegister(0x0f,40); //40 * 0,25 -> 10sek

    return (new_balance>0);
}

static uint32_t AFECheckPowerupComplete() {
    uint16_t data;
    spi_AFEReadRegister(0x00, &data, 1);
    return data == 0x6000; /* TOP_STATUS muss auf Power-up Complete sein */
}


void bms_CyclicTask(uint32_t id) {
    switch(PACK_PDO.stateMachine)
    {
        case AFE_STATE_WAIT_INIT:
            /*********************************************
             * Überprüfe, ob AFE erkannt wurde
             * JA   -> Starte Initialisierung
             * NEIN -> Warte
             *********************************************/
            if (AFECheckPowerupComplete()) { 
                syslog(LOG_INFO, "PACK%u: PB7170 gefunden, initialisiere...", PACK_PDO.id);
                AFESafeMode();
                PACK_PDO.stateMachine = AFE_STATE_INIT;
            }
            break;
        case AFE_STATE_INIT:
            /*********************************************
             * Lade Userconfig für das AFE
             * OK   -> Starte Diagnose
             * NEIN -> Fehler
             *********************************************/
            if (AFEInit(id) == 0) {
                syslog(LOG_INFO, "PACK%u: Userconfig erfolgreich geschrieben", PACK_PDO.id);
                PACK_PDO.stateMachine = AFE_STATE_WAIT_DIAG0;
            } else {
                syslog(LOG_INFO, "PACK%u: Fehler beim Schreiben der Userconfig", PACK_PDO.id);
                PACK_PDO.stateMachine = AFE_STATE_ERROR;
                break;
            }
            break;
        case AFE_STATE_WAIT_DIAG0:
            /*********************************************
             * Kabelbruchdiagnose gestartet, warte bis
             * Werte für Zellen bereit sind, bzw.
             * Diagnose ist nur eine gleichzeitig erlaubt
             * OK   -> Ein Zyklus warten
             * NEIN -> Warten
             *********************************************/
            if(diagLock == 0) {
                diagLock = 1;
                AFEDiagUnlock();
                PACK_PDO.stateMachine = AFE_STATE_DIAG0;
            }
            break;
        case AFE_STATE_DIAG0:
            /*********************************************
             * Merke Werte für Standard
             * Setze alle Zellen auf Diagnose-Pullup
             *********************************************/
            spi_AFEReadRegister(0x87,&diagData[0],NUMBER_OF_CELLS); //Werte Standard
            AFEDiagPullUp();
            PACK_PDO.stateMachine = AFE_STATE_WAIT_DIAG1;
            break;
        case AFE_STATE_WAIT_DIAG1:
            PACK_PDO.stateMachine = AFE_STATE_DIAG1;
            break;
        case AFE_STATE_DIAG1:
            /*********************************************
             * Merke Werte für Pullup
             * Setze alle Zellen auf Diagnose-Pulldown
             *********************************************/
            spi_AFEReadRegister(0x87,&diagData[NUMBER_OF_CELLS],NUMBER_OF_CELLS); //Werte Pullup
            AFEDiagPullDown();
            PACK_PDO.stateMachine = AFE_STATE_WAIT_DIAG2;
            break;
        case AFE_STATE_WAIT_DIAG2:
            PACK_PDO.stateMachine = AFE_STATE_DIAG2;
            break;
        case AFE_STATE_DIAG2:
            /*********************************************
             * Merke Werte für Pulldown
             * Setze alle Zellen auf Standard
             * Kabelbrucherkennung
             *********************************************/
            spi_AFEReadRegister(0x87,&diagData[2 * NUMBER_OF_CELLS],NUMBER_OF_CELLS); //Werte Pulldown
            AFEDiagClearLock();
            diagLock = 0;

            if(AFEWireDiag(diagData)) {
                syslog(LOG_ALERT, "PACK%u: Diagnose fehlerhaft, deaktiviere Pack", PACK_PDO.id);
                PACK_PDO_SWALERTFLAG_BITS.DIAG_ERR = 1;
                PACK_PDO.stateMachine = AFE_STATE_ERROR;
            } else {
                syslog(LOG_INFO, "PACK%u: Diagnose erfolgreich", PACK_PDO.id);
                PACK_PDO.stateMachine = AFE_STATE_CONFIG;
            }
            break;
        case AFE_STATE_CONFIG:
            /*********************************************
             * Alle Fehler löschen
             * Watchdog initialisieren
             *********************************************/
            AFEClearAllErrors();
            AFEWatchdogEnable();
            syslog(LOG_INFO, "PACK%u: Konfiguration fertig, RUN-Mode\n", PACK_PDO.id);
            
            PACK_PDO.stateMachine = AFE_STATE_RUN;
            break;
        case AFE_STATE_SANITY_CHECK:
            /*********************************************
             * Register prüfen
             *********************************************/
            if(!AFECheckPowerupComplete() && AFEVerifyUser(id)) {
                syslog(LOG_ALERT, "PACK%u: Sanity-Check fehlerhaft, deaktiviere Pack\n", PACK_PDO.id);
                PACK_PDO_SWALERTFLAG_BITS.CHIPSTATE_ERR = 1;
                PACK_PDO.stateMachine = AFE_STATE_ERROR;
            } else {
                PACK_PDO.stateMachine = AFE_STATE_RUN;
            }
        case AFE_STATE_RUN_WARNING:
        case AFE_STATE_RUN:
            /*********************************************
             * Daten lesen
             * Parameter und Limits berechnen
             * Fehlerhandling
             * Mosfets steuern
             * SOC/SOH berechnen
             * Zyklisch einen Sanity Check durchführen
             *********************************************/
            AFEReadData(id);
            CalculateParametersAndLimits(id);
            ErrorHandler(id);
            MosControl(id);
            if(PACK_PDO.hwBalancerTimer == 0)
                for (int i=0; i<NUMBER_OF_CELLS; i++)
                    if(PACK_PDO.cells[i] >= PACK_GENERALCONFIG->balancerStartVoltage) {
                        AFEBalancer(id);
                        break;
                    }
                
            break;
        case AFE_STATE_ERROR:
            break;
        default:
            break;
    }
}