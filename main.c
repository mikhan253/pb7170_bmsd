#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "tasksetup.h"
#include "spi.h"

#include "dataobjects.h"


int main(void) {
    uint32_t BatteryPackCounter = 0;
    if (setup_task(5)) {
        printf("Failed to set up task\n");
        return 1;
    }
    if (spi_init("/dev/spidev0.0", 1250000, 0, 8)) {
        printf("Failed to initialize SPI\n");
        return 1;
    }

    load_battery_all_configs();


    while (1) {
        uint64_t expirations;
        read(timer_fd, &expirations, sizeof(expirations));  // blockiert bis Timer feuert
        if (expirations == 1)
        {
            uint16_t data[32];

            switch(battery_pdo_data[BatteryPackCounter].Statemachine) {
                case PB7170_STATE_WAIT_INIT:
                    if (pb7170_spi_read_register(0x00, data, 1))
                        battery_pdo_data[BatteryPackCounter].SPI_ErrorCount++;
                    else 
                        if (data[0] == 0x6000) { /* TOP_STATUS muss auf Power-up Complete sein */
                            printf("PB7170 (ID=%u) gefunden, initialisiere...\n", battery_pdo_data[BatteryPackCounter].ID);

                            /* Safe Mode */
                            pb7170_spi_write_register(0x13, 0); // Alle MOSFETs aus
                            pb7170_spi_write_register(0x0c, 0); // Alle Balancer aus

                            battery_pdo_data[BatteryPackCounter].Statemachine = PB7170_STATE_INIT;
                        }
                    break;
                case PB7170_STATE_INIT:
                    pb7170_spi_write_register(0x45,0x95); // USER Unlock
                    for (uint32_t i = 0; battery_userconfig_blob[BatteryPackCounter][i].address > 0; i++) {
                        uint16_t readback;
                        pb7170_spi_write_register(battery_userconfig_blob[BatteryPackCounter][i].address, battery_userconfig_blob[BatteryPackCounter][i].data);
                        if (pb7170_spi_read_register(battery_userconfig_blob[BatteryPackCounter][i].address, &readback, 1))
                            battery_pdo_data[BatteryPackCounter].SPI_ErrorCount++;
                        else
                            if (battery_userconfig_blob[BatteryPackCounter][i].data != readback)
                                printf("Fehler beim Schreiben der Userconfig an Adresse %02X: Gesendet %02X, gelesen %02X\n", (unsigned int)battery_userconfig_blob[BatteryPackCounter][i].address, (unsigned int)battery_userconfig_blob[BatteryPackCounter][i].data, (unsigned int)readback);
                    }
                    printf("Userconfig erfolgreich geschrieben\n");
                    battery_pdo_data[BatteryPackCounter].Statemachine = PB7170_STATE_CONFIG;

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
