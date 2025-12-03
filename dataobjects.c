#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>      // für shm_open
#include <sys/mman.h>   // für mmap
#include <unistd.h>     // für ftruncate
#include <syslog.h>
#include "dataobjects.h"

#define SHMEM_BATTERYPDO "/battery_pdo_shm"
#define SHMEM_BATTERYSDO "/battery_sdo_shm"

// SHMEM Objekte
GLOBAL_PDO_t* g_GlobalPdoData = NULL;
PACK_PDO_t* g_PackPdoData = NULL;
PACK_SDO_t* g_PackSdoData = NULL;

// Globale Arrays
GLOBAL_CONF_t g_GlobalConfig;
PACK_USERCONF_t* g_PackUserConfig[MAX_BATTERY_PACKS];
PACK_GENERALCONF_t* g_PackGeneralConfig[MAX_BATTERY_PACKS];
PACK_CALIBRATION_t* g_PackCalibration[MAX_BATTERY_PACKS];
uint16_t g_packEnabled = 0;

// ---------------------------------------------------------
// generische Datei-Ladefunktion
static void* LoadBinaryFile(
    const char* filename,
    size_t element_size,
    int* is_duplicate,
    void* existing_ptrs[MAX_BATTERY_PACKS],
    int count,
    int check_terminator,
    int require_exact_size)
{
    struct stat st;
    if (stat(filename, &st) != 0)
        return NULL;

    if (st.st_size <= 0)
        return NULL;

    if (require_exact_size && (size_t)st.st_size != element_size) {
        syslog(LOG_ERR, "'%s' hat falsche Größe: erwartet %zu, ist %ld Bytes",
               filename, element_size, (long)st.st_size);
        return NULL;
    }

    if (!require_exact_size && (st.st_size % element_size) != 0)
        return NULL;

    size_t num_elements = st.st_size / element_size;

    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;

    void* buf = malloc(st.st_size);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t readcount = fread(buf, element_size, num_elements, f);
    fclose(f);

    if (readcount != num_elements) {
        free(buf);
        return NULL;
    }

    if (check_terminator) {
        PACK_USERCONF_t* arr = (PACK_USERCONF_t*)buf;
        PACK_USERCONF_t last = arr[num_elements - 1];
        if (last.address != 0 || last.data != 0) {
            free(buf);
            return NULL;
        }
    }

    for (int j = 0; j < count; j++) {
        if (existing_ptrs[j] && memcmp(existing_ptrs[j], buf, st.st_size) == 0) {
            *is_duplicate = j;
            free(buf);
            return existing_ptrs[j];
        }
    }

    *is_duplicate = -1;
    return buf;
}

// Helferfunktion für Statusmeldungen
static void PrintLoadState(const char* filename, int duplicate_of) {
    if (duplicate_of >= 0)
        syslog(LOG_INFO, "- %s identisch mit pack%d", filename, duplicate_of);
    else
        syslog(LOG_INFO, "- %s geladen", filename);
}

// ---------------------------------------------------------
// SHMEM initialisieren
static int InitShmem(void** target, const char* name, size_t size) {
    shm_unlink(name); // lösche altes Objekt

    int shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (shm_fd < 0) {
        perror("shm_open");
        return -1;
    }

    if (ftruncate(shm_fd, size) != 0) {
        perror("ftruncate");
        close(shm_fd);
        return -1;
    }

    *target = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (*target == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return -1;
    }

    memset(*target, 0, size); // Speicher initialisieren
    syslog(LOG_INFO, "- ShMem: %s (size=%zu)", name, size);
    close(shm_fd);
    return 0;
}

