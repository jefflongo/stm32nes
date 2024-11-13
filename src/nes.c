#include "nes.h"

#include "cartridge.h"
#include "cpu.h"
#include "memory.h"

bool nes_init(nes_t* nes, char const* file) {
    if (cartridge_init(nes, file) != CARTRIDGE_SUCCESS) {
        return false;
    }
    memory_init(nes);
    cpu_init(nes);

    return true;
}

void nes_step(nes_t* nes) {
    cpu_step(nes);
}
