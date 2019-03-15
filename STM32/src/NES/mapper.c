#include "NES/mapper.h"

static u8 *prg, *chr, *prgRam = NULL;
static u8 prgBank, chrBank = 0;
static int prgSize, chrSize, prgRamSize;
static bool hasChrRam, hasPrgRam, vram = false;
static u8 mapper;

u8 mapper_rd(u16 addr) {
	if (addr >= 0x8000) {
		return *(prg + ((addr - 0x8000 + prgBank * 0x4000) % (prgSize * 0x4000)));
	}
	else {
		return hasPrgRam ? *(prgRam + addr - 0x6000) : 0;
	}
}

void mapper_wr(u16 addr, u8 data) {
	// Use mapper's write implementation
}

u8 chr_rd(u16 addr) {
	return *(chr + addr + chrBank * 0x2000);
}

void chr_wr(u16 addr, u8 data) {
	if (hasChrRam) *(chr + addr) = data;
}

int loadRomFromSD(FATFS* fs, char* filename) {
	FIL rom;
	UINT r;
	if (f_mount(fs, "", 0) != FR_OK) return -1;
	if (f_open(&rom, filename, FA_READ) != FR_OK) return -2;
	/* Header - 16 bytes */
	// 4 byte magic number
	char magicNumber[5];
	f_gets(magicNumber, 5, &rom);
	if (strcmp(magicNumber, "NES\x1a")) return -3;
	// PRG-ROM size in 16 kb blocks
	u8 byte;
	f_read(&rom, &byte, 1, &r);
	prgSize = byte;
	if (prgSize <= 0) return -4;
	// CHR-ROM in 8 kb blocks
	f_read(&rom, &byte, 1, &r);
	if (byte != 0) {
		chrSize = byte;
	}
	else {
		chrSize = 1;
		hasChrRam = true;
	}
	// Flags 6
	f_read(&rom, &byte, 1, &r);
	// PPU nametable mirroring style
	/*** Set PPU mirroring here, (byte & 0x01) ? vertical : horizontal ***/
	// Presence of PRG RAM
	hasPrgRam = ((byte & 0x02) >> 1) ? true : false;
	// 512 byte trainer before PRG data
	if ((byte & 0x04) >> 2) return -5;
	// Ignore nametable mirroring, provide 4-screen VRAM
	vram = ((byte & 0x08) >> 3) ? true : false;
	// Mapper lower nybble
	mapper = byte >> 4;
	// Flags 7
	f_read(&rom, &byte, 1, &r);
	// Mapper upper nybble
	mapper |= (byte & 0xF0);
	// Flags 8
	f_read(&rom, &byte, 1, &r);
	// PRG RAM size
	prgRamSize = (byte != 0) ? byte : 1;
	// Flags 9
	f_read(&rom, &byte, 1, &r);
	// NTSC or PAL
	if (byte != 0) return -6;
	// Flags 10
	f_read(&rom, &byte, 1, &r);
	// Flags 11-15
	f_read(&rom, &byte, 1, &r);
	f_read(&rom, &byte, 1, &r);
	f_read(&rom, &byte, 1, &r);
	f_read(&rom, &byte, 1, &r);
	f_read(&rom, &byte, 1, &r);

	// Load PRG data
	prg = malloc(prgSize * 0x4000 * sizeof(u8));
	for (int i = 0; i < prgSize * 0x4000; i++) {
		f_read(&rom, &byte, 1, &r);
		*(prg + i) = byte;
	}
	// Load CHR data
	chr = malloc(chrSize * 0x2000 * sizeof(u8));
	for (int i = 0; i < chrSize * 0x2000; i++) {
		f_read(&rom, &byte, 1, &r);
		*(chr + i) = byte;
	}
	// Allocate PRG RAM
	if (hasPrgRam) prgRam = malloc(prgRamSize * 0x2000 * sizeof(u8));

	f_close(&rom);

	switch (mapper) {
		case 0:
			mapper0_init(&prgBank, &chrBank);
			break;
		default:
			printf("Mapper not supported!\n");
			return -7;
	}
	return 0;
}

void reset() {
	free(prg);
	free(chr);
	free(prgRam);
}
