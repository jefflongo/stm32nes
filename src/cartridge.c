#include "cartridge.h"

#include "bitmask.h"
#include "log.h"
#include "mapper0.h"
#include "nes.h"
#include "ppu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    u8 mapper;              // Mapper ID
    u8 prg_size;            // PRG size in 16kB units
    u8 chr_size;            // CHR size in 8kB units
    ppu_mirror_t mirroring; // Mirroring mode if no VRAM
    bool has_vram;          // Cart contains additional VRAM, ignore mirroring mode
    bool has_chr_ram;       // Cart contains additional CHR RAM, set if chr_size = 0
    bool has_prg_ram;       // Cart contains additional PRG RAM
    u8 prg_ram_size;        // Size of PRG RAM in 8kB units if available
} cartridge_config_t;

typedef struct {
    cartridge_config_t config;
    u8* rom;
    u8* prg;
    u8* prg_ram;
    u8* chr;
} cartridge_t;

static cartridge_t cart;

static u32 prg_map[4], chr_map[8];
// static u8 prg_bank, chr_bank;

cartridge_status_t cartridge_init(char* filename) {
    // Load ROM into memory
    FILE* rom_file = fopen(filename, "rb");
    if (!rom_file) {
        return CARTRIDGE_NOT_FOUND;
    }
    fseek(rom_file, 0L, SEEK_END);
    size_t rom_size = ftell(rom_file);
    cart.rom = malloc(rom_size * sizeof(u8));
    rewind(rom_file);
    fread(cart.rom, 1, rom_size, rom_file);
    fclose(rom_file);

    /* Header - 16 bytes */
    // 4 byte magic number
    if (memcmp(cart.rom, "NES\x1a", 4)) {
        return CARTRIDGE_UNSUPPORTED;
    }
    // PRG-ROM size in 16 kb blocks
    cart.config.prg_size = cart.rom[4];
    if (!cart.config.prg_size) {
        return CARTRIDGE_INVALID;
    }
    // CHR-ROM in 8 kb blocks
    if (cart.rom[5]) {
        cart.config.chr_size = cart.rom[5];
    } else {
        cart.config.chr_size = 1;
        cart.config.has_chr_ram = true;
    }
    // Flags 6
    // PPU nametable mirroring style
    cart.config.mirroring = NTH_BIT(cart.rom[6], 0);
    ppu_set_mirror(cart.config.mirroring);
    // Presence of PRG RAM
    cart.config.has_prg_ram = NTH_BIT(cart.rom[6], 1);
    // 512 byte trainer before PRG data
    if (NTH_BIT(cart.rom[6], 2)) {
        return CARTRIDGE_UNSUPPORTED;
    }
    // Ignore nametable mirroring, provide 4-screen VRAM
    cart.config.has_vram = NTH_BIT(cart.rom[6], 3);
    // Flags 7
    // Mapper lower nybble from flags 6, mapper upper nybble from flags 7
    cart.config.mapper = (cart.rom[6] >> 4) | (cart.rom[7] & 0xF0);
    // Flags 8
    // PRG RAM size
    cart.config.prg_ram_size = (cart.rom[8] != 0) ? cart.rom[8] : 1;
    // Flags 9
    // NTSC or PAL
    if (cart.rom[9] != 0) {
        return CARTRIDGE_UNSUPPORTED;
    }
    // Flags 10-15 unused

    // Load PRG data
    cart.prg = cart.rom + NES_HEADER_SIZE;

    // Load CHR data
    cart.chr = cart.rom + NES_HEADER_SIZE + cart.config.prg_size * NES_PRG_DATA_UNIT_SIZE;
    // Allocate PRG RAM
    if (cart.config.has_prg_ram)
        cart.prg_ram = malloc(cart.config.prg_ram_size * NES_PRG_RAM_UNIT_SIZE * sizeof(u8));

    switch (cart.config.mapper) {
        case 0:
            mapper0_init((u32*)&prg_map, (u32*)&chr_map, cart.config.prg_size);
            break;
        default:
            LOG("Mapper %d not supported.\n", cart.config.mapper);
            return CARTRIDGE_UNSUPPORTED;
    }
    return CARTRIDGE_SUCCESS;
}

u8 cartridge_prg_rd(u16 addr) {
    if (addr >= NES_PRG_DATA_OFFSET) {
        int slot = (addr - NES_PRG_DATA_OFFSET) / NES_PRG_SLOT_SIZE;
        int offset = (addr - NES_PRG_DATA_OFFSET) % NES_PRG_SLOT_SIZE;
        return cart.prg[prg_map[slot] + offset];
    } else {
        return cart.config.has_prg_ram ? cart.prg_ram[addr - NES_PRG_RAM_OFFSET] : 0;
    }
}

u8 cartridge_chr_rd(u16 addr) {
    int slot = addr / NES_CHR_SLOT_SIZE;
    int offset = addr % NES_CHR_SLOT_SIZE;
    return cart.chr[chr_map[slot] + offset];
}

void cartridge_prg_wr(u16 addr, u8 data) {
    // Use mapper's write implementation
}

void cartridge_chr_wr(u16 addr, u8 data) {
    if (cart.config.has_chr_ram) cart.chr[addr] = data;
}

void reset(void) {
    free(cart.rom);
    if (cart.config.has_prg_ram) free(cart.prg_ram);
}