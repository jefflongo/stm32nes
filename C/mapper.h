#ifndef MAPPER_H_
#define MAPPER_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef uint16_t u16;
typedef uint8_t u8;

u8 mapper_rd(u16 addr);
void mapper_wr(u16 addr, u8 data);
int load_rom(char* filename);
void mapPrg(u8 bank);
void mapChr(u8 bank);
void reset();

#endif // MAPPER_H_