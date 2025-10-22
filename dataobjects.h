#ifndef _DATAOBJECTS_H_
#define _DATAOBJECTS_H_

#define MAX_BATTERY_PACKS 10
#define GENERALCONF_CURRENTTABLE_SIZE 10
#define NUMBER_OF_CELLS 16

typedef enum {
    AFE_STATE_WAIT_INIT = 0,
    AFE_STATE_INIT,
    AFE_STATE_CONFIG,
    AFE_STATE_WAIT_DIAG0,
    AFE_STATE_DIAG0,
    AFE_STATE_WAIT_DIAG1,
    AFE_STATE_DIAG1,
    AFE_STATE_WAIT_DIAG2,
    AFE_STATE_DIAG2,
    AFE_STATE_SANITY_CHECK,
    AFE_STATE_RUN_WARNING,
    AFE_STATE_RUN,
    AFE_STATE_ERROR,
    AFE_STATE_DISABLED,
    AFE_STATE_COUNT  // Anzahl der States (letztes Element)
} EStateMachine_t;

typedef struct {
    uint32_t numberOfPacks;
    uint32_t sync;
    float voltage;
    
} GLOBAL_PDO_t;

typedef struct {
    uint32_t numberOfPacks;
    uint32_t diagWireBreakDelta;
    float prechargeDeltaVoltage;

} GLOBAL_CONF_t;

typedef struct {
    /***************** Allgemeine Informationen *****************/
    uint32_t id;
    EStateMachine_t stateMachine;
    uint32_t aliveCounter;

    /***************** Allgemeine Statusinformationen *****************/
    uint32_t spiRetries;

    /***************** SW zeug *****************/
    union {
        uint32_t swAlertFlags;
        struct {
            uint32_t HW_CHARGE_OC : 1;
            uint32_t HW_DISCHARGE_OC : 1;
            uint32_t SW_CHARGE_OC : 1;
            uint32_t SW_DISCHARGE_OC : 1;
            uint32_t SHORT : 1;
            uint32_t CHIPSTATE_ERR : 1;
            uint32_t HW_OVERTEMP : 1;
            uint32_t HW_UNDERTEMP : 1;
            uint32_t PACK_OVERTEMP : 1;
            uint32_t PACK_UNDERTEMP : 1;
            uint32_t TEMP_MISMATCH : 1;
            uint32_t COMM_ERR : 1;
            uint32_t DIAG_ERR : 1;
            uint32_t PACK_OV : 1;
            uint32_t PACK_UV : 1;
            uint32_t CELL_OV : 1;
            uint32_t CELL_UV : 1;
            uint32_t CELL_MISMATCH : 1;
            uint32_t PRECHARGE_FAIL : 1;
            uint32_t CURRENT_ABNORMAL : 1;
        } swAlertFlags_bits;
    };
    union {
        uint32_t swWarningFlags;
    };

    /***************** HW zeug *****************/
    union {
        uint32_t hwStatus; /* PB7170 STATUS_MISC 0x01 */
        struct {
            uint32_t STA_LODD : 1;
            uint32_t reserved1 : 1;
            uint32_t CD : 2;
            uint32_t STA_SLEEP : 1;
            uint32_t STA_WAKE : 1;
            uint32_t BLSW_ON : 1;
            uint32_t ADC_ON : 1;
            uint32_t STA_CHGD : 1;
            uint32_t reserved2 : 1;
            uint32_t SCH_CNT : 4;
        } hwStatus_bits;
    };
    union {
        uint32_t hwAlertFlags; /* PB7170 ALRT_FLG0,1 0x02-03 */
        struct {
            uint32_t CHARGE_OC : 1;
            uint32_t DISCHARGE_OC : 1;
            uint32_t SHORT : 1;
            uint32_t BAL_TIMEOUT : 1;
            uint32_t BAL_UV : 1;
            uint32_t SCHED_END : 1;
            uint32_t TIM_END : 1;
            uint32_t WDT_OVF : 1;
            uint32_t EXT_PROT : 1;
            uint32_t PVDD_UVOV : 1;
            uint32_t CELL_UV : 1;
            uint32_t CELL_OV : 1;
            uint32_t LV : 1;
            uint32_t THERM_SD : 1;
            uint32_t CHGDD : 1;
            uint32_t LODD : 1;
            uint32_t SPI_CRC_ERR : 1;
            uint32_t MISMATCH : 1;
            uint32_t reserved1 : 4;  // Bits 18-21
            uint32_t TDIE_HI : 1;
            uint32_t TDIE_LO : 1;
            uint32_t PACK_UV : 1;
            uint32_t PACK_OV : 1;
            uint32_t reserved2 : 2;  // Bits 26-27
            uint32_t AUX_OV : 1;
            uint32_t AUX_UV : 1;
            uint32_t reserved3 : 1;  // Bit 30
            uint32_t MEAS_DONE : 1;
        } hwAlertFlags_bits;
    };
    union {
        uint32_t hwAlertState; /* PB7170 ALRT_STAT0,1 0x05-06 */
        struct {
            uint32_t CHARGE_OC : 1;
            uint32_t DISCHARGE_OC : 1;
            uint32_t SHORT : 1;
            uint32_t CELL_UV : 1;
            uint32_t CELL_OV : 1;
            uint32_t PVDD_UVOV : 1;
            uint32_t reserved1 : 8;  // Bits 6-13
            uint32_t RESET : 1;
            uint32_t SLEEP : 1;
            uint32_t VREF : 1;
            uint32_t LVMUX : 1;
            uint32_t AVDD : 1;
            uint32_t DVDD : 1;
            uint32_t MISMATCH : 1;
            uint32_t TDIE_LO : 1;
            uint32_t TDIE_HI : 1;
            uint32_t SPI_CRC_ERR : 1;
            uint32_t EEPROM_CRC_ERR : 1;
            uint32_t PACK_UV : 1;
            uint32_t PACK_OV : 1;
            uint32_t reserved2 : 4;  // Bits 27-30
            uint32_t CLOCK_ABNORMAL : 1;
        } hwAlertState_bits;
    };
    uint32_t hwAlertCellUnderOvervoltage; /* PB7170 ALRT_OVCELL 0x07 ALRT_UVCELL 0x08 */
    union {
        uint32_t hwAlertAux; /* PB7170 ALRT_AUX 0x0A */
        struct {
            uint32_t AUXIN1_OV : 1;
            uint32_t AUXIN2_OV : 1;
            uint32_t AUXIN3_OV : 1;
            uint32_t AUXIN4_OV : 1;
            uint32_t reserved1 : 4;
            uint32_t AUXIN1_UV : 1;
            uint32_t AUXIN2_UV : 1;
            uint32_t AUXIN3_UV : 1;
            uint32_t AUXIN4_UV : 1;
        } hwAlertAux_bits;
    };
    uint32_t hwBalancerTimer; /* PB7170 BLSW_CMD 0x0F */
    uint32_t hwBalancerStatus; /* PB7170 BLSW_STAT 0x10 */
    /***************** Batteriemanagementstatus *****************/
    union {
        uint32_t mosfetStatus;
        struct {
            uint32_t PRECHARGE : 1;
            uint32_t CHARGE : 1;
            uint32_t DISCHARGE : 1;
        } mosfetStatus_bits;
    };
    float prechargeResistorI2t;

    float current;
    float fastCurrent;
    float cells[NUMBER_OF_CELLS];
    float ntcTemperature[4];
    float dieTemperature;
    float voltage;
    float pvddVoltage;
    float availableChargeCurrent;
    float availableDischargeCurrent;
    float availableCapacity;
    float totalCapacity;
    float stateOfCharge;
    float stateOfHealth;
    float cycleCount;
} PACK_PDO_t;

