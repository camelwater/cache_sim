// Ryan Zhao
// adapted from https://github.com/EmreKumas/Cache_Simulator

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>

typedef struct line{
    char validBit;
    char dirtyBit;
    int tag;
    char *data;
}line;

typedef struct set{
    line *lines;
}set;

void setGlobalVariables(int associativity);
void createCaches(set *L1I, set *L1D, set *L2);
void zeroCacheBits(set *L1I, set *L1D, set *L2);
void traceFile(set *L1I, set *L1D, set *L2);

void loadInstructionOrData(bool loadInstruction, char *cacheName, set *L1I, set *L1D, set *L2, char *address, char* data);
void dataWrite(set *L1D, set *L2, char *address, char *data);
void dataModify(set *L1I, set *L1D, set *L2, char *address, int size, char *data);

int returnLineNumberIfStored(set *L, int lineNumber, int setNum, int tag);
int copyInstructionBetweenCaches(set *source, set *dest, int sourceLineNumber, int sourceSetNum, int destSetNum, int destTag, int destTotalLine, int destBlockSize, char* destName);
int copyInstructionFromRamToL2Cache(int int_address, set *L1I, set *L1D, set *L2, int setNumL2, int tagL2, char* data);
void writeBack(set* cache, int setNum, int lineNum, char* writeBackData);
void printSummary(char *cacheName, int hit, int miss, int eviction);
void cleanUp(set *L1I, set *L1D, set *L2);

//Global variables.
int L1s, L1E, L1b, L2s, L2E, L2b, L1S, L1B, L2S, L2B;
char *fileName;

//Hit, miss, eviction counters.
int L1I_hit, L1I_miss, L1I_eviction;
int L1D_hit, L1D_miss, L1D_eviction;
int L2_hit, L2_miss, L2_eviction;
int DRAM_accesses;

// Time stats
double total_access_time, L1I_access_time, L1D_access_time, L2_access_time, DRAM_access_time, DRAM_writeback_time;
int num_accesses;

// Energy stats
double L1I_energy, L1D_energy, L2_energy, DRAM_energy;

//Loop iterators.
int i, j;

int assocs[3] = { 2, 4, 8 };
char* traceFiles[15] = {
                        "./traces/008.espresso.din",
                        "./traces/013.spice2g6.din",
                        "./traces/015.doduc.din",
                        "./traces/022.li.din",
                        "./traces/023.eqntott.din",
                        "./traces/026.compress.din",
                        "./traces/034.mdljdp2.din",
                        "./traces/039.wave5.din",
                        "./traces/047.tomcatv.din",
                        "./traces/048.ora.din",
                        "./traces/085.gcc.din",
                        "./traces/089.su2cor.din",
                        "./traces/090.hydro2d.din",
                        "./traces/093.nasa7.din",
                        "./traces/094.fpppp.din",
                        };

const int REPETITIONS = 10;


