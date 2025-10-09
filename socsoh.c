#include <stdio.h>
#include <math.h>
#include <stdlib.h>


bms->R0 = 0.00018f;       // Ohm
bms->R1 = 0.002f;          // Ohm, experimentell
bms->C1 = 2000.0f;         // Farad, experimentell
bms->C_nom = 314.0f;       // Ah
bms->Q[0][0] = 1e-7f;      // SOC Prozessrauschen
bms->Q[1][1] = 1e-5f;      // V_RC Prozessrauschen
bms->R = 2.5e-5f;          // Messrauschen
bms->P[i][0][0] = 0.01f;   // Initialcovarianz SOC
bms->P[i][1][1] = 0.01f;   // Initialcovarianz V_RC





#define NUM_CELLS 16
#define TABLE_SIZE 11
#define DT 0.25f
#define SMOOTH_ALPHA 0.1f
#define BALANCE_DELTA 0.02f  // Volt Differenz für Shunt
#define BALANCE_START_V 3.32f // Volt ab der Balancing startet
#define SAVE_FILE "bms_state.bin"

typedef struct {
    float SOC;       
    float V_RC;      
    float Ah_acc;    
    float SOC_prev;  
    float cycle_count; 
    float SOH_cap;   
    float SOH_res;   
    int shunt_on;    
} Cell_State;

typedef struct {
    Cell_State cells[NUM_CELLS];

    float C_nom;     
    float R0;        
    float R1;        
    float C1;        
    float R0_nominal;

    float SOC_table[TABLE_SIZE];
    float OCV_table[TABLE_SIZE];

    float P[NUM_CELLS][2][2];
    float Q[2][2];
    float R;
} BMS_Pack;

// ---------------- OCV Interpolation ----------------
float interpolate_OCV(float SOC, BMS_Pack* bms){
    if(SOC <= bms->SOC_table[0]) return bms->OCV_table[0];
    if(SOC >= bms->SOC_table[TABLE_SIZE-1]) return bms->OCV_table[TABLE_SIZE-1];
    for(int i=0;i<TABLE_SIZE-1;i++){
        if(SOC>=bms->SOC_table[i] && SOC<=bms->SOC_table[i+1]){
            float soc1 = bms->SOC_table[i], soc2 = bms->SOC_table[i+1];
            float v1 = bms->OCV_table[i], v2 = bms->OCV_table[i+1];
            return v1 + (v2-v1)*(SOC-soc1)/(soc2-soc1);
        }
    }
    return bms->OCV_table[0];
}

// ---------------- EKF Update ----------------
void EKF_Cell_Update(BMS_Pack* bms, int idx, float I, float V_meas){
    Cell_State* c = &bms->cells[idx];

    float SOC_pred = c->SOC - (I*DT)/(bms->C_nom*3600.0f)*100.0f;
    float V_RC_pred = c->V_RC * expf(-DT/(bms->R1*bms->C1)) + bms->R1*(1-expf(-DT/(bms->R1*bms->C1)))*I;

    float F[2][2] = { {1.0f,0.0f},{0.0f,expf(-DT/(bms->R1*bms->C1))} };
    float P_pred[2][2];
    P_pred[0][0] = F[0][0]*bms->P[idx][0][0]*F[0][0] + bms->Q[0][0];
    P_pred[0][1] = F[0][0]*bms->P[idx][0][1]*F[1][1] + bms->Q[0][1];
    P_pred[1][0] = F[1][1]*bms->P[idx][1][0]*F[0][0] + bms->Q[1][0];
    P_pred[1][1] = F[1][1]*bms->P[idx][1][1]*F[1][1] + bms->Q[1][1];

    float V_OCV = interpolate_OCV(SOC_pred,bms);
    float V_pred = V_OCV - I*bms->R0 - V_RC_pred;

    float dVdSOC = (interpolate_OCV(SOC_pred+0.01f,bms)-V_OCV)/0.01f;
    float H[2] = { dVdSOC,-1.0f };
    float S = H[0]*P_pred[0][0]*H[0] + H[0]*P_pred[0][1]*H[1] + H[1]*P_pred[1][0]*H[0] + H[1]*P_pred[1][1]*H[1] + bms->R;
    float K[2] = { (P_pred[0][0]*H[0]+P_pred[0][1]*H[1])/S, (P_pred[1][0]*H[0]+P_pred[1][1]*H[1])/S };

    float y = V_meas - V_pred;
    c->SOC = SOC_pred + K[0]*y;
    c->V_RC = V_RC_pred + K[1]*y;

    float KH[2][2] = { {K[0]*H[0],K[0]*H[1]},{K[1]*H[0],K[1]*H[1]} };
    float IminusKH[2][2] = { {1-KH[0][0],-KH[0][1]},{-KH[1][0],1-KH[1][1]} };
    float P_new[2][2];
    P_new[0][0] = IminusKH[0][0]*P_pred[0][0]+IminusKH[0][1]*P_pred[1][0];
    P_new[0][1] = IminusKH[0][0]*P_pred[0][1]+IminusKH[0][1]*P_pred[1][1];
    P_new[1][0] = IminusKH[1][0]*P_pred[0][0]+IminusKH[1][1]*P_pred[1][0];
    P_new[1][1] = IminusKH[1][0]*P_pred[0][1]+IminusKH[1][1]*P_pred[1][1];
    bms->P[idx][0][0]=P_new[0][0]; bms->P[idx][0][1]=P_new[0][1];
    bms->P[idx][1][0]=P_new[1][0]; bms->P[idx][1][1]=P_new[1][1];

    float delta_Ah = fabs(I*DT)/3600.0f;
    c->Ah_acc += delta_Ah;
    float delta_SOC_abs = fabs(c->SOC - c->SOC_prev);
    float cycle_fraction = delta_SOC_abs / 100.0f;
    c->cycle_count += cycle_fraction;

    if(c->Ah_acc>0.0f){
        float soh_instant = 100.0f*(bms->C_nom*cycle_fraction)/c->Ah_acc;
        c->SOH_cap = SMOOTH_ALPHA*soh_instant + (1-SMOOTH_ALPHA)*c->SOH_cap;
    }
    c->SOC_prev = c->SOC;

    c->SOH_res = 100.0f*bms->R0_nominal/bms->R0;
}

