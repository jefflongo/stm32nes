#ifndef CARTRIDGE_H_
#define CARTRIDGE_H_

#include "types.h"

int cartridge_init(char* filename);
u8 cartridge_prg_rd(u16 addr);
u8 cartridge_chr_rd(u16 addr);
void cartridge_prg_wr(u16 addr, u8 data);
void cartridge_chr_wr(u16 addr, u8 data);
void reset(void);

#endif // CARTRIDGE_H_