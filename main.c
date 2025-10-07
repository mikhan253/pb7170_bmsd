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

/*int CyclicDelta(int actualVal, int previousVal) {
    const int BEREICH = 16;
    int delta = (actualVal - previousVal) % BEREICH;
    
    // Korrektur für negative Modulo-Ergebnisse
    if (delta < 0) {
        delta += BEREICH;
    }
    
    // Finde den kürzesten Weg
    if (delta > BEREICH / 2) {
        delta -= BEREICH;
    }
    
    return delta;
}*/

float NtcToTemperature(float value, const float *ntcCurve, int len)
{
    float result = ntcCurve[0];
    for (int i = 1; i < len; i++) {
        result = result * value + ntcCurve[i];
    }
    return result;
}

int AFEInit(int id) {
    uint32_t i = 0;

    while (g_PackUserConfig[id][i].address > 0) {
        // Beginn eines zusammenhängenden Blocks
        uint32_t start = i;
        uint32_t len = 1;

        // Suche, wie viele aufeinanderfolgende Register wir haben (max. 32)
        while (g_PackUserConfig[id][start + len].address ==
               g_PackUserConfig[id][start + len - 1].address + 1 &&
               len < 32 &&
               g_PackUserConfig[id][start + len].address > 0)
            len++;

        // Schreibe die Register einzeln (wie bisher)
        for (uint32_t j = 0; j < len; j++) {
            spi_AFEWriteRegister(
                g_PackUserConfig[id][start + j].address,
                g_PackUserConfig[id][start + j].data
            );
        }

        // Lese den ganzen Block auf einmal
        uint16_t readback[32]; // max Blockgröße
        spi_AFEReadRegister(
                g_PackUserConfig[id][start].address,
                readback,
                len
            );

        // Vergleiche jedes Register im Block
        for (uint32_t j = 0; j < len; j++)
            if (g_PackUserConfig[id][start + j].data != readback[j])
                return -1;

        i += len; // Nächster Block
    }

    return 0;
}

int AFEReadData(int id) {
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
        g_PackPdoData[id].ntcTemperature[i] = NtcToTemperature((float)data[20 + i], g_PackGeneralConfig[id]->ntcPolynom, 11);
    g_PackPdoData[id].dieTemperature = (float)(25437 - data[26]) / 59.17 - 64.5;
    g_PackPdoData[id].fastCurrent = (float)(data[27] & 0x7fff) * g_PackGeneralConfig[id]->vadcCurrentFactor;
    if (data[27] & 0x8000)
        g_PackPdoData[id].fastCurrent = -g_PackPdoData[id].fastCurrent;

    return 0;
}


int main(void) {
    if (tas_Init(255)) { //etwas größer als 250ms, um immer einen neuen wert zu bekommen
        printf("Failed to set up task\n");
        return 1;
    }
    
    const unsigned int GPIO_PINS[3] = {24, 25, 26};
    if (spi_Init("/dev/spidev0.0", 1250000, 0, 8, "/dev/gpiochip1",GPIO_PINS, 3)) {
        printf("Failed to initialize SPI\n");
        return 1;
    }

    dob_LoadPackConfigs();

    while (1) {
        uint64_t timerExpirations;
        read(g_timerFd, &timerExpirations, sizeof(timerExpirations));  // blockiert bis Timer feuert
        if (timerExpirations != 1)
            printf("Timer expired %llu times\n", (unsigned long long)timerExpirations);

#ifdef TIME_IT
        struct timespec t_start, t_end;
        clock_gettime(CLOCK_MONOTONIC, &t_start);
#endif
        //for (uint32_t curId = 0; curId < MAX_BATTERY_PACKS)
        for(uint32_t curId = 0; curId <= 7; curId++)
        {
            if ((g_packEnabled & (1 << curId)) == 0)
                continue;
            spi_SelectDevice(curId);

            uint16_t data;


            switch(g_PackPdoData[curId].stateMachine)
            {
                case AFE_STATE_WAIT_INIT:
                    spi_AFEReadRegister(0x00, &data, 1);
                    if (data == 0x6000) { /* TOP_STATUS muss auf Power-up Complete sein */
                        printf("PACK%u: PB7170 gefunden, initialisiere...\n", g_PackPdoData[curId].id);

                        /* Safe Mode */
                        spi_AFEWriteRegister(0x13, 0); // Alle MOSFETs aus
                        spi_AFEWriteRegister(0x0c, 0); // Alle Balancer aus

                        g_PackPdoData[curId].stateMachine = AFE_STATE_INIT;
                    }
                    break;
                case AFE_STATE_INIT:
                    spi_AFEWriteRegister(0x45,0x95); // USER Unlock
                    if (AFEInit(curId) == 0) {
                        spi_AFEWriteRegister(0x45,0x00); // USER lock
                        spi_AFEWriteRegister(0x05,0x4000); //Clear RESET Flag
                        spi_AFEWriteRegister(0x0d,31); //Setup Balancer

                        printf("PACK%u: Userconfig erfolgreich geschrieben\n", g_PackPdoData[curId].id);
                        g_PackPdoData[curId].stateMachine = AFE_STATE_CONFIG;
                    } else {
                        printf("PACK%u: Fehler beim Schreiben der Userconfig\n", g_PackPdoData[curId].id);
                        g_PackPdoData[curId].stateMachine = AFE_STATE_ERROR;
                        break;
                    }
                    
                    

                    break;
                case AFE_STATE_CONFIG:
                    g_PackPdoData[curId].stateMachine = AFE_STATE_RUN;
                    break;
                case AFE_STATE_RUN:
                    //int altval = g_PackPdoData[curId].hwStatus_bits.SCH_CNT;
                    AFEReadData(curId);
                    //int newval = g_PackPdoData[curId].hwStatus_bits.SCH_CNT;
                    //if((curId==0) && (CyclicDelta(newval,altval) != 4))
                    //    printf("PACK%u: SCH_CNT=%i (d=%i) CELL0=%f CURRENT=%f\n",curId,g_PackPdoData[curId].hwStatus_bits.SCH_CNT,CyclicDelta(newval,altval),g_PackPdoData[curId].cells[0],g_PackPdoData[curId].current);

                    //printf("PACK%u: V=%.3fV I=%.3fA T=%.1fC stateOfCharge=%.1f%%\n", battery_pdo_data[curId].id, battery_pdo_data[curId].voltage, battery_pdo_data[curId].current, battery_pdo_data[curId].dieTemperature, battery_pdo_data[curId].stateOfCharge);
                    
                    // Normalbetrieb
                    break;
                case AFE_STATE_ERROR:
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
