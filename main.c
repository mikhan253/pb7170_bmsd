#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#include "tasksetup.h"
#include "spi.h"

#include "dataobjects.h"

BATTERY_PDO_t battery_data[1];

int main(void) {
    uint16_t data[32];
    int ret;
    uint32_t counta=0;
    uint32_t BatteryPackCount = 0;

    if (setup_task(5)) {
        printf("Failed to set up task\n");
        return 1;
    }
    if (spi_init("/dev/spidev0.0", 1250000, 0, 8)) {
        printf("Failed to initialize SPI\n");
        return 1;
    }

    /* Initialisiere die Batteriedaten */
    for(int i=0; i<=BatteryPackCount; i++) {
        battery_data[i].ID = i + 1;
        battery_data[i].Statemachine = PB7170_STATE_WAIT_INIT;
        battery_data[i].SPI_ErrorCount = 0;
    }

    while (1) {
        uint64_t expirations;
        read(timer_fd, &expirations, sizeof(expirations));  // blockiert bis Timer feuert
        if (expirations == 1)
        {
            switch(battery_data[BatteryPackCount].Statemachine) {
                case PB7170_STATE_WAIT_INIT:
                    if (pb7170_spi_read_register(0x00, data, 1))
                        battery_data[BatteryPackCount].SPI_ErrorCount++;
                    else 
                        if (data[0] == 0x6000) { /* TOP_STATUS muss auf Power-up Complete sein */
                            printf("PB7170 (ID=%u) gefunden, initialisiere...\n", battery_data[BatteryPackCount].ID);

                            /* Lade Konfiguration */
                            

                            battery_data[BatteryPackCount].Statemachine = PB7170_STATE_INIT;
                        }
                    break;
                case PB7170_STATE_INIT:
                    /* Safe Mode */
                    pb7170_spi_write_register(0x13, 0); // Alle MOSFETs aus
                    pb7170_spi_write_register(0x0c, 0); // Alle Balancer aus

                    pb7170_spi_write_register(0x45,0x95); // USER Unlock

                    break;
                case PB7170_STATE_CONFIG:
                    // Konfigurationscode hier
                    break;
                case PB7170_STATE_RUN:
                    // Normalbetrieb
                    break;
                case PB7170_STATE_ERROR:
                    // Fehlerbehandlung
                    break;
                default:
                    break;
            }

            ret=pb7170_spi_read_register(0x00, data, 32);
            if (ret)
                battery_data[BatteryPackCount].SPI_ErrorCount++;
            counta++;
            if (counta>=200) {
                printf("SPI (Errors=%u): 0x%04X 0x%04X 0x%04X\n", battery_data[0].SPI_ErrorCount, data[0], data[1], data[2]);
                counta=0;
            }
        }
        else
            printf("Timer expired %llu times\n", (unsigned long long)expirations);
    }

    close(timer_fd);
    return 0;
}
