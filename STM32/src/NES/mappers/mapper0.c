#include "NES/mappers/mapper0.h"

static u8 *prg_bank, *chr_bank;

void mapper0_init(u8* p_Bank, u8* c_bank) {
    prg_bank = p_bank;
    chr_bank = c_bank;

    *prg_bank = 0;
    *chr_bank = 0;
}
