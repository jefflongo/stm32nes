#include "mapper.h"

// Represents 0x4020 - 0xFFFF
u8 mem[0xBFE0];

// Offset of addr - 0x8000 + 0x10 in the ROM
u8 mapper_rd(u16 addr) {
	return mem[addr - 0x4020];
}
	
void mapper_wr(u16 addr, u8 data) {
	mem[addr - 0x4020] = data;
}
	
int load_rom(char* filename) {
	FILE* rom = fopen(filename, "rb");
	if (rom) {
		fseek(rom, 0x10, SEEK_SET);
		for (int i = 0; i < 0x4000; i++) {
			u8 byte = (u8)fgetc(rom);
			mem[0x8000 - 0x4020 + i] = byte;
			mem[0x8000 - 0x4020 + 0x4000 + i] = byte;
		}
		return 1;
	}
	return 0;
}