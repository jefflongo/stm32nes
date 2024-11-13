#include "nes.h"

#include "cpu.h"
#include "memory.h"

void nes_init(nes_t* nes) {
    memory_init(nes);
    cpu_init(nes);
}

void nes_step(nes_t* nes) {
    cpu_step(nes);
}
