#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#include "dataobjects.h"

// SHMEM Objekt
BATTERY_PDO_t battery_pdo_data[MAX_BATTERY_PACKS];

// Globale Arrays
BATTERY_USERCONF_BLOB_t* battery_userconfig_blob[MAX_BATTERY_PACKS];
BATTERY_GENERALCONF_t*   battery_generalconfig_blob[MAX_BATTERY_PACKS];
BATTERY_CALIBRATION_t*   battery_calibration[MAX_BATTERY_PACKS];
uint16_t battery_enabled = 0;

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

    // Prüfung für Typen mit fixer Größe (z.B. calibration / generalconf)
    if (require_exact_size && (size_t)st.st_size != element_size) {
        printf("⚠️  '%s' hat falsche Größe: erwartet %zu, ist %ld Bytes\n",
               filename, element_size, (long)st.st_size);
        return NULL;
    }

    // Prüfung für Typen mit variabler Anzahl (nur userconf)
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

    // --- Terminierungsprüfung (nur für userconf)
    if (check_terminator) {
        BATTERY_USERCONF_BLOB_t* arr = (BATTERY_USERCONF_BLOB_t*)buf;
        BATTERY_USERCONF_BLOB_t last = arr[num_elements - 1];
        if (last.address != 0 || last.data != 0) {
            free(buf);
            return NULL;
        }
    }

    // --- Duplikaterkennung
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

void load_battery_all_configs(void)
{
    char filename[64];
    battery_enabled = 0;  // Alle Packs zunächst deaktivieren

    for (int i = 0; i < MAX_BATTERY_PACKS; i++) {
        battery_pdo_data[i].ID = i;
        battery_pdo_data[i].Statemachine = PB7170_STATE_DISABLED;

        // =========================
        // 1️⃣ Prüfen, ob alle 3 Dateien existieren
        // =========================
        struct stat st_user, st_gen, st_cal;

        snprintf(filename, sizeof(filename), "pack%d_userconf.bin", i);
        if (stat(filename, &st_user) != 0) continue;

        snprintf(filename, sizeof(filename), "pack%d_generalconf.bin", i);
        if (stat(filename, &st_gen) != 0) continue;

        snprintf(filename, sizeof(filename), "pack%d_calibration.bin", i);
        if (stat(filename, &st_cal) != 0) continue;

        // =========================
        // 2️⃣ Alle Dateien existieren → jetzt einlesen
        // =========================
        int duplicate_of;

        // --- USERCONF
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
        if (!battery_userconfig_blob[i]) continue;  // fehlgeschlagen, Pack überspringen
        if (duplicate_of >= 0)
            printf("🔁 pack%d_userconf.bin identisch mit pack%d_userconf.bin\n", i, duplicate_of);
        else
            printf("✅ pack%d_userconf.bin geladen\n", i);

        // --- GENERALCONF
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
        if (duplicate_of >= 0)
            printf("🔁 pack%d_generalconf.bin identisch mit pack%d_generalconf.bin\n", i, duplicate_of);
        else
            printf("✅ pack%d_generalconf.bin geladen\n", i);

        // --- CALIBRATION
        snprintf(filename, sizeof(filename), "pack%d_calibration.bin", i);
        duplicate_of = -1;
        battery_calibration[i] = load_binary_file(
            filename,
            sizeof(BATTERY_CALIBRATION_t),
            &duplicate_of,
            (void**)battery_calibration,
            i,
            0,
            1
        );
        if (!battery_calibration[i]) continue;
        if (duplicate_of >= 0)
            printf("🔁 pack%d_calibration.bin identisch mit pack%d_calibration.bin\n", i, duplicate_of);
        else
            printf("✅ pack%d_calibration.bin geladen\n", i);

        // =========================
        // 3️⃣ Pack ist vollständig geladen → aktiv setzen
        // =========================
        battery_enabled |= (1 << i);
        battery_pdo_data[i].Statemachine = PB7170_STATE_WAIT_INIT;
    }

    // =========================
    // 4️⃣ Zusammenfassung aller aktiven Packs
    // =========================
    printf("🔋 battery_enabled = 0x%04X\n", battery_enabled);
}
