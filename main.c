#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#include <sys/timerfd.h>
#include <sys/resource.h>

#include <sys/syscall.h>
#include <sched.h>

// IO-Priorität Konstanten
#define IOPRIO_CLASS_SHIFT 13
#define IOPRIO_CLASS_RT 1


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
    uint32_t ID;
    PB7170_Statemachine_t Statemachine;

    union {
        uint32_t MOSFet_Status;
        struct {
            uint32_t PRECHARGE : 1;
            uint32_t CHARGE : 1;
            uint32_t DISCHARGE : 1;
        } MOSFet_Status_bits;
    };

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

    uint32_t BalancerState;

    float Current;
    float FastCurrent;
    float V_Cells[16];
    float NTC_Temps[4];
    float DieTemp;
    float PackVoltage;

    float SOC;
    float SOH;
    float CycleCount;

    float Capacity;
} BATTERY_PDO_t;




void cyclic_task() {
    static uint64_t last_ms = 0;
    uint64_t now_ms = 0;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    now_ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    if (last_ms != 0) {
        printf("Cyclic task executed, delta: %llu ms\n", (unsigned long long)(now_ms - last_ms));
    } else {
        printf("Cyclic task executed, first call\n");
    }
    last_ms = now_ms;
}

int main() {
    int tfd;
    struct itimerspec timer;
    uint64_t expirations;

    // Setze sehr hohe Prozess-Priorität (Realtime)
    if (setpriority(PRIO_PROCESS, 0, -20) != 0) {
        perror("setpriority");
    }

    struct sched_param param;
    param.sched_priority = 99;
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        perror("sched_setscheduler");
    }

    // Setze IO-Priorität auf höchste Klasse (IOPRIO_CLASS_RT, prio 0)
    int ioprio = (IOPRIO_CLASS_RT << IOPRIO_CLASS_SHIFT) | 0;
    if (syscall(SYS_ioprio_set, 1, 0, ioprio) != 0) { // SYS_ioprio_set aus <sys/syscall.h>
        perror("ioprio_set");
    }

    // Verhindere OOM-Kill
    FILE *oom = fopen("/proc/self/oom_score_adj", "w");
    if (oom) {
        fprintf(oom, "-1000\n");
        fclose(oom);
    } else {
        perror("oom_score_adj");
    }

    tfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (tfd == -1) {
        perror("timerfd_create");
        return 1;
    }

    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_nsec = 250 * 1000000; // 250ms
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_nsec = 250 * 1000000; // first expiration after 250ms

    if (timerfd_settime(tfd, 0, &timer, NULL) == -1) {
        perror("timerfd_settime");
        close(tfd);
        return 1;
    }

    while (1) {
        if (read(tfd, &expirations, sizeof(expirations)) != sizeof(expirations)) {
            perror("read");
            break;
        }
        cyclic_task();
    }

    close(tfd);
    return 0;
}