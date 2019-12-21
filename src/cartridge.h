#ifndef CARTRIDGE_H_
#define CARTRIDGE_H_

#include "types.h"

u8 cartridge_rd(u16 addr);
void cartridge_wr(u16 addr, u8 data);
int load_rom(char* filename);
void map_prg(u8 bank);
void map_chr(u8 bank);
void reset();

#endif // CARTRIDGE_H_