int main(int argc, char *argv[]){
    srand(time(NULL));

    remove("./results_c2.txt");
    if (! freopen("results_c2.txt", "w", stdout)) {
        fprintf(stderr, "Failed to create results.txt file");
        return 1;
    }
    for (int f = 0; f < 15; f++) {
        fileName = traceFiles[f];
        printf("Tracefile: %s\n", fileName);
        for (int ass = 0; ass < 3; ass++) {
            double L1I_energies[REPETITIONS];
            double L1D_energies[REPETITIONS];
            double L2_energies[REPETITIONS];
            double DRAM_energies[REPETITIONS];
            double access_times[REPETITIONS];
            double total_access_times[REPETITIONS];
            for (int iter = 0; iter < REPETITIONS; iter++) {
                setGlobalVariables(assocs[ass]);

                set L1D[L1S];
                set L1I[L1S];
                set L2[L2S];

                createCaches(L1I, L1D, L2);

                //After creating the caches, we need to make sure their valid and dirty bit is 0 initially.
                zeroCacheBits(L1I, L1D, L2);

                //Now, we need to read the trace file.
                traceFile(L1I, L1D, L2);

                /////////////////////////////////////////////////////////////////////////////////////////////

                total_access_time = 0.5 * num_accesses;
                total_access_time += L1I_access_time + L1D_access_time + L2_access_time + DRAM_access_time;
                double avg_access_time = total_access_time / num_accesses;
                
                //convert picojoules to nanojoules
                L1I_energy /= 1000;
                L1D_energy /= 1000; 
                L2_energy /= 1000;
                DRAM_energy /= 1000;
                
                L1I_energy += (total_access_time - L1I_access_time) * 0.5; // idle energy usage
                L1I_energy += L1I_access_time * 1; // active energy usage
                
                L1D_energy += (total_access_time - L1D_access_time) * 0.5; // idle energy usage
                L1D_energy += L1D_access_time * 1; // active energy usage

                L2_energy += (total_access_time - L2_access_time) * 0.8; // idle energy usage
                L2_energy += L2_access_time * 2; // active energy usage

                DRAM_energy += (total_access_time - DRAM_access_time - DRAM_writeback_time) * 0.8; // idle
                DRAM_energy += (DRAM_access_time + DRAM_writeback_time) * 4; // active

                L1I_energies[iter] = L1I_energy;
                L1D_energies[iter] = L1D_energy;
                L2_energies[iter] = L2_energy;
                DRAM_energies[iter] = DRAM_energy;
                access_times[iter] = avg_access_time;
                total_access_times[iter] = total_access_time;

                cleanUp(L1I, L1D, L2);
            }

            //Printing the results.
            printf("\n");
            printf("L2 ASSOCIATIVITY = %d", assocs[ass]);
            printSummary("L1I", L1I_hit, L1I_miss, L1I_eviction);
            printSummary("L1D", L1D_hit, L1D_miss, L1D_eviction);
            printSummary("L2", L2_hit, L2_miss, L2_eviction);
            printf("\nDRAM accesses: %d", DRAM_accesses);
            printf("\nwrite back time: %0.3f, access time: %0.3f", DRAM_writeback_time, DRAM_access_time);
            // printf("\n----------------------------------------\nEnergy and timing stats:\n");
            double mean_L1I_energy = 0, mean_L1D_energy = 0, mean_L2_energy = 0, mean_DRAM_energy = 0;
            double L1I_sd = 0, L1D_sd = 0, L2_sd = 0, DRAM_sd = 0;
            double mean_access_time = 0;
            double access_time_sd = 0;
            double mean_total_access_time = 0;
            double total_access_time_sd = 0;
            for (i = 0; i < REPETITIONS; i++) {
                mean_L1I_energy += L1I_energies[i];
                mean_L1D_energy += L1D_energies[i];
                mean_L2_energy += L2_energies[i];
                mean_DRAM_energy += DRAM_energies[i];
                mean_access_time += access_times[i];
                mean_total_access_time += total_access_times[i];
            }
            mean_L1I_energy /= REPETITIONS;
            mean_L1D_energy /= REPETITIONS;
            mean_L2_energy /= REPETITIONS;
            mean_DRAM_energy /= REPETITIONS;
            mean_access_time /= REPETITIONS;
            mean_total_access_time /= REPETITIONS;

            for (i = 0; i < REPETITIONS; i++) {
                L1I_sd += pow(L1I_energies[i] - mean_L1I_energy, 2);
                L1D_sd += pow(L1D_energies[i] - mean_L1D_energy, 2);
                L2_sd += pow(L2_energies[i] - mean_L2_energy, 2);
                DRAM_sd += pow(DRAM_energies[i] - mean_DRAM_energy, 2);
                access_time_sd += pow(access_times[i] - mean_access_time, 2);
                total_access_time_sd += pow(total_access_times[i] - mean_total_access_time, 2);
            }

            L1I_sd = sqrt(L1I_sd / REPETITIONS);
            L1D_sd = sqrt(L1D_sd / REPETITIONS);
            L2_sd = sqrt(L2_sd / REPETITIONS);
            DRAM_sd = sqrt(DRAM_sd / REPETITIONS);
            access_time_sd = sqrt(access_time_sd / REPETITIONS);
            total_access_time_sd = sqrt(total_access_time_sd / REPETITIONS);

            printf("\nmean L1I energy usage: %0.2f nJ, std dev: %0.3f\n", mean_L1I_energy, L1I_sd);
            printf("mean L1D energy usage: %0.2f nJ, std dev: %0.3f\n", mean_L1D_energy, L1D_sd);
            printf("mean L2 energy usage: %0.2f nJ, std dev: %0.3f\n", mean_L2_energy, L2_sd);
            printf("mean DRAM energy usage: %0.2f nJ, std dev: %0.3f\n", mean_DRAM_energy, DRAM_sd);
            printf("mean operation memory access time: %0.3f ns, std dev: %0.3f\n", mean_access_time, access_time_sd);
            printf("mean total memory access time: %0.3f ns, std dev: %0.3f", mean_total_access_time, total_access_time_sd);
            printf("\n");
        }
        printf("\n----------------------------------------\n\n");
        fprintf(stderr, "Tracefile %d/15\n", f+1);
    }

    return 0;
}

