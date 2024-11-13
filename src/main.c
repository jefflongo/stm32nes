#include "cartridge.h"
#include "log.h"
#include "nes.h"

#include <stdlib.h>

int main() {
    nes_t nes;
    if (!nes_init(&nes, "roms/smb.nes")) {
        LOG("Failed to initialize\n");
        return 1;
    }

    while (1) {
        nes_step(&nes);
    }

    return 0;
}