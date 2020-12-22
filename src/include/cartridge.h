#pragma once

#include "types.h"

typedef enum {
    CARTRIDGE_SUCCESS = 0,
    CARTRIDGE_NOT_FOUND,
    CARTRIDGE_INVALID,
    CARTRIDGE_UNSUPPORTED,
} cartridge_status_t;

cartridge_status_t cartridge_init(char* filename);
u8 cartridge_prg_rd(u16 addr);
u8 cartridge_chr_rd(u16 addr);
void cartridge_prg_wr(u16 addr, u8 data);
void cartridge_chr_wr(u16 addr, u8 data);
void reset(void);