void setGlobalVariables(int associativity){
    L1s = 9; // 9 index bits 32K/64 = 2^15/2^6 = 2^9
    L1E = 1; // direct-mapped
    L1b = 6; // 6 block bits (64B blocks)

    L2s = 12 - log(associativity)/log(2) ; // 10 index bits - (256K/64)/4 = 2^10
    L2E = associativity; // set associativity
    L2b = 6; // 6 block bits (64 byte blocks)

    L1S = (int) pow(2, L1s);
    L2S = (int) pow(2, L2s);

    L1B = (int) pow(2, L1b);
    L2B = (int) pow(2, L2b);

    L1I_hit = 0, L1I_miss = 0, L1I_eviction = 0;
    L1D_hit = 0, L1D_miss = 0, L1D_eviction = 0;
    L2_hit = 0, L2_miss = 0, L2_eviction = 0;
    DRAM_accesses = 0;

    total_access_time = 0, num_accesses = 0;
    L1I_access_time = 0, L1D_access_time = 0, L2_access_time = 0, DRAM_access_time = 0, DRAM_writeback_time = 0;
    L1I_energy = 0, L1D_energy = 0, L2_energy = 0, DRAM_energy = 0;
}

void createCaches(set *L1I, set *L1D, set *L2){
    int blockSizeL1 = (int) pow(2, L1b);
    int blockSizeL2 = (int) pow(2, L2b);

    //Allocating space for lines and datas.
    for(i = 0; i < L1S; i++){
        L1D[i].lines = malloc(L1E * sizeof(line));
        L1I[i].lines = malloc(L1E * sizeof(line));

        //Setting the block sizes.
        for(j = 0; j < L1E; j++){
            L1D[i].lines[j].data = malloc(blockSizeL1 * sizeof(char));
            L1I[i].lines[j].data = malloc(blockSizeL1 * sizeof(char));
        }
    }

    //Same thing for L2 cache.
    for(i = 0; i < L2S; i++){
        L2[i].lines = malloc(L2E * sizeof(line));

        //Setting the block sizes.
        for(j = 0; j < L2E; j++)
            L2[i].lines[j].data = malloc(blockSizeL2 * sizeof(char));
    }
}

void zeroCacheBits(set *L1I, set *L1D, set *L2){
    for(i = 0; i < L1S; i++){
        for(j = 0; j < L1E; j++){
            L1I[i].lines[j].dirtyBit = 0;
            L1D[i].lines[j].dirtyBit = 0;
            L1I[i].lines[j].validBit = 0;
            L1D[i].lines[j].validBit = 0;
        }
    }

    for(i = 0; i < L2S; i++){
        for(j = 0; j < L2E; j++){

            L2[i].lines[j].validBit = 0;
            L2[i].lines[j].dirtyBit = 0;

        }
    }
}

