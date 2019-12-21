#include "mapper0.h"

static u8 *_prg_bank, *_chr_bank;

void mapper0_init(u8* prg_bank, u8* chr_bank) {
    _prg_bank = prg_bank;
    _chr_bank = chr_bank;

    *_prg_bank = 0;
    *_chr_bank = 0;
}