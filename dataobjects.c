#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>      // für shm_open
#include <sys/mman.h>   // für mmap
#include <unistd.h>     // für ftruncate
#include "dataobjects.h"

// SHMEM Objekt
PACK_PDO_t* g_PackPdoData = NULL;
PACK_SDO_t* g_PackSdoData = NULL;

// Globale Arrays
PACK_USERCONF_t* g_PackUserConfig[MAX_BATTERY_PACKS];
PACK_GENERALCONF_t* g_PackGeneralConfig[MAX_BATTERY_PACKS];
PACK_CALIBRATION_t* g_PackCalibration[MAX_BATTERY_PACKS];
uint16_t g_packEnabled = 0;

// SHMEM Konfiguration


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
        printf("'%s' hat falsche Größe: erwartet %zu, ist %ld Bytes\n",
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
        printf("- %s identisch mit pack%d\n", filename, duplicate_of);
    else
        printf("- %s geladen\n", filename);
}

// ---------------------------------------------------------
// SHMEM initialisieren
static int InitShmem(void* target, char* name, size_t size) {
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

    target = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (target == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return -1;
    }

    memset(target, 0, size); // Speicher initialisieren
    printf("Shared Memory: %s (size=%u)\n",name, size);
    close(shm_fd);
    return 0;
}

void dob_LoadPackConfigs(void)
{
    if (InitShmem(g_PackPdoData, "/battery_pdo_shm", sizeof(PACK_PDO_t) * MAX_BATTERY_PACKS) != 0) {
        printf("Fehler: SHMEM_PDO konnte nicht initialisiert werden.\n");
        return;
    }
    if (InitShmem(g_PackSdoData, "/battery_sdo_shm", sizeof(PACK_SDO_t) * MAX_BATTERY_PACKS) != 0) {
        printf("Fehler: SHMEM_SDO konnte nicht initialisiert werden.\n");
        return;
    }

    char filename[64];
    g_packEnabled = 0;

    printf("Lade Konfigurationsdateien...\n");
    for (int i = 0; i < MAX_BATTERY_PACKS; i++) {
        g_PackPdoData[i].stateMachine = AFE_STATE_DISABLED;

        struct stat st;
        snprintf(filename, sizeof(filename), "conf/pack%d_userconf.bin", i);
        if (stat(filename, &st) != 0) continue;

        snprintf(filename, sizeof(filename), "conf/pack%d_generalconf.bin", i);
        if (stat(filename, &st) != 0) continue;

        snprintf(filename, sizeof(filename), "conf/pack%d_calibration.bin", i);
        if (stat(filename, &st) != 0) continue;

        int duplicate_of;

        snprintf(filename, sizeof(filename), "conf/pack%d_userconf.bin", i);
        duplicate_of = -1;
        g_PackUserConfig[i] = LoadBinaryFile(
            filename,
            sizeof(PACK_USERCONF_t),
            &duplicate_of,
            (void**)g_PackUserConfig,
            i,
            1,
            0
        );
        if (!g_PackUserConfig[i]) continue;
        PrintLoadState(filename, duplicate_of);

        snprintf(filename, sizeof(filename), "conf/pack%d_generalconf.bin", i);
        duplicate_of = -1;
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

        snprintf(filename, sizeof(filename), "conf/pack%d_calibration.bin", i);
        duplicate_of = -1;
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
        g_PackPdoData[i].id = i + 1;
        g_packEnabled |= (1 << i);
        g_PackPdoData[i].stateMachine = AFE_STATE_WAIT_INIT;
    }

    for (int i = 0; i < MAX_BATTERY_PACKS; i++)
        if (g_packEnabled & (1 << i)) 
            printf("Pack %d aktiviert\n", i);
}
