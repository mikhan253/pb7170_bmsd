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

        // Schreibe die Register einzeln
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

int AFEErrorHandler(int id) {
    #define SWALERT g_PackPdoData[id].swAlertFlags_bits
    #define HWALERTFLAGS g_PackPdoData[id].hwAlertFlags_bits
    #define HWALERTSTATE g_PackPdoData[id].hwAlertState_bits
   SWALERT.CHARGE_OC = 
            HWALERTFLAGS.CHARGE_OC;
    SWALERT.DISCHARGE_OC = 
            HWALERTFLAGS.DISCHARGE_OC;
    SWALERT.SHORT = 
            HWALERTFLAGS.SHORT;
    SWALERT.CHIPSTATE_ERR = 
            HWALERTFLAGS.WDT_OVF |
            HWALERTFLAGS.EXT_PROT |
            HWALERTSTATE.RESET |
            HWALERTSTATE.SLEEP |
            HWALERTSTATE.VREF |
            HWALERTSTATE.LVMUX |
            HWALERTSTATE.AVDD |
            HWALERTSTATE.DVDD |
            HWALERTSTATE.EEPROM_CRC_ERR |
            HWALERTSTATE.CLOCK_ABNORMAL;
    SWALERT.OVERTEMP =
            HWALERTFLAGS.THERM_SD |
            HWALERTFLAGS.TDIE_HI;
    SWALERT.UNDERTEMP =
            HWALERTFLAGS.TDIE_LO;
    SWALERT.COMM_ERR =
            HWALERTFLAGS.SPI_CRC_ERR |
            HWALERTSTATE.SPI_CRC_ERR;
    SWALERT.DIAG_ERR =
            HWALERTFLAGS.AUX_OV |
            HWALERTFLAGS.AUX_UV |
            HWALERTFLAGS.LV |
            HWALERTFLAGS.PVDD_UVOV;
    SWALERT.PACK_OV =
            HWALERTFLAGS.PACK_OV;
    SWALERT.PACK_UV =
            HWALERTFLAGS.PACK_UV;
    SWALERT.CELL_OV =
            HWALERTFLAGS.CELL_OV;
    SWALERT.CELL_UV =
            HWALERTFLAGS.CELL_UV;
    SWALERT.CELL_MISMATCH =
            HWALERTFLAGS.MISMATCH;

    #undef SWALERT
    #undef HWALERTFLAGS
    #undef HWALERTSTATE
    return 0;
}

int AFEWireDiag(uint16_t *data)
{
#define KABELBRUCH_MAX_DELTA 200
    for(int i=16; i<=32;i+=16)
        for(int j=0; j<16;j++)
        {
            int32_t delta;
            delta = data[j] - data[i+j];
            if(delta < 0)
                delta = -delta;
            if((delta >= KABELBRUCH_MAX_DELTA))
                return 1; //Kabelbruch erkannt
        }
    return 0;
}

