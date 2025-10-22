// 16-cell LiFePO4 BMS (EVE MB31 tuned parameters)
// - EKF per cell (states: SOC, V_RC)
// - SOH (capacity & resistance) per cell, float cycle counting
// - passive shunt balancing (starts above BALANCE_START_V)
// - save/load state to binary file
//
// Tuned defaults for EVE MB31:
//   C_nom = 314 Ah
//   R0    = 0.00018 Ohm (0.18 mΩ)
//   R1    = 0.002 Ohm (example starting value)
//   C1    = 2000.0 F (example starting value)
//   Q[0][0] = 1e-7  (SOC process noise)
//   Q[1][1] = 1e-5  (V_RC process noise)
//   R(meas) = (5 mV)^2 = 2.5e-5 V^2
//
// NOTE:
// - R1 and C1 should ideally be identified from pulse tests for accurate transients.
// - After first deployment fine-tune Q and R to get desired responsiveness vs stability.
// - For embedded target, replace FILE I/O with EEPROM/Flash primitives.

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define NUM_CELLS 16
#define TABLE_SIZE 11
#define DT 0.25f
#define SMOOTH_ALPHA 0.1f
#define BALANCE_DELTA 0.02f     // 20 mV threshold above target
#define BALANCE_START_V 3.35f   // start balancing near top-region (adjustable)
#define SAVE_FILE "bms_state.bin"

typedef struct {
    float SOC;         // [%]
    float V_RC;        // [V]
    float Ah_acc;      // [Ah] accumulated in current (sub-)cycle
    float SOC_prev;    // previous SOC for delta calculation
    float cycle_count; // float cycles (fractional)
    float SOH_cap;     // [%] smoothed capacity SOH
    float SOH_res;     // [%] resistance-based SOH
    int   shunt_on;    // balancing flag
} Cell_State;

typedef struct {
    Cell_State cells[NUM_CELLS];

    // pack-level known parameters (per cell basis)
    float C_nom;       // [Ah]
    float R0;          // [Ohm] series internal resistance (per cell)
    float R1;          // [Ohm] RC resistor (per cell)
    float C1;          // [F] RC capacitor
    float R0_nominal;  // [Ohm] reference R0 for SOH_res calculation

    // OCV curve (per cell)
    float SOC_table[TABLE_SIZE];
    float OCV_table[TABLE_SIZE];

    // EKF covariance per cell (2x2), process & measurement noise
    float P[NUM_CELLS][2][2];
    float Q[2][2];
    float R; // measurement noise (variance)
} BMS_Pack;


// ---------------- OCV interpolation (linear) ----------------
static float interpolate_OCV(float SOC, BMS_Pack* bms) {
    if (SOC <= bms->SOC_table[0]) return bms->OCV_table[0];
    if (SOC >= bms->SOC_table[TABLE_SIZE-1]) return bms->OCV_table[TABLE_SIZE-1];
    for (int i = 0; i < TABLE_SIZE-1; ++i) {
        if (SOC >= bms->SOC_table[i] && SOC <= bms->SOC_table[i+1]) {
            float soc1 = bms->SOC_table[i], soc2 = bms->SOC_table[i+1];
            float v1 = bms->OCV_table[i], v2 = bms->OCV_table[i+1];
            return v1 + (v2 - v1) * (SOC - soc1) / (soc2 - soc1);
        }
    }
    return bms->OCV_table[0];
}


