#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <float.h>

#include "bms.h"
#include "dataobjects.h"

GLOBAL_PDO_t GlobalPdoData;
PACK_PDO_t PackPdoData[MAX_BATTERY_PACKS];
PACK_SDO_t PackSdoData[MAX_BATTERY_PACKS];
PACK_USERCONF_t PackUserConfig[MAX_BATTERY_PACKS];
PACK_GENERALCONF_t PackGeneralConfig[MAX_BATTERY_PACKS];
PACK_CALIBRATION_t PackCalibration[MAX_BATTERY_PACKS];

GLOBAL_CONF_t g_GlobalConfig;
GLOBAL_PDO_t* g_GlobalPdoData = NULL;
PACK_PDO_t* g_PackPdoData = NULL;
PACK_SDO_t* g_PackSdoData = NULL;
PACK_USERCONF_t* g_PackUserConfig[MAX_BATTERY_PACKS];
PACK_GENERALCONF_t* g_PackGeneralConfig[MAX_BATTERY_PACKS];
PACK_CALIBRATION_t* g_PackCalibration[MAX_BATTERY_PACKS];

uint16_t SpiReg[0xff];

int spi_SelectDevice(uint_fast8_t device) {
    return 0;
};
int spi_Init(const char *spiDevice, uint32_t speed, uint8_t mode, uint8_t bits, const char *gpioDevice, const unsigned int *gpioPins, const unsigned int gpioNrPins) {
    return 0;
};
int spi_AFEReadRegister(uint8_t addr, uint16_t* output, uint_fast8_t count) {
    *output = SpiReg[addr];
    return 0;
};
int spi_AFEWriteRegister(uint8_t addr, uint16_t data) {
    SpiReg[addr] = data;
    return 0;
};


#include "bms.c"

