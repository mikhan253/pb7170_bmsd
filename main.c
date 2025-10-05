#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "tasksetup.h"
#include "spi.h"

#include "dataobjects.h"

#define MAX_BATTERY_PACKS 10

BATTERY_PDO_t battery_data[MAX_BATTERY_PACKS];
uint8_t* battery_userconfig[MAX_BATTERY_PACKS];


uint32_t load_battery_userconfigs() {
    uint32_t found_packs = 0;
    for (int i = 0; i < MAX_BATTERY_PACKS; i++) {
        char filename[20];
        snprintf(filename, sizeof(filename), "pack%d_userconf.bin", i);

        struct stat st;
        if (stat(filename, &st) == 0 && st.st_size > 0) {
            FILE *f = fopen(filename, "rb");
            if (f) {
                battery_userconfig[i] = malloc(st.st_size);
                if (battery_userconfig[i]) {
                    fread(battery_userconfig[i], 1, st.st_size, f);
                    found_packs++;
                }
                fclose(f);
            }
        } else {
            battery_userconfig[i] = NULL;
        }
    }
    return found_packs;
}

int main(void) {
    uint32_t BatteryPackCounter = 0;
    uint32_t MaxBatteryPacks = 0;
    if (setup_task(5)) {
        printf("Failed to set up task\n");
        return 1;
    }
    if (spi_init("/dev/spidev0.0", 1250000, 0, 8)) {
        printf("Failed to initialize SPI\n");
        return 1;
    }

    /* Initialisiere die Batteriedaten */
    for(int i=0; i<MAX_BATTERY_PACKS; i++) {
        battery_data[i].ID = i;
        battery_data[i].Statemachine = PB7170_STATE_WAIT_INIT;
        battery_data[i].SPI_ErrorCount = 0;
    }

    MaxBatteryPacks = load_battery_userconfigs();
    printf("Gefundene Userconfigs: %u Batteriepacks\n", MaxBatteryPacks);

    while (1) {
        uint64_t expirations;
        read(timer_fd, &expirations, sizeof(expirations));  // blockiert bis Timer feuert
        if (expirations == 1)
        {
            uint32_t ret, counta=0;
            uint16_t data[32];

            switch(battery_data[BatteryPackCounter].Statemachine) {
                case PB7170_STATE_WAIT_INIT:
                    if (pb7170_spi_read_register(0x00, data, 1))
                        battery_data[BatteryPackCounter].SPI_ErrorCount++;
                    else 
                        if (data[0] == 0x6000) { /* TOP_STATUS muss auf Power-up Complete sein */
                            printf("PB7170 (ID=%u) gefunden, initialisiere...\n", battery_data[BatteryPackCounter].ID);

                            /* Safe Mode */
                            pb7170_spi_write_register(0x13, 0); // Alle MOSFETs aus
                            pb7170_spi_write_register(0x0c, 0); // Alle Balancer aus

                            battery_data[BatteryPackCounter].Statemachine = PB7170_STATE_INIT;
                        }
                    break;
                case PB7170_STATE_INIT:
                    pb7170_spi_write_register(0x45,0x95); // USER Unlock
                    for (int i = 0; i < 6; i++)
                    printf("%02X: %02X ", (unsigned int)battery_userconfig[BatteryPackCounter][0], (unsigned int)battery_userconfig[BatteryPackCounter][1]<<8 | (unsigned int)battery_userconfig[BatteryPackCounter][2]);

                    battery_data[BatteryPackCounter].Statemachine = PB7170_STATE_CONFIG;

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
        }
        else
            printf("Timer expired %llu times\n", (unsigned long long)expirations);
    }

    close(timer_fd);
    return 0;
}