// ---------------- Pack Update ----------------
void BMS_Pack_Update(BMS_Pack* pack, float I_total, float V_meas_array[NUM_CELLS]){
    for(int i=0;i<NUM_CELLS;i++){
        EKF_Cell_Update(pack, i, I_total, V_meas_array[i]);
    }
}

float Pack_Average_SOC(BMS_Pack* pack){
    float sum=0;
    for(int i=0;i<NUM_CELLS;i++) sum += pack->cells[i].SOC;
    return sum/NUM_CELLS;
}
float Pack_Average_SOH(BMS_Pack* pack){
    float sum=0;
    for(int i=0;i<NUM_CELLS;i++) sum += pack->cells[i].SOH_cap;
    return sum/NUM_CELLS;
}

// ---------------- Balancing ----------------
void BMS_Balance(BMS_Pack* pack){
    float Vcells[NUM_CELLS];
    for(int i=0;i<NUM_CELLS;i++)
        Vcells[i] = pack->cells[i].SOC/100.0f * (pack->OCV_table[TABLE_SIZE-1]-pack->OCV_table[0]) + pack->OCV_table[0];

    // Top 3 Zellen mitteln
    for(int i=0;i<NUM_CELLS-1;i++){
        int max_idx=i;
        for(int j=i+1;j<NUM_CELLS;j++) if(Vcells[j]>Vcells[max_idx]) max_idx=j;
        float tmp=Vcells[i]; Vcells[i]=Vcells[max_idx]; Vcells[max_idx]=tmp;
    }
    float V_target = (Vcells[0]+Vcells[1]+Vcells[2])/3.0f;

    for(int i=0;i<NUM_CELLS;i++){
        float Vcell = pack->cells[i].SOC/100.0f*(pack->OCV_table[TABLE_SIZE-1]-pack->OCV_table[0]) + pack->OCV_table[0];
        // Shunt nur aktiv, wenn Zellspannung über BALANCE_START_V
        if(Vcell >= BALANCE_START_V && Vcell > V_target + BALANCE_DELTA)
            pack->cells[i].shunt_on = 1;
        else
            pack->cells[i].shunt_on = 0;
    }
}

void Print_Balancing(BMS_Pack* pack){
    for(int i=0;i<NUM_CELLS;i++){
        printf("Zelle %02d: SOC=%.2f%% | SOH=%.2f%% | Shunt=%d\n",
            i+1, pack->cells[i].SOC, pack->cells[i].SOH_cap, pack->cells[i].shunt_on);
    }
}

// ---------------- Speicher & Laden ----------------
int BMS_Save(BMS_Pack* pack){
    FILE* f = fopen(SAVE_FILE,"wb");
    if(!f) return -1;
    fwrite(pack, sizeof(BMS_Pack), 1, f);
    fclose(f);
    return 0;
}

int BMS_Load(BMS_Pack* pack){
    FILE* f = fopen(SAVE_FILE,"rb");
    if(!f) return -1;
    fread(pack, sizeof(BMS_Pack), 1, f);
    fclose(f);
    return 0;
}

// ----------------- Main -----------------
int main(){
    BMS_Pack pack;

    if(BMS_Load(&pack)==0){
        printf("BMS-Zustand geladen.\n");
    } else {
        printf("Neustart: Initialisiere BMS.\n");
        pack.C_nom=100; pack.R0=0.002f; pack.R1=0.003f; pack.C1=1500; pack.R0_nominal=0.002f;
        pack.Q[0][0]=1e-7f; pack.Q[0][1]=0; pack.Q[1][0]=0; pack.Q[1][1]=1e-5f; pack.R=0.001f;

        float soc_table[TABLE_SIZE]={0,10,20,30,40,50,60,70,80,90,100};
        float ocv_table[TABLE_SIZE]={2.50,3.00,3.20,3.22,3.25,3.26,3.27,3.30,3.32,3.35,3.40};
        for(int i=0;i<TABLE_SIZE;i++){ pack.SOC_table[i]=soc_table[i]; pack.OCV_table[i]=ocv_table[i]; }

        for(int i=0;i<NUM_CELLS;i++){
            pack.cells[i].SOC=80; pack.cells[i].V_RC=0; pack.cells[i].Ah_acc=0; pack.cells[i].SOC_prev=80;
            pack.cells[i].cycle_count=0; pack.cells[i].SOH_cap=100; pack.cells[i].SOH_res=100; pack.cells[i].shunt_on=0;
            pack.P[i][0][0]=0.01f; pack.P[i][0][1]=0; pack.P[i][1][0]=0; pack.P[i][1][1]=0.01f;
        }
    }

    float I_total=-10;
    float V_meas[NUM_CELLS];
    for(int i=0;i<NUM_CELLS;i++) V_meas[i]=3.27f;

    for(int t=0;t<200;t++){
        BMS_Pack_Update(&pack,I_total,V_meas);
        BMS_Balance(&pack);

        if(t%40==0) BMS_Save(&pack);

        printf("t=%.2fs | Pack SOC=%.2f%% | Pack SOH=%.2f%%\n", t*DT, Pack_Average_SOC(&pack), Pack_Average_SOH(&pack));
        Print_Balancing(&pack);
    }

    return 0;
}
