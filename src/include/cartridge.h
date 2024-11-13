#pragma once

#include "nes.h"

typedef enum {
    CARTRIDGE_SUCCESS = 0,
    CARTRIDGE_NOT_FOUND,
    CARTRIDGE_INVALID,
    CARTRIDGE_UNSUPPORTED,
} cartridge_result_t;

cartridge_result_t cartridge_init(nes_t* nes, char const* filename);
u8 cartridge_prg_rd(nes_t* nes, u16 addr);
u8 cartridge_chr_rd(nes_t* nes, u16 addr);
void cartridge_prg_wr(nes_t* nes, u16 addr, u8 data);
void cartridge_chr_wr(nes_t* nes, u16 addr, u8 data);
void reset(nes_t* nes);
