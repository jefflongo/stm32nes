#include "../cartridge.h"
#include "../cpu.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static void parse_cpu_state(char* s, int len) {
    if (len < 100) return;

    cpu_state_t state;
    char buffer[10];

    cpu_get_state(&state);

    strcpy(s, "PC:");
    sprintf(buffer, "%04X", state.PC);
    strcat(s, buffer);
    strcat(s, " A:");
    sprintf(buffer, "%02X", state.A);
    strcat(s, buffer);
    strcat(s, " X:");
    sprintf(buffer, "%02X", state.X);
    strcat(s, buffer);
    strcat(s, " Y:");
    sprintf(buffer, "%02X", state.Y);
    strcat(s, buffer);
    strcat(s, " P:");
    sprintf(buffer, "%02X", state.P);
    strcat(s, buffer);
    strcat(s, " SP:");
    sprintf(buffer, "%02X", state.S);
    strcat(s, buffer);
    strcat(s, " CYC:");
    sprintf(buffer, "%lu", (unsigned long)state.cycle);
    strcat(s, buffer);
}

static void parse_verification_state(const char* line, char* s, int len) {
    if (len < 100) return;

    char buffer[10];

    strcpy(s, "PC:");
    strncpy(buffer, line, 4);
    buffer[4] = '\0';
    strcat(s, buffer);
    strcat(s, " A:");
    strncpy(buffer, line + 50, 2);
    buffer[2] = '\0';
    strcat(s, buffer);
    strcat(s, " X:");
    strncpy(buffer, line + 55, 2);
    buffer[2] = '\0';
    strcat(s, buffer);
    strcat(s, " Y:");
    strncpy(buffer, line + 60, 2);
    buffer[2] = '\0';
    strcat(s, buffer);
    strcat(s, " P:");
    strncpy(buffer, line + 65, 2);
    buffer[2] = '\0';
    strcat(s, buffer);
    strcat(s, " SP:");
    strncpy(buffer, line + 71, 2);
    buffer[2] = '\0';
    strcat(s, buffer);
    strcat(s, " CYC:");
    strcpy(buffer, line + 90);
    int count = 0;
    while (line[90 + count] != '\r' && line[90 + count] != EOF) count++;
    buffer[count] = '\0';
    strcat(s, buffer);
}

static bool test_cpu() {
    // Load verification log and test rom
    FILE* test = fopen("test/nestest.txt", "rb");
    if (!test || load_rom("test/nestest.nes") != 0) {
        printf("CPU TEST FAILURE\nVerification files not found.\n");
        return false;
    }

    char state[100];
    char exp[100];
    char line[100];

    // Init cpu
    cpu_init();

    // Run test
    while (fgets(line, sizeof(line), test)) {
        parse_cpu_state(state, sizeof(state));
        parse_verification_state(line, exp, sizeof(exp));
        if (strcmp(state, exp)) {
            printf("CPU TEST FAILURE\nExpected %s\nGot      %s\n", exp, state);
            return false;
        }
        // For debugging:
        // printf("%s\n", state);
        cpu_run();
    }

    printf("CPU TEST SUCCESS\n");
    return true;
}

int main() {
    if (!test_cpu()) return -1;

    return 0;
}