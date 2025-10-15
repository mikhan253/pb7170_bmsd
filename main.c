#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>

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




int main(void) {
    
    uint64_t timerExpirations;


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
            //CYCLE_TASK

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
