#ifndef PPU_H_
#define PPU_H_

#include "types.h"

typedef enum {
    HORIZONTAL,
    VERTICAL,
} ppu_mirror_t;

typedef struct {
    u8 ppu_ctrl;
    u8 ppu_mask;
    u8 ppu_status;
    u8 oam_addr;
    u8 oam_dma;
    // Internal registers
    u8 bus_latch;
    u16 addr_latch;      // 15 bit temporary VRAM address
    u16 vram_addr;       // 15 bit VRAM address
    bool addr_latch_sel; // Selects address latch bits to modify
    u8 fine_x;           // 3 bit fine x scroll
} ppu_registers_t;

typedef struct {
    ppu_registers_t reg;
    ppu_mirror_t mirror;
    bool ready;
    u64 cycle;
} ppu_state_t;

void ppu_init(void);
void ppu_reset(void);
void ppu_tick(void);
u8 ppu_rd(u16 addr);
void ppu_wr(u16 addr, u8 data);
void ppu_set_mirror(ppu_mirror_t mirror);

#endif /* PPU_H_ */
