#include "cartridge.h"
#include "cpu.h"
#include "log.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static void parse_cpu_state(char* s, int len) {
    cpu_state_t state;
    cpu_get_state(&state);
    snprintf(
      s,
      len,
      "%04X A:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%lu",
      state.PC,
      state.A,
      state.X,
      state.Y,
      state.P,
      state.S,
      state.cycle);
}

static void parse_verification_state(char* line) {
    size_t n = strlen(line);
    assert(n > 90);
    // remove the instruction information
    memmove(line + 5, line + 48, 26);
    // remove the PPU counter
    // TODO: support this
    memmove(line + 31, line + 86, n - 86);
    // remove newline
    char* newline = strchr(line, '\n');
    if (newline) {
        *newline = '\0';
    }
}

static bool test_cpu() {
    // Load verification log and test rom
    FILE* test = fopen("test/nestest.txt", "rb");
    if (!test || cartridge_init("test/nestest.nes") != 0) {
        LOG("CPU TEST FAILURE\nVerification files not found.\n");
        return false;
    }

    char state[100];
    char line[100];

    // Init cpu
    cpu_init();

    // Run test
    while (fgets(line, sizeof(line), test)) {
        parse_cpu_state(state, sizeof(state));
        parse_verification_state(line);
        if (strcmp(state, line)) {
            LOG("CPU TEST FAILURE\nExpected %s\nGot      %s\n", line, state);
            return false;
        }
        cpu_run();
    }

    LOG("CPU TEST SUCCESS\n");
    return true;
}

int main(void) {
    return test_cpu();
}