int main() {
    uint32_t id=0;
    uint32_t errors=0;
    printf("--- UNIT FEST für BMS.C ---\n");
    g_GlobalPdoData = &GlobalPdoData;
    g_PackPdoData = PackPdoData;
    g_PackSdoData = PackSdoData;
    g_PackGeneralConfig[id] = &PackGeneralConfig[id];

#define SET_NTC(x) for(int i=0;i<4;i++) PACK_PDO.ntcTemperature[i]=x;
#define CT_TEMP 
/*********************************************************************************************/
printf("CalculateParametersAndLimits\n");
    PACK_GENERALCONFIG->currentTableTemperature[0] = -30;
    PACK_GENERALCONFIG->currentTableTemperature[1] = -20;
    PACK_GENERALCONFIG->currentTableTemperature[2] = -10;
    PACK_GENERALCONFIG->currentTableTemperature[3] = 0;
    PACK_GENERALCONFIG->currentTableTemperature[4] = 5;
    PACK_GENERALCONFIG->currentTableTemperature[5] = 10;
    PACK_GENERALCONFIG->currentTableTemperature[6] = 15;
    PACK_GENERALCONFIG->currentTableChargeCurrent[0] = 0;
    PACK_GENERALCONFIG->currentTableChargeCurrent[1] = 0;
    PACK_GENERALCONFIG->currentTableChargeCurrent[2] = 0;
    PACK_GENERALCONFIG->currentTableChargeCurrent[3] = 15.7;
    PACK_GENERALCONFIG->currentTableChargeCurrent[4] = 37.6;
    PACK_GENERALCONFIG->currentTableChargeCurrent[5] = 94.2;
    PACK_GENERALCONFIG->currentTableChargeCurrent[6] = 157;
    PACK_GENERALCONFIG->currentTableDischargeCurrent[0] = 0;
    PACK_GENERALCONFIG->currentTableDischargeCurrent[1] = -157;
    PACK_GENERALCONFIG->currentTableDischargeCurrent[2] = -157;
    PACK_GENERALCONFIG->currentTableDischargeCurrent[3] = -157;
    PACK_GENERALCONFIG->currentTableDischargeCurrent[4] = -157;
    PACK_GENERALCONFIG->currentTableDischargeCurrent[5] = -157;
    PACK_GENERALCONFIG->currentTableDischargeCurrent[6] = -157;
    for(int i=7;i<10;i++) {
        PACK_GENERALCONFIG->currentTableTemperature[i] = 60;
        PACK_GENERALCONFIG->currentTableChargeCurrent[i] = 0;
        PACK_GENERALCONFIG->currentTableDischargeCurrent[i] = 0;
    }
    PACK_PDO.mosfetStatus_bits.CHARGE = 1;
    PACK_PDO.mosfetStatus_bits.DISCHARGE = 1;
    PACK_GENERALCONFIG->bmsMaxCurrent = 200;
    PACK_GENERALCONFIG->bmsMaxCurrentReduced = 50;
    PACK_GENERALCONFIG->prechargeResistorI2tDecay=0.1;
    PACK_PDO.current = 0;

#define TESTCASE(nr, set1, expect1, expect2) \
        SET_NTC(set1); \
        CalculateParametersAndLimits(id); \
        if( (PACK_PDO.availableChargeCurrent != expect1) || (PACK_PDO.availableDischargeCurrent != expect2) ) { \
            printf("   TC%02u FAIL: ntcTemperature=%f\n              availableChargeCurrent=%f (expect %f)\n              availableDischargeCurrent=%f (expect %f)\n",nr, PACK_PDO.ntcTemperature[0], PACK_PDO.availableChargeCurrent, expect1, PACK_PDO.availableDischargeCurrent, expect2); \
            errors++; \
        }

    printf(" * Temperaturtabelle prüfen, Mosfets ein\n");
    TESTCASE( 1,  -40.0f ,     0.0f ,       0.0f)
    TESTCASE( 2,  -30.0f ,     0.0f ,       0.0f)
    TESTCASE( 3,  -20.1f ,     0.0f ,       0.0f)
    TESTCASE( 4,  -20.0f ,     0.0f ,    -157.0f)
    TESTCASE( 5,  -19.9f ,     0.0f ,    -157.0f)
    TESTCASE( 6,    0.0f ,    15.7f ,    -157.0f)
    TESTCASE( 7,   30.0f ,   157.0f ,    -157.0f)
    TESTCASE( 8,   59.9f ,   157.0f ,    -157.0f)
    TESTCASE( 9,   60.0f ,     0.0f ,       0.0f)
    TESTCASE(10,   90.0f ,     0.0f ,       0.0f)
    printf(" * Temperaturtabelle prüfen, Mosfets ein, BMS Limit\n");
    PACK_GENERALCONFIG->bmsMaxCurrent = 125;
    TESTCASE( 1,  -40.0f ,     0.0f ,       0.0f)
    TESTCASE( 2,  -19.9f ,     0.0f ,    -125.0f)
    TESTCASE( 3,    0.0f ,    15.7f ,    -125.0f)
    TESTCASE( 4,   59.9f ,   125.0f ,    -125.0f)
    TESTCASE( 5,   60.0f ,     0.0f ,       0.0f)
    printf(" * Temperaturtabelle prüfen, Mosfets aus\n");
    PACK_PDO.mosfetStatus_bits.CHARGE = 1;
    PACK_PDO.mosfetStatus_bits.DISCHARGE = 0;
    TESTCASE( 1,  -40.0f ,     0.0f ,       0.0f)
    TESTCASE( 2,  -19.9f ,     0.0f ,     -50.0f)
    TESTCASE( 3,    0.0f ,    15.7f ,     -50.0f)
    TESTCASE( 4,   59.9f ,    50.0f ,     -50.0f)
    TESTCASE( 5,   60.0f ,     0.0f ,       0.0f)
    PACK_PDO.mosfetStatus_bits.CHARGE = 0;
    PACK_PDO.mosfetStatus_bits.DISCHARGE = 1;
    TESTCASE( 6,  -40.0f ,     0.0f ,       0.0f)
    TESTCASE( 7,  -19.9f ,     0.0f ,     -50.0f)
    TESTCASE( 8,    0.0f ,    15.7f ,     -50.0f)
    TESTCASE( 9,   59.9f ,    50.0f ,     -50.0f)
    TESTCASE(10,   60.0f ,     0.0f ,       0.0f)
    PACK_PDO.mosfetStatus_bits.CHARGE = 0;
    PACK_PDO.mosfetStatus_bits.DISCHARGE = 0;
    TESTCASE(11,  -40.0f ,     0.0f ,       0.0f)
    TESTCASE(12,  -19.9f ,     0.0f ,     -50.0f)
    TESTCASE(13,    0.0f ,    15.7f ,     -50.0f)
    TESTCASE(14,   59.9f ,    50.0f ,     -50.0f)
    TESTCASE(15,   60.0f ,     0.0f ,       0.0f)
#undef TESTCASE

    PACK_GENERALCONFIG->prechargeResistorI2tDecay=0.1f;
    PACK_PDO.current = 0;
#define TESTCASE(nr, set1, expect1) \
        PACK_PDO.current = set1; \
        CalculateParametersAndLimits(id); \
        if( (PACK_PDO.prechargeResistorI2t != expect1) ) { \
            printf("   TC%02u FAIL: current=%f\n              prechargeResistorI2t=%f (expect %f)\n",nr, PACK_PDO.current, PACK_PDO.prechargeResistorI2t, expect1); \
            errors++; \
        }
    printf(" * I2t Vorladewiderstand, Precharge nicht ein\n");
    TESTCASE( 1,    0.0f ,     0.0f)
    TESTCASE( 2,   10.0f ,     0.0f)
    TESTCASE( 3,  -10.0f ,     0.0f)
    printf(" * I2t Vorladewiderstand, Precharge ein\n");
    PACK_PDO.mosfetStatus_bits.PRECHARGE = 1;
    TESTCASE( 1,    0.0f ,     0.0f)
    TESTCASE( 2,   10.0f ,     25.200001f)
    TESTCASE( 3,  -10.0f ,     50.400002f)
    TESTCASE( 4,    0.0f ,     50.400002f)
    printf(" * I2t Vorladewiderstand, Precharge aus\n");
    PACK_PDO.mosfetStatus_bits.PRECHARGE = 0;
    TESTCASE( 1,    0.0f ,     50.300003f)
    TESTCASE( 2,    0.0f ,     50.200005f)
    TESTCASE( 3,    0.0f ,     50.100006f)
    printf(" * I2t Vorladewiderstand, Alle Mosfets ein\n");
    PACK_PDO.mosfetStatus_bits.PRECHARGE = 1;
    PACK_PDO.mosfetStatus_bits.CHARGE = 1;
    PACK_PDO.mosfetStatus_bits.DISCHARGE = 1;
    TESTCASE( 1,   10.0f ,     50.000008f)
    TESTCASE( 2,  -10.0f ,     49.900009f)
#undef TESTCASE
/*********************************************************************************************/
    printf("MosControl\n");
#define TESTCASE(nr, set1, set2, set3, set4, set5, set6, set7, set8, expect1) \
        PACK_SDO.ChargeEnable = set1; \
        PACK_SDO.DischargeEnable = set2; \
        g_PackPdoData[id].swAlertFlags = set3; \
        PACK_PDO.mosfetStatus_bits.CHARGE = set4; \
        PACK_PDO.mosfetStatus_bits.DISCHARGE = set5; \
        PACK_PDO.mosfetStatus_bits.PRECHARGE = set6; \
        PACK_PDO.voltage = set7; \
        g_GlobalPdoData->voltage = set8; \
        MosControl(id); \
        if( (SpiReg[0x13] != expect1) ) { \
            printf("   TC%02u FAIL: ChargeEnable=%u\n",nr,set1); \
            printf("              DischargeEnable=%u\n",set2); \
            printf("              swAlertFlags=%u\n",set3); \
            printf("              mosfetStatus_bits.CHARGE=%u\n",set4); \
            printf("              mosfetStatus_bits.DISCHARGE=%u\n",set5); \
            printf("              mosfetStatus_bits.PRECHARGE=%u\n",set6); \
            printf("              voltage=%f\n",set7); \
            printf("              global.voltage=%f\n",set8); \
            printf("              MOS_TRIG=%u (expect %u)\n",SpiReg[0x13],expect1); \
            errors++; \
        }
    g_GlobalConfig.prechargeDeltaVoltage = 1.0f;
    printf(" * Alles Aus, kein Fehler\n");
    /*           ChaEn  DisEn  swAlert  moCha moDis moPre  voltage  globvoltage  mos_tim*/
    TESTCASE( 1, 0,     0,     0,       0,    0,    0,       40.0f,       0.0f,  0)
    printf(" * Wechsel aus -> ein, Ladevorgang, keine Fehler\n");
    TESTCASE( 1, 1,     1,     0,       0,    0,    0,       40.0f,       0.0f,  4)
    TESTCASE( 2, 1,     1,     0,       0,    0,    1,       40.0f,       0.0f,  4)
    TESTCASE( 3, 1,     1,     0,       0,    0,    1,       40.0f,       38.0f,  4)
    TESTCASE( 4, 1,     1,     0,       0,    0,    1,       40.0f,       39.0f,  7) //Wechsel in alle ein
    TESTCASE( 5, 1,     1,     0,       1,    1,    1,       40.0f,       40.0f,  3) //Precharge wird wieder abgedreht
    TESTCASE( 6, 1,     1,     0,       1,    1,    0,       40.0f,       40.0f,  3)
    printf(" * Wechsel ein -> aus, keine Fehler\n");
    TESTCASE( 1, 1,     1,     0,       1,    1,    0,       40.0f,       40.0f,  3)
    TESTCASE( 2, 0,     0,     0,       1,    1,    0,       40.0f,       40.0f,  0)
    printf(" * Wechsel aus -> ein, ohne Ladevorgang, keine Fehler\n");
    TESTCASE( 1, 1,     1,     0,       0,    0,    0,       40.0f,       40.0f,  4)
    TESTCASE( 2, 1,     1,     0,       0,    0,    1,       40.0f,       40.0f,  7)
    TESTCASE( 3, 1,     1,     0,       1,    1,    1,       40.0f,       40.0f,  3)
    TESTCASE( 4, 1,     1,     0,       1,    1,    0,       40.0f,       40.0f,  3)
    printf(" * Discharge ein, Charge aus -> ein, ohne Ladevorgang, keine Fehler\n");
    TESTCASE( 1, 0,     1,     0,       1,    1,    0,       40.0f,       40.0f,  1)
    TESTCASE( 2, 0,     1,     0,       0,    1,    0,       40.0f,       40.0f,  1)
    TESTCASE( 3, 1,     1,     0,       0,    1,    0,       40.0f,       40.0f,  5)
    TESTCASE( 4, 1,     1,     0,       0,    1,    1,       40.0f,       40.0f,  7)
    TESTCASE( 5, 1,     1,     0,       1,    1,    1,       40.0f,       40.0f,  3)
    TESTCASE( 6, 1,     1,     0,       1,    1,    0,       40.0f,       40.0f,  3)
    printf(" * Discharge aus -> ein, Charge ein, ohne Ladevorgang, keine Fehler\n");
    TESTCASE( 1, 1,     0,     0,       1,    1,    0,       40.0f,       40.0f,  2)
    TESTCASE( 2, 1,     0,     0,       1,    0,    0,       40.0f,       40.0f,  2)
    TESTCASE( 3, 1,     1,     0,       1,    0,    0,       40.0f,       40.0f,  6)
    TESTCASE( 4, 1,     1,     0,       1,    0,    1,       40.0f,       40.0f,  7)
    TESTCASE( 5, 1,     1,     0,       1,    1,    1,       40.0f,       40.0f,  3)
    TESTCASE( 6, 1,     1,     0,       1,    1,    0,       40.0f,       40.0f,  3)
    printf(" * Fehler errorAll\n");
    TESTCASE( 1, 0,     0,     3,       0,    0,    0,       40.0f,       40.0f,  0)
    TESTCASE( 2, 0,     1,     3,       0,    0,    1,       40.0f,       40.0f,  0)
    TESTCASE( 3, 1,     0,     3,       0,    0,    1,       40.0f,       40.0f,  0)
    TESTCASE( 4, 1,     1,     3,       0,    0,    1,       40.0f,       40.0f,  0)
    TESTCASE( 5, 0,     1,     3,       0,    1,    1,       40.0f,       40.0f,  0)
    TESTCASE( 6, 1,     0,     3,       1,    0,    1,       40.0f,       40.0f,  0)
    TESTCASE( 7, 1,     1,     3,       1,    1,    1,       40.0f,       40.0f,  0)
    TESTCASE( 8, 0,     1,     3,       0,    1,    0,       40.0f,       40.0f,  0)
    TESTCASE( 9, 1,     0,     3,       1,    0,    0,       40.0f,       40.0f,  0)
    TESTCASE(10, 1,     1,     3,       1,    1,    0,       40.0f,       40.0f,  0)
    printf(" * Fehler errorCharge\n");
    TESTCASE( 1, 0,     0,     1,       0,    0,    0,       40.0f,       40.0f,  0)
    TESTCASE( 2, 0,     1,     1,       0,    0,    1,       40.0f,       40.0f,  5)
    TESTCASE( 3, 1,     0,     1,       0,    0,    1,       40.0f,       40.0f,  0)
    TESTCASE( 4, 1,     1,     1,       0,    0,    1,       40.0f,       40.0f,  5)
    TESTCASE( 5, 0,     1,     1,       0,    1,    1,       40.0f,       40.0f,  1)
    TESTCASE( 6, 1,     0,     1,       1,    0,    1,       40.0f,       40.0f,  0)
    TESTCASE( 7, 1,     1,     1,       1,    1,    1,       40.0f,       40.0f,  1)
    TESTCASE( 8, 0,     1,     1,       0,    1,    0,       40.0f,       40.0f,  1)
    TESTCASE( 9, 1,     0,     1,       1,    0,    0,       40.0f,       40.0f,  0)
    TESTCASE(10, 1,     1,     1,       1,    1,    0,       40.0f,       40.0f,  1)
    printf(" * Fehler errorDischarge\n");
    TESTCASE( 1, 0,     0,     2,       0,    0,    0,       40.0f,       40.0f,  0)
    TESTCASE( 2, 0,     1,     2,       0,    0,    1,       40.0f,       40.0f,  0)
    TESTCASE( 3, 1,     0,     2,       0,    0,    1,       40.0f,       40.0f,  6)
    TESTCASE( 4, 1,     1,     2,       0,    0,    1,       40.0f,       40.0f,  6)
    TESTCASE( 5, 0,     1,     2,       0,    1,    1,       40.0f,       40.0f,  0)
    TESTCASE( 6, 1,     0,     2,       1,    0,    1,       40.0f,       40.0f,  2)
    TESTCASE( 7, 1,     1,     2,       1,    1,    1,       40.0f,       40.0f,  2)
    TESTCASE( 8, 0,     1,     2,       0,    1,    0,       40.0f,       40.0f,  0)
    TESTCASE( 9, 1,     0,     2,       1,    0,    0,       40.0f,       40.0f,  2)
    TESTCASE(10, 1,     1,     2,       1,    1,    0,       40.0f,       40.0f,  2)


#undef TESTCASE
/*********************************************************************************************/
    printf("%u Fehler\n",errors);
    return (errors != 0);
}