void traceFile(set *L1I, set *L1D, set *L2){
    FILE *fp = NULL;

    //If the file exists, we read it. If not, we exit the function with an error.
    if(access(fileName, F_OK) != -1){
        fp = fopen(fileName, "r");

    } else {
        printf("%s does not exist. Please make sure it exists before starting this program.\n", fileName);
        return;
    }

    char operation;
    char address[9], data[9];

    while(!feof(fp)) { // NOLINT

        if(fscanf(fp, "%c", &operation) == 1){

            //Now, we will look at the "operation" to understand which operation to execute.
            switch(operation){
                case '0':
                    if(fscanf(fp, " %s %s\n", address, data) == 2){ // NOLINT
                        loadInstructionOrData(false, "L1D", L1I, L1D, L2, address, data);
                        num_accesses++;
                    }
                    break;
                case '1':
                    if(fscanf(fp, " %s %s\n", address, data) == 2){ // NOLINT
                        dataWrite(L1D, L2, address, data);
                        num_accesses++;
                    }
                    break;
                case '2':
                    if(fscanf(fp, " %s %s\n", address, data) == 2){ // NOLINT
                        loadInstructionOrData(true, "L1I", L1I, L1D, L2, address, data);
                        num_accesses++;
                    }
                    break;
                case '3':
                    // printf("\nunknown access type / ignore");
                    break;
                case '4':
                    // flushCache();
                    break;
                default:
                    //If we encounter any weird symbol it means this file is corrupted. We return from the function.
                    printf("\nThe program encountered an invalid operation type, exiting...\n");
                    return;
            }
        }
    }

    fclose(fp);
}

void loadInstructionOrData(bool loadInstruction, char *cacheName, set *L1I, set *L1D, set *L2, char *address, char* data){
    //At the start of the function, we need to tell program which set to use for the rest of the function.
    set *setToUse;
    int size = 4;

    if(loadInstruction)
        setToUse = L1I;
    else
        setToUse = L1D;

    //First, we need to convert the address into an appropriate format and also convert hex to decimal.
    int int_address = (int) strtol(address, NULL, 16);

    //Getting necessary information.
    int blockNumL1 = int_address & ((int) pow(2, L1b) - 1); // NOLINT
    int setNumL1 = (int_address >> L1b) & ((int) pow(2, L1s) - 1); // NOLINT
    int tagSizeL1 = 32 - (L1s + L1b);
    int tagL1 = (int_address >> (L1s + L1b)) & ((int) pow(2, tagSizeL1) - 1); // NOLINT

    int blockNumL2 = int_address & ((int) pow(2, L2b) - 1); // NOLINT
    int setNumL2 = (int_address >> L1b) & ((int) pow(2, L2s) - 1); // NOLINT
    int tagSizeL2 = 32 - (L2s + L2b);
    int tagL2 = (int_address >> (L2s + L2b)) & ((int) pow(2, tagSizeL2) - 1); // NOLINT

    // TODO: fetch the next line if overflows?
    //Checking if the data can fit into cache block.
    // if(blockNumL1 + size > L1B || blockNumL2 + size > L2B){
    //     int next_address = tagL1 << (L1s + L1b);
    //     next_address = next_address | (setNumL1 << L1b);
    //     next_address = next_address | L1B + 1;
    //     // loadInstructionOrData(loadInstruction, cacheName, L1I, L1D, L2, next_address, data);
    // }

    //Then, we need to check if the instruction is in caches.
    int L1_location = returnLineNumberIfStored(setToUse, L1E, setNumL1, tagL1);
    int L2_location = returnLineNumberIfStored(L2, L2E, setNumL2, tagL2);

    // checking L1 cache
    if (loadInstruction) 
        L1I_access_time += .5;
    else 
        L1D_access_time += .5;
    if(L1_location != -1) {
        ////////////////////////////////// HIT IN L1

        (loadInstruction == true) ? L1I_hit++ : L1D_hit++;
    }else{
        ////////////////////////////////// MISS IN L1

        (loadInstruction == true) ? L1I_miss++ : L1D_miss++;

        //After that, we need to check if the instruction is stored in L2 cache.
        L2_energy += 5 * (int) pow(10, -12); // L2 penalty
        L2_access_time += 4.5;
        if(L2_location != -1) {
            ////////////////////////////////// MISS IN L1 BUT HIT IN L2

            //The instruction is in L2 cache.
            L2_hit++;

            //Now, we need to load it into L1 cache.
            int eviction_counter = copyInstructionBetweenCaches(L2, setToUse, L2_location, setNumL2, setNumL1, tagL1, L1E, L1B, "L1");

            if(loadInstruction) L1I_eviction += eviction_counter;
            else L1D_eviction += eviction_counter;
            
            // write to L1
            // if(loadInstruction) L1I_access_time += .5;
            // else L1D_access_time += .5;
        } else {
            ////////////////////////////////// MISS IN BOTH L1 AND L2

            L2_miss++;

            //The instruction is not in L1 nor L2. So we need to load it from RAM.
            L2_location = copyInstructionFromRamToL2Cache(int_address, L1I, L1D, L2, setNumL2, tagL2, data);
            int eviction_counter = copyInstructionBetweenCaches(L2, setToUse, L2_location, setNumL2, setNumL1, tagL1, L1E, L1B, "L1");
            if(loadInstruction) L1I_eviction += eviction_counter;
            else L1D_eviction += eviction_counter;

            DRAM_access_time += 45;
            DRAM_energy += 640 * (int) pow(10, -12); // DRAM penalty
            DRAM_accesses++;

            // read from DRAM, write to L2, write to L1
            // DRAM_access_time += 50;
            // L2_access_time += 5;
            // L1D_access_time += 0.5;
            // L2_energy += 5 * (int) pow(10, -12); // L2 penalty
            // DRAM_energy += 640 * (int) pow(10, -12); // DRAM penalty
        }
    }
}

