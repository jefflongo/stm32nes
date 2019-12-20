#include "NES/mapper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static u8 *prg, *chr, *prg_ram = NULL;
static u8 prg_bank, chr_bank = 0;
static int prg_size, chr_size, prg_ram_size;
static bool has_chr_ram, has_prg_ram, vram = false;
static u8 mapper;

u8 mapper_rd(u16 addr) {
    if (addr >= 0x8000) {
        return *(
          prg + ((addr - 0x8000 + prg_bank * 0x4000) % (prg_size * 0x4000)));
    } else {
        return has_prg_ram ? *(prg_ram + addr - 0x6000) : 0;
    }
}

void mapper_wr(u16 addr, u8 data) {
    // Use mapper's write implementation
}

u8 chr_rd(u16 addr) {
    return *(chr + addr + chr_bank * 0x2000);
}

void chr_wr(u16 addr, u8 data) {
    if (has_chr_ram) *(chr + addr) = data;
}

int load_rom_from_sd(FATFS* fs, char* filename) {
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
    prg_size = byte;
    if (prg_size <= 0) return -4;
    // CHR-ROM in 8 kb blocks
    f_read(&rom, &byte, 1, &r);
    if (byte != 0) {
        chr_size = byte;
    } else {
        chr_size = 1;
        has_chr_ram = true;
    }
    // Flags 6
    f_read(&rom, &byte, 1, &r);
    // PPU nametable mirroring style
    /*** Set PPU mirroring here, (byte & 0x01) ? vertical : horizontal ***/
    // Presence of PRG RAM
    has_prg_ram = ((byte & 0x02) >> 1) ? true : false;
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
    prg_ram_size = (byte != 0) ? byte : 1;
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
    prg = malloc(prg_size * 0x4000 * sizeof(u8));
    for (int i = 0; i < prg_size * 0x4000; i++) {
        f_read(&rom, &byte, 1, &r);
        *(prg + i) = byte;
    }
    // Load CHR data
    chr = malloc(chr_size * 0x2000 * sizeof(u8));
    for (int i = 0; i < chr_size * 0x2000; i++) {
        f_read(&rom, &byte, 1, &r);
        *(chr + i) = byte;
    }
    // Allocate PRG RAM
    if (has_prg_ram) prg_ram = malloc(prg_ram_size * 0x2000 * sizeof(u8));

    f_close(&rom);

    switch (mapper) {
        case 0:
            mapper0_init(&prg_bank, &chr_bank);
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
    free(prg_ram);
}