typedef struct {
    uint32_t ChargeEnable;
    uint32_t DischargeEnable;
    uint32_t swAlertFlagsClear;
    uint32_t ResetStateMachine;
} PACK_SDO_t;

/**************** Konfigurationsdateien ****************/
typedef struct __attribute__((packed)) {
    uint8_t address;
    uint16_t data;
} PACK_USERCONF_t;

typedef struct {
    float batteryNominalCapacity;
    float batteryNominalResistance;
    float bmsMaxCurrent;
    float bmsMaxCurrentReduced;
    float balancerStartVoltage;
    float balancerDiffVoltage;
    float cadcCurrentFactor;
    float vadcCurrentFactor;
    float prechargeResistorMaxI2t;
    float prechargeResistorI2tDecay;
    float ntcPolynom[11];
    float currentTableTemperature[GENERALCONF_CURRENTTABLE_SIZE];
    float currentTableChargeCurrent[GENERALCONF_CURRENTTABLE_SIZE];
    float currentTableDischargeCurrent[GENERALCONF_CURRENTTABLE_SIZE];
    float ocvTableSOC[11];
    float ocvTableVoltage[11];
    
} PACK_GENERALCONF_t;

typedef struct {
    float cadcOffset;
    float vadcOffset;
    float ntcOffset[4];
    float cellOffset[16];
    float pvddOffset;
    float tdieOffset;
    float cadcGain;
    float vadcGain;
    float ntcGain[4];
    float cellGain[16];
    float pvddGain;
    float tdieGain;
} PACK_CALIBRATION_t;

extern GLOBAL_CONF_t g_GlobalConfig;
extern GLOBAL_PDO_t* g_GlobalPdoData;
extern PACK_PDO_t* g_PackPdoData;
extern PACK_SDO_t* g_PackSdoData;
extern PACK_USERCONF_t* g_PackUserConfig[MAX_BATTERY_PACKS];
extern PACK_GENERALCONF_t* g_PackGeneralConfig[MAX_BATTERY_PACKS];
extern PACK_CALIBRATION_t* g_PackCalibration[MAX_BATTERY_PACKS];
extern uint16_t g_packEnabled;

int dob_LoadPackConfigs(void);
void dob_Cleanup();

#endif