void dataWrite(set *L1D, set *L2, char *address, char *data){
    //First, we need to convert the address into an appropriate format and also convert hex to decimal.
    int int_address = (int) strtol(address, NULL, 16);
    int size = 4;

    //Also, we need to convert the data to a an appropriate format.
    char char_data[size], subString[2];

    for(int a = 0; a < size; a++){
        strncpy(subString, (data + a * 2), 2);
        char_data[a] = (char) strtol(subString, NULL, 16);
    }

    //Getting necessary information.
    int blockNumL1 = int_address & ((int) pow(2, L1b) - 1); // NOLINT
    int setNumL1 = (int_address >> L1b) & ((int) pow(2, L1s) - 1); // NOLINT
    int tagSizeL1 = 32 - (L1s + L1b);
    int tagL1 = (int_address >> (L1s + L1b)) & ((int) pow(2, tagSizeL1) - 1); // NOLINT

    int blockNumL2 = int_address & ((int) pow(2, L2b) - 1); // NOLINT
    int setNumL2 = (int_address >> L1b) & ((int) pow(2, L2s) - 1); // NOLINT
    int tagSizeL2 = 32 - (L2s + L2b);
    int tagL2 = (int_address >> (L2s + L2b)) & ((int) pow(2, tagSizeL2) - 1); // NOLINT

    //Checking if the data can fit into cache block.
    // if(blockNumL1 + size > L1B || blockNumL2 + size > L2B){
    //     int next_address = tagL1 << (L1s + L1b);
    //     next_address = next_address | (setNumL1 << L1b);
    //     next_address = next_address | L1B + 1;
    //     // TODO:
    // }

    //Then, we need to check if the data is in caches.
    int L1D_location = returnLineNumberIfStored(L1D, L1E, setNumL1, tagL1);
    int L2_location = returnLineNumberIfStored(L2, L2E, setNumL2, tagL2);

    L1D_access_time += .5;
    if(L1D_location != -1) {
        ////////////////////////////////// HIT IN L1D

        L1D_hit++;

        // write to L1
        L1D[setNumL1].lines[L1D_location].dirtyBit = 1;
        memcpy(L1D[setNumL1].lines[L1D_location].data, char_data, (size_t) size);

        L2_hit++;
        
        // write through to L2 (inclusive so the line is present in L2)
        L2[setNumL2].lines[L2_location].dirtyBit = 1;
        memcpy(L2[setNumL2].lines[L2_location].data, L1D[setNumL1].lines[L1D_location].data, (size_t) size);

        L2_access_time += 4.5;

    }else{
        ////////////////////////////////// MISS IN L1D

        L1D_miss++;

        //After that, we need to check if the data is stored in L2 cache.
        // L2_energy += 5 * (int) pow(10, -12); // L2 penalty
        L2_access_time += 4.5;
        if(L2_location != -1) {
            ////////////////////////////////// MISS IN L1D BUT HIT IN L2
            //The data is in L2 cache.
            L2_hit++;

            //Now, we need to load this data into L1D cache.
            L1D_eviction += copyInstructionBetweenCaches(L2, L1D, L2_location, setNumL2, setNumL1, tagL1, L1E, L1B, "L1");

            // read from L2, write to L1
            // L2_access_time += 5;
            // L1D_access_time += 0.5;
        }else{
            ////////////////////////////////// MISS IN BOTH L1D AND L2

            L2_miss++;
            
            // writing directly to the L2 line, but using this function
            // just not adding DRAM penalty and access time
            L2_location = copyInstructionFromRamToL2Cache(int_address, L1D, L1D, L2, setNumL2, tagL2, data);
            L1D_eviction += copyInstructionBetweenCaches(L2, L1D, L2_location, setNumL2, setNumL1, tagL1, L1E, L1B, "L1");

            // DRAM_access_time += 45;
            // DRAM_energy += 640 * (int) pow(10, -12); // DRAM penalty
        }

        //Now, we caught all misses and evictions and datas are in both L1D and L2 caches.

        //Make the changes. For this, we need the line number.
        L1D_location = returnLineNumberIfStored(L1D, L1E, setNumL1, tagL1);

        // write to L1
        L1D[setNumL1].lines[L1D_location].dirtyBit = 1;
        memcpy(L1D[setNumL1].lines[L1D_location].data, char_data, (size_t) size);

        // write through to L2
        L2[setNumL2].lines[L2_location].dirtyBit = 1;
        memcpy(L2[setNumL2].lines[L2_location].data, char_data, (size_t) size);
    }
}