int main(void) {
    uint32_t diagLock = 0;

    uint64_t timerExpirations;
    uint16_t diagData[16 * 3];

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

            switch(g_PackPdoData[curId].stateMachine)
            {
                case AFE_STATE_WAIT_INIT:
                    /*********************************************
                     * Überprüfe, ob AFE erkannt wurde
                     * JA   -> Starte Initialisierung
                     * NEIN -> Warte
                     *********************************************/
                    spi_AFEReadRegister(0x00, &diagData[0], 1);
                    if (diagData[0] == 0x6000) { /* TOP_STATUS muss auf Power-up Complete sein */
                        printf("PACK%u: PB7170 gefunden, initialisiere...\n", g_PackPdoData[curId].id);

                        /* Safe Mode */
                        spi_AFEWriteRegister(0x13, 0); // Alle MOSFETs aus
                        spi_AFEWriteRegister(0x0c, 0); // Alle Balancer aus

                        g_PackPdoData[curId].stateMachine = AFE_STATE_INIT;
                    }
                    break;
                case AFE_STATE_INIT:
                    /*********************************************
                     * Lade Userconfig für das AFE
                     * OK   -> Starte Diagnose
                     * NEIN -> Fehler
                     *********************************************/
                    spi_AFEWriteRegister(0x45,0x95); // USER Unlock
                    if (AFEInit(curId) == 0) {
                        spi_AFEWriteRegister(0x45,0x00); // USER lock
                        spi_AFEWriteRegister(0x05,0x4000); //Clear RESET Flag
                        spi_AFEWriteRegister(0x0d,31); //Setup Balancer

                        printf("PACK%u: Userconfig erfolgreich geschrieben\n", g_PackPdoData[curId].id);
                        g_PackPdoData[curId].stateMachine = AFE_STATE_WAIT_DIAG0;
                    } else {
                        printf("PACK%u: Fehler beim Schreiben der Userconfig\n", g_PackPdoData[curId].id);
                        g_PackPdoData[curId].stateMachine = AFE_STATE_ERROR;
                        break;
                    }
                    break;
                case AFE_STATE_WAIT_DIAG0:
                    /*********************************************
                     * Kabelbruchdiagnose gestartet, warte bis
                     * Werte für Zellen bereit sind, bzw.
                     * Diagnose ist nur eine gleichzeitig erlaubt
                     * OK   -> Ein Zyklus warten
                     * NEIN -> Warten
                     *********************************************/
                    if(diagLock == 0)
                    {
                        diagLock = 1;
                        spi_AFEWriteRegister(0x43,0xa8); //DIAG_Unlock
                        g_PackPdoData[curId].stateMachine = AFE_STATE_DIAG0;
                    }
                    break;
                case AFE_STATE_DIAG0:
                    /*********************************************
                     * Merke Werte für Standard
                     * Setze alle Zellen auf Diagnose-Pullup
                     *********************************************/
                    spi_AFEReadRegister(0x87,&diagData[0],16); //Werte Standard
                    // Set Pull-Up
                    spi_AFEWriteRegister(0x50,0xffff);
                    spi_AFEWriteRegister(0x51,0x0000);
                    spi_AFEWriteRegister(0x52,0x0004);
                    g_PackPdoData[curId].stateMachine = AFE_STATE_WAIT_DIAG1;
                    break;
                case AFE_STATE_WAIT_DIAG1:
                    g_PackPdoData[curId].stateMachine = AFE_STATE_DIAG1;
                    break;
                case AFE_STATE_DIAG1:
                    /*********************************************
                     * Merke Werte für Pullup
                     * Setze alle Zellen auf Diagnose-Pulldown
                     *********************************************/
                    spi_AFEReadRegister(0x87,&diagData[16],16); //Werte Pullup
                    // Set Pull-Down
                    spi_AFEWriteRegister(0x50,0x0000);
                    spi_AFEWriteRegister(0x51,0xffff);
                    spi_AFEWriteRegister(0x52,0x0001);
                    g_PackPdoData[curId].stateMachine = AFE_STATE_WAIT_DIAG2;
                    break;
                case AFE_STATE_WAIT_DIAG2:
                    g_PackPdoData[curId].stateMachine = AFE_STATE_DIAG2;
                    break;
                case AFE_STATE_DIAG2:
                    /*********************************************
                     * Merke Werte für Pulldown
                     * Setze alle Zellen auf Standard
                     * Kabelbrucherkennung
                     *********************************************/
                    spi_AFEReadRegister(0x87,&diagData[32],16); //Werte Pulldown
                    // Disable all
                    spi_AFEWriteRegister(0x50,0x0000);
                    spi_AFEWriteRegister(0x51,0x0000);
                    spi_AFEWriteRegister(0x52,0x0000);

                    spi_AFEWriteRegister(0x43,0x0); //DIAG_Lock                    
                    diagLock = 0;

                    if(AFEWireDiag(diagData))
                    {
                        printf("PACK%u: Diagnose fehlerhaft, deaktiviere Pack\n", g_PackPdoData[curId].id);
                        g_PackPdoData[curId].swAlertFlags_bits.DIAG_ERR = 1;
                        g_PackPdoData[curId].stateMachine = AFE_STATE_ERROR;
                    }
                    else
                    {
                        printf("PACK%u: Diagnose erfolgreich\n", g_PackPdoData[curId].id);
                        g_PackPdoData[curId].stateMachine = AFE_STATE_CONFIG;
                    }
                    break;
                case AFE_STATE_CONFIG:
                    // Lösche alle Fehler
                    spi_AFEWriteRegister(0x02,0xFFFF);
                    spi_AFEWriteRegister(0x03,0xFFFF);
                    spi_AFEWriteRegister(0x05,0xC000);
                    
                    printf("PACK%u: Konfiguration fertig, RUN-Mode\n", g_PackPdoData[curId].id);
                    g_PackPdoData[curId].stateMachine = AFE_STATE_RUN;
                    break;
                case AFE_STATE_SANITY_CHECK:
                    //AFESanityCheck(curId);
                case AFE_STATE_RUN:
                    //int altval = g_PackPdoData[curId].hwStatus_bits.SCH_CNT;
                    AFEReadData(curId);
                    AFEErrorHandler(curId);

                    //int newval = g_PackPdoData[curId].hwStatus_bits.SCH_CNT;
                    //if((curId==0) && (CyclicDelta(newval,altval) != 4))
                    //    printf("PACK%u: SCH_CNT=%i (d=%i) CELL0=%f CURRENT=%f\n",curId,g_PackPdoData[curId].hwStatus_bits.SCH_CNT,CyclicDelta(newval,altval),g_PackPdoData[curId].cells[0],g_PackPdoData[curId].current);

                    //printf("PACK%u: V=%.3fV\n", g_PackPdoData[curId].id, g_PackPdoData[curId].voltage);
                    
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
