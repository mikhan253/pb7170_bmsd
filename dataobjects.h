#ifndef _DATAOBJECTS_H_
#define _DATAOBJECTS_H_

#define MAX_BATTERY_PACKS 10

typedef enum {
    PB7170_STATE_WAIT_INIT = 0,
    PB7170_STATE_INIT,
    PB7170_STATE_CONFIG,
    PB7170_STATE_DIAG0,
    PB7170_STATE_WAIT_DIAG0,
    PB7170_STATE_DIAG1,
    PB7170_STATE_WAIT_DIAG1,
    PB7170_STATE_DIAG2,
    PB7170_STATE_WAIT_DIAG2,
    PB7170_STATE_RUN,
    PB7170_STATE_ERROR,
    PB7170_STATE_DISABLED,
    PB7170_STATE_COUNT  // Anzahl der States (letztes Element)
} PB7170_Statemachine_t;

typedef struct {
    /***************** Allgemeine Informationen *****************/
    uint32_t ID;
    PB7170_Statemachine_t Statemachine;

    /***************** Allgemeine Statusinformationen *****************/
    uint32_t SPI_ErrorCount;

    /***************** SW zeug *****************/
    union {
        uint32_t SW_AlertFlags;
        struct {
            uint32_t CHARGE_OC : 1;
            uint32_t DISCHARGE_OC : 1;
            uint32_t STATE_ERR : 1;
            uint32_t OVERTEMP : 1;
            uint32_t UNDERTEMP : 1;
            uint32_t COMM_ERR : 1;
        } SW_AlertFlags_bits;
    };

    /***************** HW zeug *****************/
    union {
        uint32_t HW_Status; /* PB7170 STATUS_MISC 0x01 */
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
        } HW_Status_bits;
    };
    union {
        uint32_t HW_AlertFlags; /* PB7170 ALRT_FLG0,1 0x02-03 */
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
            uint32_t reserved2 : 1;  // Bit 24
            uint32_t PACK_UV : 1;
            uint32_t PACK_OV : 1;
            uint32_t reserved3 : 2;  // Bits 26-27
            uint32_t AUX_OV : 1;
            uint32_t AUX_UV : 1;
            uint32_t reserved4 : 1;  // Bit 30
            uint32_t MEAS_DONE : 1;
        } HW_AlertFlags_bits;
    };
    union {
        uint32_t HW_AlertState; /* PB7170 ALRT_STAT0,1 0x05-06 */
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
        } HW_AlertState_bits;
    };
    uint16_t HW_Alert_CellOvervoltage; /* PB7170 ALRT_OVCELL 0x07 */
    uint16_t HW_Alert_CellUndervoltage; /* PB7170 ALRT_OVCELL 0x08 */
    union {
        uint16_t HW_AlertAux; /* PB7170 ALRT_AUX 0x0A */
        struct {
            uint16_t AUXIN1_OV : 1;
            uint16_t AUXIN2_OV : 1;
            uint16_t AUXIN3_OV : 1;
            uint16_t AUXIN4_OV : 1;
            uint16_t reserved1 : 4;
            uint16_t AUXIN1_UV : 1;
            uint16_t AUXIN2_UV : 1;
            uint16_t AUXIN3_UV : 1;
            uint16_t AUXIN4_UV : 1;
        } HW_AlertAux_bits;
    };
    uint16_t HW_BalanceTimer; /* PB7170 BLSW_CMD 0x0F */
    uint16_t HW_BalanceStatus; /* PB7170 BLSW_STAT 0x10 */
    /***************** Batteriemanagementstatus *****************/
    union {
        uint32_t MOSFet_Status;
        struct {
            uint32_t PRECHARGE : 1;
            uint32_t CHARGE : 1;
            uint32_t DISCHARGE : 1;
        } MOSFet_Status_bits;
    };

    uint32_t BalancerState;

    float Current;
    float FastCurrent;
    float V_Cells[16];
    float NTC_Temps[4];
    float DieTemp;
    float PackVoltage;
    float PVDDVoltage;
    float AvailableChargeCurrent;
    float AvailableDischargeCurrent;

    float Capacity;
    float SOC;
    float SOH;
    float CycleCount;
} BATTERY_PDO_t;

/**************** Konfigurationsdateien ****************/
typedef struct __attribute__((packed)) {
    uint8_t address;
    uint16_t data;
} BATTERY_USERCONF_BLOB_t;

typedef struct {
    float balancer_start_voltage;
    float balancer_diff_voltage;
    float current_cadc_factor;
    float current_vadc_factor;
    float ntc_polynom[11];
} BATTERY_GENERALCONF_t;

typedef struct {
    float cadc_offset;
    float vadc_offset;
    float ntc_offset[4];
    float cell_offset[16];
    float pvdd_offset;
    float cadc_gain;
    float vadc_gain;
    float ntc_gain[4];
    float cell_gain[16];
    float pvdd_gain;
} BATTERY_CALIBRATION_t;

extern BATTERY_PDO_t* battery_pdo_data;
extern BATTERY_USERCONF_BLOB_t* battery_userconfig_blob[MAX_BATTERY_PACKS];
extern BATTERY_GENERALCONF_t*   battery_generalconfig_blob[MAX_BATTERY_PACKS];
extern BATTERY_CALIBRATION_t*   battery_calibration_blob[MAX_BATTERY_PACKS];
extern uint16_t battery_enabled;

void load_battery_all_configs(void);
#endif