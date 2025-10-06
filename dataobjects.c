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
BATTERY_PDO_t* battery_pdo_data = NULL;

// Globale Arrays
BATTERY_USERCONF_BLOB_t* battery_userconfig_blob[MAX_BATTERY_PACKS];
BATTERY_GENERALCONF_t*   battery_generalconfig_blob[MAX_BATTERY_PACKS];
BATTERY_CALIBRATION_t*   battery_calibration_blob[MAX_BATTERY_PACKS];
uint16_t battery_enabled = 0;

// SHMEM Konfiguration
#define SHM_NAME "/battery_pdo_shm"

// ---------------------------------------------------------
// generische Datei-Ladefunktion
static void* load_binary_file(
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
        BATTERY_USERCONF_BLOB_t* arr = (BATTERY_USERCONF_BLOB_t*)buf;
        BATTERY_USERCONF_BLOB_t last = arr[num_elements - 1];
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
static void print_load_status(const char* filename, int duplicate_of) {
    if (duplicate_of >= 0)
        printf("- %s identisch mit pack%d\n", filename, duplicate_of);
    else
        printf("- %s geladen\n", filename);
}

// ---------------------------------------------------------
// SHMEM initialisieren
static int init_shmem(void) {
    shm_unlink(SHM_NAME); // lösche altes Objekt

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd < 0) {
        perror("shm_open");
        return -1;
    }

    if (ftruncate(shm_fd, sizeof(BATTERY_PDO_t) * MAX_BATTERY_PACKS) != 0) {
        perror("ftruncate");
        close(shm_fd);
        return -1;
    }

    battery_pdo_data = mmap(NULL, sizeof(BATTERY_PDO_t) * MAX_BATTERY_PACKS,
                            PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (battery_pdo_data == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return -1;
    }

    memset(battery_pdo_data, 0, sizeof(BATTERY_PDO_t) * MAX_BATTERY_PACKS); // Speicher initialisieren
    close(shm_fd);
    return 0;
}

void load_battery_all_configs(void)
{
    if (init_shmem() != 0) {
        printf("Fehler: SHMEM konnte nicht initialisiert werden.\n");
        return;
    }

    char filename[64];
    battery_enabled = 0;

    printf("Lade Konfigurationsdateien...\n");
    for (int i = 0; i < MAX_BATTERY_PACKS; i++) {
        battery_pdo_data[i].ID = i;
        battery_pdo_data[i].Statemachine = PB7170_STATE_DISABLED;

        struct stat st;
        snprintf(filename, sizeof(filename), "pack%d_userconf.bin", i);
        if (stat(filename, &st) != 0) continue;

        snprintf(filename, sizeof(filename), "pack%d_generalconf.bin", i);
        if (stat(filename, &st) != 0) continue;

        snprintf(filename, sizeof(filename), "pack%d_calibration.bin", i);
        if (stat(filename, &st) != 0) continue;

        int duplicate_of;

        snprintf(filename, sizeof(filename), "pack%d_userconf.bin", i);
        duplicate_of = -1;
        battery_userconfig_blob[i] = load_binary_file(
            filename,
            sizeof(BATTERY_USERCONF_BLOB_t),
            &duplicate_of,
            (void**)battery_userconfig_blob,
            i,
            1,
            0
        );
        if (!battery_userconfig_blob[i]) continue;
        print_load_status(filename, duplicate_of);

        snprintf(filename, sizeof(filename), "pack%d_generalconf.bin", i);
        duplicate_of = -1;
        battery_generalconfig_blob[i] = load_binary_file(
            filename,
            sizeof(BATTERY_GENERALCONF_t),
            &duplicate_of,
            (void**)battery_generalconfig_blob,
            i,
            0,
            1
        );
        if (!battery_generalconfig_blob[i]) continue;
        print_load_status(filename, duplicate_of);

        snprintf(filename, sizeof(filename), "pack%d_calibration.bin", i);
        duplicate_of = -1;
        battery_calibration_blob[i] = load_binary_file(
            filename,
            sizeof(BATTERY_CALIBRATION_t),
            &duplicate_of,
            (void**)battery_calibration_blob,
            i,
            0,
            1
        );
        if (!battery_calibration_blob[i]) continue;
        print_load_status(filename, duplicate_of);

        battery_enabled |= (1 << i);
        battery_pdo_data[i].Statemachine = PB7170_STATE_WAIT_INIT;
    }

    for (int i = 0; i < MAX_BATTERY_PACKS; i++)
        if (battery_enabled & (1 << i)) 
            printf("Pack %d aktiviert\n", i);
}