// ---------------- EKF update for one cell ----------------
static void EKF_Cell_Update(BMS_Pack* bms, int idx, float I, float V_meas) {
    Cell_State* c = &bms->cells[idx];

    // --- 1) Predict state ---
    float SOC_pred = c->SOC - (I * DT) / (bms->C_nom * 3600.0f) * 100.0f; // %SOC
    float tau = bms->R1 * bms->C1;
    float exp_term = expf(-DT / tau);
    float V_RC_pred = c->V_RC * exp_term + bms->R1 * (1.0f - exp_term) * I;

    // --- 2) Linearized state transition F (approx) ---
    float F00 = 1.0f;
    float F01 = 0.0f;
    float F10 = 0.0f;
    float F11 = exp_term;

    // --- 3) Predict covariance P = F * P * F^T + Q ---
    float P00 = F00 * bms->P[idx][0][0] * F00 + bms->Q[0][0];
    float P01 = F00 * bms->P[idx][0][1] * F11 + bms->Q[0][1];
    float P10 = F11 * bms->P[idx][1][0] * F00 + bms->Q[1][0];
    float P11 = F11 * bms->P[idx][1][1] * F11 + bms->Q[1][1];

    // --- 4) Measurement prediction ---
    float V_OCV = interpolate_OCV(SOC_pred, bms);
    float V_pred = V_OCV - I * bms->R0 - V_RC_pred;

    // --- 5) Measurement jacobian H = [dV/dSOC, dV/dV_RC] ---
    // numerical derivative for dV/dSOC
    float dSOC = 0.01f; // 0.01% step (small)
    float V_ocv_plus = interpolate_OCV(SOC_pred + dSOC, bms);
    float dVdSOC = (V_ocv_plus - V_OCV) / dSOC; // [V per %SOC]
    float H0 = dVdSOC;
    float H1 = -1.0f;

    // --- 6) Innovation covariance S = H*P_pred*H^T + R ---
    float S = H0 * (P00 * H0 + P01 * H1) + H1 * (P10 * H0 + P11 * H1) + bms->R;
    if (S < 1e-12f) S = 1e-12f; // numerical safety

    // --- 7) Kalman gain K = P_pred * H^T / S ---
    float K0 = (P00 * H0 + P01 * H1) / S;
    float K1 = (P10 * H0 + P11 * H1) / S;

    // --- 8) Update states with measurement ---
    float y = V_meas - V_pred;
    c->SOC  = SOC_pred  + K0 * y;
    c->V_RC = V_RC_pred + K1 * y;

    // clip SOC
    if (c->SOC > 100.0f) c->SOC = 100.0f;
    if (c->SOC < 0.0f)   c->SOC = 0.0f;

    // --- 9) Update covariance P = (I - K*H) * P_pred ---
    float KH00 = K0 * H0, KH01 = K0 * H1;
    float KH10 = K1 * H0, KH11 = K1 * H1;
    float I_KH00 = 1.0f - KH00, I_KH01 = -KH01;
    float I_KH10 = -KH10, I_KH11 = 1.0f - KH11;

    float P_new00 = I_KH00 * P00 + I_KH01 * P10;
    float P_new01 = I_KH00 * P01 + I_KH01 * P11;
    float P_new10 = I_KH10 * P00 + I_KH11 * P10;
    float P_new11 = I_KH10 * P01 + I_KH11 * P11;

    bms->P[idx][0][0] = P_new00;
    bms->P[idx][0][1] = P_new01;
    bms->P[idx][1][0] = P_new10;
    bms->P[idx][1][1] = P_new11;

    // --- 10) SOH capacity update (use integrated Ah & cycle fraction) ---
    float delta_Ah = fabsf(I * DT) / 3600.0f; // Ah added for this dt
    c->Ah_acc += delta_Ah;
    float delta_SOC_abs = fabsf(c->SOC - c->SOC_prev);
    float cycle_fraction = delta_SOC_abs / 100.0f;
    c->cycle_count += cycle_fraction;

    if (c->Ah_acc > 0.0f) {
        // instant capacity estimate: Ah_nom * cycle_fraction / Ah_measured
        float soh_instant = 100.0f * (bms->C_nom * cycle_fraction) / c->Ah_acc;
        // smooth
        c->SOH_cap = SMOOTH_ALPHA * soh_instant + (1.0f - SMOOTH_ALPHA) * c->SOH_cap;
    }

    c->SOC_prev = c->SOC;

    // resistance-based SOH
    if (bms->R0_nominal > 0.0f && bms->R0 > 0.0f)
        c->SOH_res = 100.0f * (bms->R0_nominal / bms->R0);
    else
        c->SOH_res = 100.0f;
}


// ---------------- Pack update (iterate all cells) ----------------
static void BMS_Pack_Update(BMS_Pack* pack, float I_total, float V_meas_array[NUM_CELLS]) {
    // Assuming series-connected cells: same pack current flows through all cells
    for (int i = 0; i < NUM_CELLS; ++i) {
        EKF_Cell_Update(pack, i, I_total, V_meas_array[i]);
    }
}


// ---------------- Pack-level helpers ----------------
static float Pack_Average_SOC(BMS_Pack* pack) {
    float sum = 0.0f;
    for (int i = 0; i < NUM_CELLS; ++i) sum += pack->cells[i].SOC;
    return sum / (float)NUM_CELLS;
}
static float Pack_Average_SOH(BMS_Pack* pack) {
    float sum = 0.0f;
    for (int i = 0; i < NUM_CELLS; ++i) sum += pack->cells[i].SOH_cap;
    return sum / (float)NUM_CELLS;
}


// ---------------- Balancing: passive shunt strategy ----------------
static void BMS_Balance(BMS_Pack* pack) {
    float Vcells[NUM_CELLS];
    for (int i = 0; i < NUM_CELLS; ++i) {
        // Map SOC -> approximate terminal voltage via OCV table (simple)
        Vcells[i] = pack->cells[i].SOC / 100.0f * (pack->OCV_table[TABLE_SIZE-1] - pack->OCV_table[0]) + pack->OCV_table[0];
    }

    // find top 3 cell voltages (selection sort partial)
    for (int i = 0; i < 3; ++i) {
        int max_idx = i;
        for (int j = i + 1; j < NUM_CELLS; ++j) {
            if (Vcells[j] > Vcells[max_idx]) max_idx = j;
        }
        // swap
        float tmp = Vcells[i];
        Vcells[i] = Vcells[max_idx];
        Vcells[max_idx] = tmp;
    }

    float V_target = (Vcells[0] + Vcells[1] + Vcells[2]) / 3.0f;

    // apply balancing rule: only if Vcell >= BALANCE_START_V and above target + delta
    for (int i = 0; i < NUM_CELLS; ++i) {
        float Vcell = pack->cells[i].SOC / 100.0f * (pack->OCV_table[TABLE_SIZE-1] - pack->OCV_table[0]) + pack->OCV_table[0];
        if (Vcell >= BALANCE_START_V && Vcell > V_target + BALANCE_DELTA)
            pack->cells[i].shunt_on = 1;
        else
            pack->cells[i].shunt_on = 0;
    }
}

