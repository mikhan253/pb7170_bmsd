#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>

#include "tasksetup.h"
#include "spi.h"

#include "dataobjects.h"

float ntc_to_temperature(float value, const float *NTC_CURVE, int curve_len)
{
    float result = 0.0f;
    int n = curve_len - 1;

    for (int i = 0; i < curve_len; i++) {
        result += NTC_CURVE[i] * powf(value, (float)(n - i));
    }

    return result;
}

int pb7170_init(int id) {
    uint32_t i = 0;

    while (battery_userconfig_blob[id][i].address > 0) {
        // Beginn eines zusammenhängenden Blocks
        uint32_t block_start = i;
        uint32_t block_length = 1;

        // Suche, wie viele aufeinanderfolgende Register wir haben (max. 32)
        while (battery_userconfig_blob[id][block_start + block_length].address ==
               battery_userconfig_blob[id][block_start + block_length - 1].address + 1 &&
               block_length < 32 &&
               battery_userconfig_blob[id][block_start + block_length].address > 0)
            block_length++;

        // Schreibe die Register einzeln (wie bisher)
        for (uint32_t j = 0; j < block_length; j++) {
            pb7170_spi_write_register(
                battery_userconfig_blob[id][block_start + j].address,
                battery_userconfig_blob[id][block_start + j].data
            );
        }

        // Lese den ganzen Block auf einmal
        uint16_t readback_block[32]; // max Blockgröße
        pb7170_spi_read_register(
                battery_userconfig_blob[id][block_start].address,
                readback_block,
                block_length
            );

        // Vergleiche jedes Register im Block
        for (uint32_t j = 0; j < block_length; j++)
            if (battery_userconfig_blob[id][block_start + j].data != readback_block[j])
                return -1;

        i += block_length; // Nächster Block
    }

    return 0;
}

int pb7170_readdata(int id) {
    uint16_t data[28];

    pb7170_spi_read_register(0x01, data, 16);
    battery_pdo_data[id].HW_Status = data[0];
    battery_pdo_data[id].HW_AlertFlags = (data[2] << 16) | data[1];
    battery_pdo_data[id].HW_AlertState = (data[5] << 16) | data[4];
    battery_pdo_data[id].HW_Alert_CellUnderOvervoltage = (data[7] << 16) | data[6];
    battery_pdo_data[id].HW_AlertAux = data[9];
    battery_pdo_data[id].HW_BalanceTimer = data[14];
    battery_pdo_data[id].HW_BalanceStatus = data[15];

    pb7170_spi_read_register(0x84, data, 28);
    battery_pdo_data[id].Current = (float)((int16_t)data[0]) * battery_generalconfig_blob[id]->current_cadc_factor;
    battery_pdo_data[id].PackVoltage = (float)data[1] * 1.6e-3;
    battery_pdo_data[id].PVDDVoltage = (float)data[2] * 2.5e-3;
    for(int i=0; i < 16; i++)
        battery_pdo_data[id].V_Cells[i] = (float)data[3 + i] * 100e-6;
    for(int i=0; i < 4; i++)
        battery_pdo_data[id].NTC_Temperatures[i] = ntc_to_temperature((float)data[20 + i], battery_generalconfig_blob[id]->ntc_polynom, 11);
    battery_pdo_data[id].DieTemp = (float)(25437 - data[26]) / 59.17 - 64.5;
    battery_pdo_data[id].FastCurrent = (float)(data[27] & 0x7fff) * battery_generalconfig_blob[id]->current_vadc_factor;
    if (data[27] & 0x8000)
        battery_pdo_data[id].FastCurrent = -battery_pdo_data[id].FastCurrent;

    return 0;
}

int main(void) {
    if (setup_task(125)) {
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
            for(uint32_t current_id = 0; current_id <= 7; current_id++)
            {
                if ((battery_enabled & (1 << current_id)) == 0)
                    continue;
                spi_select_device(current_id);

                uint16_t read_data;
                //for (uint32_t current_id = 0; current_id < MAX_BATTERY_PACKS)

                switch(battery_pdo_data[current_id].Statemachine) {
                    case PB7170_STATE_WAIT_INIT:
                        pb7170_spi_read_register(0x00, &read_data, 1);
                        if (read_data == 0x6000) { /* TOP_STATUS muss auf Power-up Complete sein */
                            printf("PACK%u: PB7170 gefunden, initialisiere...\n", battery_pdo_data[current_id].ID);

                            /* Safe Mode */
                            pb7170_spi_write_register(0x13, 0); // Alle MOSFETs aus
                            pb7170_spi_write_register(0x0c, 0); // Alle Balancer aus

                            battery_pdo_data[current_id].Statemachine = PB7170_STATE_INIT;
                        }
                        break;
                    case PB7170_STATE_INIT:
                        pb7170_spi_write_register(0x45,0x95); // USER Unlock
                        if (pb7170_init(current_id) == 0) {
                            pb7170_spi_write_register(0x45,0x00); // USER lock
                            pb7170_spi_write_register(0x05,0x4000); //Clear RESET Flag
                            pb7170_spi_write_register(0x0d,31); //Setup Balancer

                            printf("PACK%u: Userconfig erfolgreich geschrieben\n", battery_pdo_data[current_id].ID);
                            battery_pdo_data[current_id].Statemachine = PB7170_STATE_CONFIG;
                        } else {
                            printf("PACK%u: Fehler beim Schreiben der Userconfig\n", battery_pdo_data[current_id].ID);
                            battery_pdo_data[current_id].Statemachine = PB7170_STATE_ERROR;
                            break;
                        }
                        
                        

                        break;
                    case PB7170_STATE_CONFIG:
                        battery_pdo_data[current_id].Statemachine = PB7170_STATE_RUN;
                        break;
                    case PB7170_STATE_RUN:
                        pb7170_readdata(current_id);
                        //printf("PACK%u: V=%.3fV I=%.3fA T=%.1fC SOC=%.1f%%\n", battery_pdo_data[current_id].ID, battery_pdo_data[current_id].PackVoltage, battery_pdo_data[current_id].Current, battery_pdo_data[current_id].DieTemp, battery_pdo_data[current_id].SOC);
                        // Normalbetrieb
                        break;
                    case PB7170_STATE_ERROR:
                        // Fehlerbehandlung
                        break;
                    default:
                        break;
                }
            }
        }
        else
            printf("Timer expired %llu times\n", (unsigned long long)expirations);
    }

    close(timer_fd);
    return 0;
}