// ---------------------------------------------------------
int dob_LoadPackConfigs(void)
{
    char filename[64];
    int duplicate_of;

    // -----------------------------------------------------
    // GlobalConfig laden
    syslog(LOG_INFO, "Lade globale Konfigurationsdatei...");
    snprintf(filename, sizeof(filename), "conf/global.bin");
    duplicate_of = -1;
    void* global_buf = LoadBinaryFile(
        filename,
        sizeof(GLOBAL_CONF_t),
        &duplicate_of,
        NULL,
        0,
        0,   // kein Terminator prüfen
        1    // exakte Größe erforderlich
    );
    if (!global_buf) {
        syslog(LOG_ERR, "'%s' konnte nicht geladen werden oder hat falsche Größe.\n", filename);
        return -1;
    }
    g_GlobalConfig = *(GLOBAL_CONF_t*)global_buf;
    free(global_buf);
    syslog(LOG_INFO, "- %s geladen\n", filename);

    // -----------------------------------------------------
    // Shared Memory für PDO: globalPDO + PackPDO
    size_t numPacks = g_GlobalConfig.numberOfPacks;
    if (numPacks == 0 || numPacks > MAX_BATTERY_PACKS) {
        syslog(LOG_ERR, "Ungültige Anzahl Packs in global config: %zu\n", numPacks);
        return -1;
    }

    size_t shmSize = sizeof(GLOBAL_PDO_t) + sizeof(PACK_PDO_t) * numPacks;
    void* shm_ptr = NULL;
    if (InitShmem(&shm_ptr, SHMEM_BATTERYPDO, shmSize) != 0) {
        syslog(LOG_ERR, "SHMEM_PDO konnte nicht initialisiert werden.\n");
        return -1;
    }

    g_GlobalPdoData = (GLOBAL_PDO_t*)shm_ptr;
    g_PackPdoData   = (PACK_PDO_t*)(g_GlobalPdoData + 1);

    // -----------------------------------------------------
    // Shared Memory für SDO
    if (InitShmem((void**)&g_PackSdoData, SHMEM_BATTERYSDO, sizeof(PACK_SDO_t) * numPacks) != 0) {
        syslog(LOG_ERR, "SHMEM_SDO konnte nicht initialisiert werden.\n");
        return -1;
    }

    g_packEnabled = 0;

    syslog(LOG_INFO, "Lade Konfigurationsdateien...\n");
    for (int i = 0; i < numPacks; i++) {
        g_PackPdoData[i].stateMachine = AFE_STATE_DISABLED;

        // UserConfig
        snprintf(filename, sizeof(filename), "conf/pack%d_userconf.bin", i);
        g_PackUserConfig[i] = LoadBinaryFile(
            filename,
            sizeof(PACK_USERCONF_t),
            &duplicate_of,
            (void**)g_PackUserConfig,
            i,
            1, // Terminator prüfen
            0
        );
        if (!g_PackUserConfig[i]) continue;
        PrintLoadState(filename, duplicate_of);

        // GeneralConfig
        snprintf(filename, sizeof(filename), "conf/pack%d_generalconf.bin", i);
        g_PackGeneralConfig[i] = LoadBinaryFile(
            filename,
            sizeof(PACK_GENERALCONF_t),
            &duplicate_of,
            (void**)g_PackGeneralConfig,
            i,
            0,
            1
        );
        if (!g_PackGeneralConfig[i]) continue;
        PrintLoadState(filename, duplicate_of);

        // Calibration
        snprintf(filename, sizeof(filename), "conf/pack%d_calibration.bin", i);
        g_PackCalibration[i] = LoadBinaryFile(
            filename,
            sizeof(PACK_CALIBRATION_t),
            &duplicate_of,
            (void**)g_PackCalibration,
            i,
            0,
            1
        );
        if (!g_PackCalibration[i]) continue;
        PrintLoadState(filename, duplicate_of);

        // Pack aktivieren
        g_PackPdoData[i].id = i + 1;
        g_packEnabled |= (1 << i);
        g_PackPdoData[i].stateMachine = AFE_STATE_WAIT_INIT;
    }

    for (int i = 0; i < numPacks; i++)
        if (g_packEnabled & (1 << i))
            syslog(LOG_INFO, "Pack %d aktiviert\n", i);

    // GlobalPdoData initialisieren
    g_GlobalPdoData->numberOfPacks = g_GlobalConfig.numberOfPacks;
    return 0;
}

void dob_Cleanup() {
    shm_unlink(SHMEM_BATTERYPDO);
    shm_unlink(SHMEM_BATTERYSDO);
}