#include "cartridge.h"
#include "log.h"
#include "nes.h"

#include <stdlib.h>

int main() {
    if (cartridge_init("roms/smb.nes") != 0) {
        LOG("Failed to load ROM");
        return -1;
    }

    nes_t nes;
    nes_init(&nes);
    while (1) {
        nes_step(&nes);
    }

    return 0;
}