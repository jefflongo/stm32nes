#ifndef MAPPER_H_
#define MAPPER_H_

#include "mappers/mapper0.h"
#include "types.h"

u8 mapper_rd(u16 addr);
void mapper_wr(u16 addr, u8 data);
int load_rom(char* filename);
void mapPrg(u8 bank);
void mapChr(u8 bank);
void reset();

#endif // MAPPER_H_