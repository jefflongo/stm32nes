#include "cartridge.h"

#include "bitmask.h"
#include "log.h"
#include "mappers/mapper0.h"
#include "nes.h"
#include "ppu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cartridge_result_t cartridge_init(nes_t* nes, char const* filename) {
    // Load ROM into memory
    FILE* rom_file = fopen(filename, "rb");
    if (!rom_file) {
        return CARTRIDGE_NOT_FOUND;
    }
    fseek(rom_file, 0L, SEEK_END);
    size_t rom_size = ftell(rom_file);
    nes->cartridge.rom = malloc(rom_size * sizeof(u8));
    rewind(rom_file);
    fread(nes->cartridge.rom, 1, rom_size, rom_file);
    fclose(rom_file);

    /* Header - 16 bytes */
    // 4 byte magic number
    if (memcmp(nes->cartridge.rom, "NES\x1a", 4)) {
        return CARTRIDGE_UNSUPPORTED;
    }
    // PRG-ROM size in 16 kb blocks
    nes->cartridge.config.prg_size = nes->cartridge.rom[4];
    if (!nes->cartridge.config.prg_size) {
        return CARTRIDGE_INVALID;
    }
    // CHR-ROM in 8 kb blocks
    if (nes->cartridge.rom[5]) {
        nes->cartridge.config.chr_size = nes->cartridge.rom[5];
    } else {
        nes->cartridge.config.chr_size = 1;
        nes->cartridge.config.has_chr_ram = true;
    }
    // Flags 6
    // PPU nametable mirroring style
    // TODO
    // nes->cartridge.config.mirroring = NTH_BIT(nes->cartridge.rom[6], 0);
    // ppu_set_mirror(nes->cartridge.config.mirroring);
    // Presence of PRG RAM
    nes->cartridge.config.has_prg_ram = NTH_BIT(nes->cartridge.rom[6], 1);
    // 512 byte trainer before PRG data
    if (NTH_BIT(nes->cartridge.rom[6], 2)) {
        return CARTRIDGE_UNSUPPORTED;
    }
    // Ignore nametable mirroring, provide 4-screen VRAM
    nes->cartridge.config.has_vram = NTH_BIT(nes->cartridge.rom[6], 3);
    // Flags 7
    // Mapper lower nybble from flags 6, mapper upper nybble from flags 7
    nes->cartridge.config.mapper = (nes->cartridge.rom[6] >> 4) | (nes->cartridge.rom[7] & 0xF0);
    // Flags 8
    // PRG RAM size
    nes->cartridge.config.prg_ram_size = (nes->cartridge.rom[8] != 0) ? nes->cartridge.rom[8] : 1;
    // Flags 9
    // NTSC or PAL
    if (nes->cartridge.rom[9] != 0) {
        return CARTRIDGE_UNSUPPORTED;
    }
    // Flags 10-15 unused

    // Load PRG data
    nes->cartridge.prg = nes->cartridge.rom + NES_HEADER_SIZE;

    // Load CHR data
    nes->cartridge.chr = nes->cartridge.rom + NES_HEADER_SIZE +
                         nes->cartridge.config.prg_size * NES_PRG_DATA_UNIT_SIZE;
    // Allocate PRG RAM
    if (nes->cartridge.config.has_prg_ram)
        nes->cartridge.prg_ram =
          malloc(nes->cartridge.config.prg_ram_size * NES_PRG_RAM_UNIT_SIZE * sizeof(u8));

    switch (nes->cartridge.config.mapper) {
        case 0:
            mapper0_init(
              nes->cartridge.prg_map, nes->cartridge.chr_map, nes->cartridge.config.prg_size);
            break;
        default:
            LOG("Mapper %d not supported.\n", nes->cartridge.config.mapper);
            return CARTRIDGE_UNSUPPORTED;
    }
    return CARTRIDGE_SUCCESS;
}

u8 cartridge_prg_rd(nes_t* nes, u16 addr) {
    if (addr >= NES_PRG_DATA_OFFSET) {
        int slot = (addr - NES_PRG_DATA_OFFSET) / NES_PRG_SLOT_SIZE;
        int offset = (addr - NES_PRG_DATA_OFFSET) % NES_PRG_SLOT_SIZE;
        return nes->cartridge.prg[nes->cartridge.prg_map[slot] + offset];
    } else {
        return nes->cartridge.config.has_prg_ram ? nes->cartridge.prg_ram[addr - NES_PRG_RAM_OFFSET]
                                                 : 0;
    }
}

u8 cartridge_chr_rd(nes_t* nes, u16 addr) {
    int slot = addr / NES_CHR_SLOT_SIZE;
    int offset = addr % NES_CHR_SLOT_SIZE;
    return nes->cartridge.chr[nes->cartridge.chr_map[slot] + offset];
}

void cartridge_prg_wr(nes_t* nes, u16 addr, u8 data) {
    // TODO: Use mapper's write implementation
    (void)nes;
    (void)addr;
    (void)data;
}

void cartridge_chr_wr(nes_t* nes, u16 addr, u8 data) {
    if (nes->cartridge.config.has_chr_ram) {
        nes->cartridge.chr[addr] = data;
    }
}

void reset(nes_t* nes) {
    free(nes->cartridge.rom);
    if (nes->cartridge.config.has_prg_ram) {
        free(nes->cartridge.prg_ram);
    }
}
