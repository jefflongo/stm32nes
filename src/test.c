#include "cartridge.h"
#include "log.h"
#include "nes.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static void parse_cpu_state(nes_t* nes, char* s, int len) {
    snprintf(
      s,
      len,
      "%04X A:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%lu",
      nes->cpu.pc,
      nes->cpu.a,
      nes->cpu.x,
      nes->cpu.y,
      nes->cpu.p,
      nes->cpu.s,
      nes->cpu.cycle);
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

static bool test_cpu(void) {
    // load verification file
    FILE* test = fopen("test/nestest.txt", "rb");

    nes_t nes;
    if (!test || !nes_init(&nes, "test/nestest.nes")) {
        LOG("CPU TEST FAILURE\nVerification files not found.\n");
        return false;
    }
    // nestest should start at 0xC000 instead of 0xC004 for emulators with no GUI
    nes.cpu.pc &= ~0x0F;

    char cpu_state[100];
    char line[100];

    // Run test
    while (fgets(line, sizeof(line), test)) {
        parse_cpu_state(&nes, cpu_state, sizeof(cpu_state));
        parse_verification_state(line);
        if (strcmp(cpu_state, line)) {
            LOG("CPU TEST FAILURE\nExpected %s\nGot      %s\n", line, cpu_state);
            return false;
        }
        nes_step(&nes);
    }

    LOG("CPU TEST SUCCESS\n");
    return true;
}

int main(void) {
    return test_cpu() ? 0 : 1;
}