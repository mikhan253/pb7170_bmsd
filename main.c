#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/timerfd.h>
#include <sched.h>

#ifdef TIME_IT
#include <time.h>
#endif

#include "globalconst.h"
#include "spi.h"
#include "bms.h"
#include "dataobjects.h"


// Globale Variable für kontrollierte Beendigung
static volatile sig_atomic_t g_shutdownRequest = 0;
static int g_timerFd = -1;

// Signal-Handler-Funktion
static void SignalHandler(int sig) {
    printf("!!! Signal %d - Herunterfahren erzwingen...\n", sig);
    g_shutdownRequest = 1;
}

static int SetupTask(int cycletimeMs) {
    /************** Setup Priority **************/
    struct sched_param sp;
    sp.sched_priority = 20; // Wertebereich: 1–99 (höher = wichtiger)
    if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0)
        return -1;

    /************** Setup Timer **************/
    g_timerFd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (g_timerFd < 0)
        return -2;
    struct itimerspec period = {
        .it_interval = {0, cycletimeMs * 1000000},
        .it_value = {0, cycletimeMs * 1000000}
    };
    if (timerfd_settime(g_timerFd, 0, &period, NULL))
        return -3;
    return 0;
}

// Setup der Signal-Handler
static int SetupSignalHandlers(void) {
    struct sigaction sa = {
        .sa_handler = SignalHandler,
        .sa_flags = 0
    };
    
    // SIGINT (Ctrl+C) blockieren während der Handler läuft
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGTERM);
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Failed to set up SIGINT handler");
        return -1;
    }
    
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("Failed to set up SIGTERM handler");
        return -1;
    }
    
    // SIGQUIT (Ctrl+\) ignorieren oder behandeln
    sa.sa_handler = SignalHandler; // oder SIG_IGN um zu ignorieren
    if (sigaction(SIGQUIT, &sa, NULL) == -1) {
        perror("Failed to set up SIGQUIT handler");
        return -1;
    }
    
    return 0;
}

// Ressourcen-Cleanup Funktion
static void CleanupResources(void) {
    spi_Cleanup();
    dob_Cleanup();
    if (g_timerFd >= 0) {
        close(g_timerFd);
    }
}

int main(void) {
    uint64_t timerExpirations;
    
    printf("PB7170 BMS Controller (Build: %s %s)\n", __DATE__, __TIME__);
    
    // Signal-Handler setup
    if (SetupSignalHandlers()) {
        printf("Failed to set up signal handlers\n");
        return 1;
    }
    
    // Task Initialisierung
    if (SetupTask(CYCLE_TIME_MS)) {
        printf("Failed to set up task\n");
        return 1;
    }
    
    // SPI Initialisierung
    const unsigned int GPIO_PINS[3] = {24, 25, 26};
    if (spi_Init("/dev/spidev0.0", 1250000, 0, 8, "/dev/gpiochip1", GPIO_PINS, 3)) {
        printf("Failed to initialize SPI\n");
        CleanupResources();
        return 1;
    }
    
    // Konfiguration laden
    if (dob_LoadPackConfigs()) {
        printf("Failed to initialize configurations\n");
        CleanupResources();
        return 1;
    }
    
    printf("--- Starte Task mit %u Packs ---\n", g_GlobalConfig.numberOfPacks);
    
    // Hauptschleife
    while (!g_shutdownRequest) {
        ssize_t bytesRead = read(g_timerFd, &timerExpirations, sizeof(timerExpirations));
        
        if (bytesRead != sizeof(timerExpirations)) {
            if (g_shutdownRequest) {
                // Bei Shutdown ist das normal
                break;
            }
            printf("Error reading timer: %zd bytes\n", bytesRead);
            continue;
        }
        
        if (timerExpirations != 1) {
            printf("Timer expired %llu times\n", (unsigned long long)timerExpirations);
        }
        
#ifdef TIME_IT
        struct timespec t_start, t_end;
        clock_gettime(CLOCK_MONOTONIC, &t_start);
#endif
        
        // BMS Aufgaben für alle aktiven Packs
        g_GlobalPdoData->sync = 0;
        for (uint32_t curId = 0; curId < g_GlobalConfig.numberOfPacks && !g_shutdownRequest; curId++) {
            if ((g_packEnabled & (1 << curId)) == 0) {
                continue;
            }
            
            spi_SelectDevice(curId);
            bms_CyclicTask(curId);
            // CYCLE_TASK
        }
        g_GlobalPdoData->sync = 1;
        
#ifdef TIME_IT
        if (clock_gettime(CLOCK_MONOTONIC, &t_end) == 0) {
            double elapsed_ms = (t_end.tv_sec - t_start.tv_sec) * 1e3 + (t_end.tv_nsec - t_start.tv_nsec) / 1e6;
            printf("loop duration: %.3f ms\n", elapsed_ms);
        }
#endif
    }
    
    // Kontrolliertes Herunterfahren
    printf("BMS Controller herunterfahren... ");
    CleanupResources();
    printf("OK\n");
    
    return 0;
}