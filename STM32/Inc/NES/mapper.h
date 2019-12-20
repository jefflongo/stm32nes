#ifndef MAPPER_H_
#define MAPPER_H_

#include "fatfs.h"
#include "mappers/mapper0.h"
#include "types.h"

u8 mapper_rd(u16 addr);
void mapper_wr(u16 addr, u8 data);
int load_rom_from_sd(FATFS* fs, char* filename);
void map_prg(u8 bank);
void map_chr(u8 bank);
void reset();

#endif // MAPPER_H_
