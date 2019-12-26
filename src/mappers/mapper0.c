#include "mappers/mapper0.h"

#include "nes.h"

void mapper0_init(u32* prg_map, u32* chr_map, u8 prg_units) {
    // Perform 1:1 mapping for prg and chr
    for (int i = 0; i < 4; i++) {
        prg_map[i] =
          (PRG_SLOT_SIZE * i) % (prg_units * PRG_DATA_UNIT_SIZE);
    }
    for (int i = 0; i < 8; i++) {
        chr_map[i] = CHR_SLOT_SIZE * i;
    }
}