static void Print_Balancing(BMS_Pack* pack) {
    for (int i = 0; i < NUM_CELLS; ++i) {
        printf("Cell %02d: SOC=%6.2f %% | SOH_cap=%6.2f %% | shunt=%d\n",
               i + 1, pack->cells[i].SOC, pack->cells[i].SOH_cap, pack->cells[i].shunt_on);
    }
}


// ---------------- Persistence (binary save/load) ----------------
static int BMS_Save(BMS_Pack* pack) {
    FILE* f = fopen(SAVE_FILE, "wb");
    if (!f) return -1;
    size_t wrote = fwrite(pack, sizeof(BMS_Pack), 1, f);
    fclose(f);
    return (wrote == 1) ? 0 : -2;
}

static int BMS_Load(BMS_Pack* pack) {
    FILE* f = fopen(SAVE_FILE, "rb");
    if (!f) return -1;
    size_t read = fread(pack, sizeof(BMS_Pack), 1, f);
    fclose(f);
    return (read == 1) ? 0 : -2;
}


// ---------------- Example main (simulation-ish loop) ----------------
int main(void) {
    BMS_Pack pack;

    // Try to load saved state; otherwise initialize with MB31 defaults
    if (BMS_Load(&pack) == 0) {
        printf("BMS state loaded from " SAVE_FILE ".\n");
    } else {
        printf("No saved state: initializing BMS with MB31 defaults.\n");

        // MB31 specific base parameters
        pack.C_nom      = 314.0f;     // Ah
        pack.R0         = 0.00018f;   // 0.18 mOhm
        pack.R1         = 0.002f;     // example start value (Ohm)
        pack.C1         = 2000.0f;    // example start value (F)
        pack.R0_nominal = 0.00018f;   // use same as initial nominal

        // OCV / SOC reference table (per-cell)
        float soc_table[TABLE_SIZE] = { 0.0f,10.0f,20.0f,30.0f,40.0f,50.0f,60.0f,70.0f,80.0f,90.0f,100.0f };
        float ocv_table[TABLE_SIZE] = { 2.50f,3.00f,3.20f,3.22f,3.25f,3.26f,3.27f,3.30f,3.32f,3.35f,3.40f };
        for (int i = 0; i < TABLE_SIZE; ++i) {
            pack.SOC_table[i] = soc_table[i];
            pack.OCV_table[i] = ocv_table[i];
        }

        // EKF noise and initial covariances tuned for MB31 (starting guesses)
        pack.Q[0][0] = 1e-7f; pack.Q[0][1] = 0.0f;
        pack.Q[1][0] = 0.0f;  pack.Q[1][1] = 1e-5f;
        pack.R = 2.5e-5f; // (5 mV)^2

        // initialize per-cell states and covariances
        for (int i = 0; i < NUM_CELLS; ++i) {
            pack.cells[i].SOC = 80.0f;       // initial guess
            pack.cells[i].V_RC = 0.0f;
            pack.cells[i].Ah_acc = 0.0f;
            pack.cells[i].SOC_prev = pack.cells[i].SOC;
            pack.cells[i].cycle_count = 0.0f;
            pack.cells[i].SOH_cap = 100.0f;
            pack.cells[i].SOH_res = 100.0f;
            pack.cells[i].shunt_on = 0;

            // initial P (uncertainty)
            pack.P[i][0][0] = 0.01f; pack.P[i][0][1] = 0.0f;
            pack.P[i][1][0] = 0.0f;  pack.P[i][1][1] = 0.01f;
        }
    }

    // Example: run an update loop with simulated inputs
    // Replace with actual sensor readings in embedded deployment:
    float I_total = -100.0f; // example discharge current (A), negative => discharging by convention used earlier
    float V_meas[NUM_CELLS];
    for (int i = 0; i < NUM_CELLS; ++i) V_meas[i] = 3.27f; // example per-cell measured voltages

    // run for N steps as example
    for (int t = 0; t < 200; ++t) {
        // In practice: read cell temperatures, cell voltages (V_meas[]), coulomb counter current and pack current I_total
        // Here we reuse I_total and V_meas[] as constants for demo.

        BMS_Pack_Update(&pack, I_total, V_meas);
        BMS_Balance(&pack);

        // periodically save state (every 10s here: DT=0.25 => 40 steps)
        if ((t % 40) == 0) {
            if (BMS_Save(&pack) == 0) {
                printf("State saved (t=%.2f s)\n", t * DT);
            } else {
                printf("Warning: state save failed\n");
            }
        }

        // print pack summary
        printf("t=%.2f s | Pack SOC=%.2f %% | Pack SOH=%.2f %%\n", t * DT, Pack_Average_SOC(&pack), Pack_Average_SOH(&pack));
        Print_Balancing(&pack);
    }

    return 0;
}