int returnLineNumberIfStored(set *L, int lineNumber, int setNum, int tag){
    int i;

    //First, we need to check if the corresponding line is valid and is it contains the data.
    for(i = 0; i < lineNumber; i++){
        //We will look if the valid bit is 1 and also the tag matches.
        if(L[setNum].lines[i].validBit == 1 && L[setNum].lines[i].tag == tag)
            return i;
    }

    return -1;
}

int copyInstructionBetweenCaches(set *source, set *dest, int sourceLineNumber, int sourceSetNum, int destSetNum, int destTag, int destTotalLine, int destBlockSize, char* destName){
    //Return value. 1 if there is an eviction, 0 if there is no eviction.
    int returnValue = 0;

    bool freeSlot = false;
    int lineToPlace = 0;

    for(i = 0; i < destTotalLine; i++){

        if(dest[destSetNum].lines[i].validBit == 0){
            freeSlot = true;
            lineToPlace = i;
            break;
        }
    }

    //If freeSlot is false, it means all the lines are valid so we have an eviction.
    if(freeSlot == false) {
        returnValue++;

        // random eviction policy
        lineToPlace = rand() % destTotalLine;

        // write back to L2 if line is dirty
        // if (dest[destSetNum].lines[lineToPlace].dirtyBit == 1) {
        //     // char* writeBackData = malloc(64);
        //     // memcpy(writeBackData, dest[destSetNum].lines[lineToPlace].data, destBlockSize);
        //     int tag = dest[destSetNum].lines[lineToPlace].tag << L1s;
        //     int addr = tag | destSetNum;
        //     int setNum = addr & (L2S - 1);
        //     tag = (addr >> L2s) & ((int) pow(2, 26 - L2s) - 1);
        //     int lineNum = returnLineNumberIfStored(source, L2E, setNum, tag);
        //     writeBack(source, setNum, lineNum, dest[destSetNum].lines[lineToPlace].data);
        //     L2_hit++;
        // }
        
    }

    //Now, we have determined which line to store our instruction.
    dest[destSetNum].lines[lineToPlace].validBit = 1;
    dest[destSetNum].lines[lineToPlace].dirtyBit = 0;
    dest[destSetNum].lines[lineToPlace].tag = destTag;
    memcpy(dest[destSetNum].lines[lineToPlace].data, source[sourceSetNum].lines[sourceLineNumber].data, (size_t) destBlockSize);

    return returnValue;
}

