#include "memory.h"

#include "cartridge.h"

void memory_init(nes_t* state) {
    for (size_t i = 0; i < NES_RAM_SIZE; i++) {
        state->memory.ram[i] = 0x00;
    }
}

u8 memory_read(nes_t* state, u16 addr) {
    if (addr < 0x2000) {
        return state->memory.ram[addr % NES_RAM_SIZE];
    } else if (addr < 0x4000) {
        // TODO: return ppu_reg_access(addr % 0x100, 0, READ);
        return 0;
    } else if (addr <= 0x4015) {
        // TODO: APU, Peripherals..
        return 0;
    } else if (addr == 0x4016) {
        // TODO: return controller_rd(0);
        return 0;
    } else if (addr == 0x4017) {
        // TODO: return controller_rd(1);
        return 0;
    } else {
        return cartridge_prg_rd(state, addr);
    }
}

void memory_write(nes_t* state, u16 addr, u8 data) {
    if (addr < 0x2000) {
        state->memory.ram[addr % NES_RAM_SIZE] = data;
    } else if (addr < 0x4000) {
        // TODO: ppu_reg_access(addr % 0x100, 0, WRITE);
    } else if (addr <= 0x4015) {
        // TODO: APU, Peripherals..
    } else if (addr == 0x4016) {
        // TODO: controller_wr(data);
    } else if (addr == 0x4017) {
        // TODO
    } else {
        cartridge_prg_wr(state, addr, data);
    }
}
