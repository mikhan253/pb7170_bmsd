#ifndef _DATAOBJECTS_H_
#define _DATAOBJECTS_H_

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
    PB7170_STATE_COUNT  // Anzahl der States (letztes Element)
} PB7170_Statemachine_t;

typedef struct {
    /***************** Allgemeine Informationen *****************/
    uint32_t ID;
    PB7170_Statemachine_t Statemachine;

    /***************** Allgemeine Statusinformationen *****************/
    uint32_t SPI_ErrorCount;

    /***************** Fehlerbits *****************/
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
    union {
        uint32_t HW_AlertFlags;
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
        uint32_t HW_AlertState;
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
    float AvailableChargeCurrent;
    float AvailableDischargeCurrent;

    float Capacity;
    float SOC;
    float SOH;
    float CycleCount;
} BATTERY_PDO_t;

#endif