int copyInstructionFromRamToL2Cache(int int_address, set *L1I, set *L1D, set *L2, int setNumL2, int tagL2, char* data){

    char readData[64], subString[2];

    for(int a = 0; a < 64; a++){
        strncpy(subString, (data + a * 2), 2);
        readData[a] = (char) strtol(subString, NULL, 16);
    }

    //Now, we need to store this data into the L2 cache.
    bool freeSlot = false;
    int lineToPlace = 0;

    for(i = 0; i < L2E; i++){
        if(L2[setNumL2].lines[i].validBit == 0){
            freeSlot = true;
            lineToPlace = i;
            break;
        }
    }

    //If freeSlot is false, it means all the lines are valid so we have an eviction.
    if(freeSlot == false){
        L2_eviction++;

        // random eviction policy
        lineToPlace = rand() % L2E;

        // back invalidation for inclusivity - if block evicted from L2, it must not be in L1
        int tag = L2[setNumL2].lines[lineToPlace].tag << L2s;
        int addr = tag | setNumL2;
        int setNum = addr & (L1S - 1);
        tag = (addr >> L1s) & ((int) pow(2, 26 - L1s) - 1);
        int lD = returnLineNumberIfStored(L1D, L1E, setNum, tag);
        int lI = returnLineNumberIfStored(L1I, L1E, setNum, tag);
        if (lD != -1) {
            // write back to L2 (the current block) if line is dirty
            // if (L1D[setNum].lines[lD].dirtyBit == 1) {
            //     char* writeBackData = malloc(64);
            //     // char writeBackData[64];
            //     memcpy(writeBackData, L1D[setNum].lines[lD].data, 64);
            //     // int tag = dest[destSetNum].lines[lineToPlace].tag << L1s;
            //     // int addr = tag | destSetNum;
            //     // int setNum = addr & (L2S - 1);
            //     // tag = (addr >> L2s) & ((int) pow(2, 26 - L2s) - 1);
            //     // int lineNum = returnLineNumberIfStored(source, L2E, setNum, tag);
            //     writeBack(L2, setNumL2, lineToPlace, writeBackData);
            //     L2_hit++;
            // }

            // evict from L1
            L1D[setNum].lines[lD].validBit = 0;
            L1D_eviction++;
        } else if (lI != -1) {
            // fprintf(stderr, "back invalidate");
            // L1I[setNum].lines[lI].validBit = 0;
            // L1I_eviction++;
        }
        // if (isInstruction) L1I_access_time += .5;
        // else L1D_access_time += .5;

        // write back if line is dirty
        if (L2[setNumL2].lines[lineToPlace].dirtyBit == 1) {
            writeBack(NULL, setNumL2, lineToPlace, NULL);
        }
    }

    //Now, we have determined which line to store our instruction.
    L2[setNumL2].lines[lineToPlace].validBit = 1;
    L2[setNumL2].lines[lineToPlace].dirtyBit = 0;
    L2[setNumL2].lines[lineToPlace].tag = tagL2;
    memcpy(L2[setNumL2].lines[lineToPlace].data, readData, (size_t) L2B);

    return lineToPlace;
}

// write back only from L2->DRAM (asynchronously)
void writeBack(set* cache, int setNum, int lineNum, char* writeBackData){
    if (cache) { // write back to L2 (evicted from L1)
        // memcpy(cache[setNum].lines[lineNum].data, writeBackData, 64);
        // cache[setNum].lines[lineNum].dirtyBit = 1;
        // L2_access_time += 5;
        // L2_energy += 5 * (int) pow(10, -12); // L2 penalty
    } else { //write back to disk (evicted from L2)
        DRAM_writeback_time += 45; // asynchronous
        DRAM_energy += 640 * (int) pow(10, -12); // DRAM penalty
        DRAM_accesses++;
    }

}

void printSummary(char *cacheName, int hit, int miss, int eviction){

    printf("\n%s-accesses: %d %s-misses: %d %s-evictions: %d", cacheName, hit+miss, cacheName, miss, cacheName, eviction);

}

void cleanUp(set *L1I, set *L1D, set *L2){

    //Freeing L1 cache.
    for(i = 0; i < L1S; i++){

        //Firstly, we need to free data pointers.
        for(j = 0; j < L1E; j++){
            free(L1I[i].lines[j].data);
            free(L1D[i].lines[j].data);
        }

        //Now, we can free lines.
        free(L1D[i].lines);
        free(L1I[i].lines);
    }

    //Same things for L2 cache.
    for(i = 0; i < L2S; i++){

        //Firstly, we need to free data pointers.
        for(j = 0; j < L2E; j++)
            free(L2[i].lines[j].data);

        //Now, we can free lines.
        free(L2[i].lines);
    }
}
