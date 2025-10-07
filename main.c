#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>

#ifdef TIME_IT
#include <time.h>
#endif

#include "tasksetup.h"
#include "spi.h"

#include "dataobjects.h"

float ntc_to_temperature(float value, const float *NTC_CURVE, int curve_len)
{
    double result = NTC_CURVE[0];
    for (int i = 1; i < curve_len; i++) {
        result = result * value + NTC_CURVE[i];
    }
    return (float)result;
}

int pb7170_init(int id) {
    uint32_t i = 0;

    while (g_PackUserConfig[id][i].address > 0) {
        // Beginn eines zusammenhängenden Blocks
        uint32_t block_start = i;
        uint32_t block_length = 1;

        // Suche, wie viele aufeinanderfolgende Register wir haben (max. 32)
        while (g_PackUserConfig[id][block_start + block_length].address ==
               g_PackUserConfig[id][block_start + block_length - 1].address + 1 &&
               block_length < 32 &&
               g_PackUserConfig[id][block_start + block_length].address > 0)
            block_length++;

        // Schreibe die Register einzeln (wie bisher)
        for (uint32_t j = 0; j < block_length; j++) {
            spi_AFEWriteRegister(
                g_PackUserConfig[id][block_start + j].address,
                g_PackUserConfig[id][block_start + j].data
            );
        }

        // Lese den ganzen Block auf einmal
        uint16_t readback_block[32]; // max Blockgröße
        spi_AFEReadRegister(
                g_PackUserConfig[id][block_start].address,
                readback_block,
                block_length
            );

        // Vergleiche jedes Register im Block
        for (uint32_t j = 0; j < block_length; j++)
            if (g_PackUserConfig[id][block_start + j].data != readback_block[j])
                return -1;

        i += block_length; // Nächster Block
    }

    return 0;
}

int pb7170_read_data(int id) {
    uint16_t data[28];

    spi_AFEReadRegister(0x01, data, 16);
    g_PackPdoData[id].hwStatus = data[0];
    g_PackPdoData[id].hwAlertFlags = (data[2] << 16) | data[1];
    g_PackPdoData[id].hwAlertState = (data[5] << 16) | data[4];
    g_PackPdoData[id].hwAlertCellUnderOvervoltage = (data[7] << 16) | data[6];
    g_PackPdoData[id].hwAlertAux = data[9];
    g_PackPdoData[id].hwBalancerTimer = data[14];
    g_PackPdoData[id].hwBalancerStatus = data[15];

    spi_AFEReadRegister(0x84, data, 28);
    g_PackPdoData[id].current = (float)((int16_t)data[0]) * g_PackGeneralConfig[id]->cadcCurrentFactor;
    g_PackPdoData[id].voltage = (float)data[1] * 1.6e-3;
    g_PackPdoData[id].pvddVoltage = (float)data[2] * 2.5e-3;
    for(int i=0; i < 16; i++)
        g_PackPdoData[id].cells[i] = (float)data[3 + i] * 100e-6;
    for(int i=0; i < 4; i++)
        g_PackPdoData[id].ntcTemperature[i] = ntc_to_temperature((float)data[20 + i], g_PackGeneralConfig[id]->ntcPolynom, 11);
    g_PackPdoData[id].dieTemperature = (float)(25437 - data[26]) / 59.17 - 64.5;
    g_PackPdoData[id].fastCurrent = (float)(data[27] & 0x7fff) * g_PackGeneralConfig[id]->vadcCurrentFactor;
    if (data[27] & 0x8000)
        g_PackPdoData[id].fastCurrent = -g_PackPdoData[id].fastCurrent;

    return 0;
}

int main(void) {
    if (tas_Init(125)) {
        printf("Failed to set up task\n");
        return 1;
    }
    if (spi_Init("/dev/spidev0.0", 1250000, 0, 8)) {
        printf("Failed to initialize SPI\n");
        return 1;
    }

    dob_LoadPackConfigs();

    while (1) {
        uint64_t expirations;
        read(g_timerFd, &expirations, sizeof(expirations));  // blockiert bis Timer feuert
        if (expirations != 1)
            printf("Timer expired %llu times\n", (unsigned long long)expirations);

#ifdef TIME_IT
        struct timespec t_start, t_end;
        clock_gettime(CLOCK_MONOTONIC, &t_start);
#endif
        for(uint32_t current_id = 0; current_id <= 7; current_id++)
        {
            if ((g_packEnabled & (1 << current_id)) == 0)
                continue;
            spi_SelectDevice(current_id);

            uint16_t read_data;
            //for (uint32_t current_id = 0; current_id < MAX_BATTERY_PACKS)

            switch(g_PackPdoData[current_id].stateMachine)
            {
                case PB7170_STATE_WAIT_INIT:
                    spi_AFEReadRegister(0x00, &read_data, 1);
                    if (read_data == 0x6000) { /* TOP_STATUS muss auf Power-up Complete sein */
                        printf("PACK%u: PB7170 gefunden, initialisiere...\n", g_PackPdoData[current_id].id);

                        /* Safe Mode */
                        spi_AFEWriteRegister(0x13, 0); // Alle MOSFETs aus
                        spi_AFEWriteRegister(0x0c, 0); // Alle Balancer aus

                        g_PackPdoData[current_id].stateMachine = PB7170_STATE_INIT;
                    }
                    break;
                case PB7170_STATE_INIT:
                    spi_AFEWriteRegister(0x45,0x95); // USER Unlock
                    if (pb7170_init(current_id) == 0) {
                        spi_AFEWriteRegister(0x45,0x00); // USER lock
                        spi_AFEWriteRegister(0x05,0x4000); //Clear RESET Flag
                        spi_AFEWriteRegister(0x0d,31); //Setup Balancer

                        printf("PACK%u: Userconfig erfolgreich geschrieben\n", g_PackPdoData[current_id].id);
                        g_PackPdoData[current_id].stateMachine = PB7170_STATE_CONFIG;
                    } else {
                        printf("PACK%u: Fehler beim Schreiben der Userconfig\n", g_PackPdoData[current_id].id);
                        g_PackPdoData[current_id].stateMachine = PB7170_STATE_ERROR;
                        break;
                    }
                    
                    

                    break;
                case PB7170_STATE_CONFIG:
                    g_PackPdoData[current_id].stateMachine = PB7170_STATE_RUN;
                    break;
                case PB7170_STATE_RUN:
                    pb7170_read_data(current_id);
                    //printf("PACK%u: V=%.3fV I=%.3fA T=%.1fC stateOfCharge=%.1f%%\n", battery_pdo_data[current_id].id, battery_pdo_data[current_id].voltage, battery_pdo_data[current_id].current, battery_pdo_data[current_id].dieTemperature, battery_pdo_data[current_id].stateOfCharge);
                    // Normalbetrieb
                    break;
                case PB7170_STATE_ERROR:
                    // Fehlerbehandlung
                    break;
                default:
                    break;
            }
        }
#ifdef TIME_IT
        if (!clock_gettime(CLOCK_MONOTONIC, &t_end) != 0) {
            double elapsed_ms = (t_end.tv_sec - t_start.tv_sec) * 1e3 + (t_end.tv_nsec - t_start.tv_nsec) / 1e6;
            printf("for-loop duration: %.3f ms\n", elapsed_ms);
        }
#endif

    }

    close(g_timerFd);
    return 0;
}
