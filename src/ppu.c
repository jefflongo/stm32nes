#include "ppu.h"

#include "bitmask.h"
#include "nes.h"

#include <stdio.h>

typedef enum {
    VISIBLE,
    POST,
    NMI,
    PRE,
} ppu_scanline_t;

typedef struct {
    u8 r;
    u8 g;
    u8 b;
} ppu_palette_t;

typedef struct {
    u8 y;
    u8 tile;
    u8 attr;
    u8 x;
} ppu_sprite_t;

// Registers, cycle tracker
static ppu_state_t ppu;
// Internal PPU memory
static u8 vram[PPU_VRAM_SIZE];               // PPU RAM
static u8 oam[OAM_PRIMARY_SIZE];             // Sprite memory
static u8 oam_secondary[OAM_SECONDARY_SIZE]; // Sprite scanline buffer
// const static ppu_palette_t palette[64]

static inline void increment_ppu_addr(void) {
    ppu.reg.vram_addr +=
      NTH_BIT(ppu.reg.ppu_ctrl, PPU_CTRL_VRAM_INCR_POS) ? 32 : 1;
}

static void rd_ppu_status(u8* data) {
    // 5 LSBs determined by bus latch
    *data = ppu.reg.ppu_status |
            BITS(ppu.reg.bus_latch, PPU_STATUS_BUS_MSK, PPU_STATUS_BUS_POS);
    // Update bus latch
    ppu.reg.bus_latch = *data;
    // Clear VBLANK bit
    CLEAR_NTH_BIT(ppu.reg.ppu_status, PPU_STATUS_VBLANK_POS);
    // Clear address latch (used by PPUSCROLL and PPUADDR)
    ppu.reg.addr_latch_sel = 0;
}

static void rd_oam_data(u8* data) {
    // TODO: some edge cases while rendering
    *data = oam[ppu.reg.oam_addr];
    // Update bus latch
    ppu.reg.bus_latch = *data;
}

static void rd_ppu_data(u8* data) {
    *data = vram[ppu.reg.vram_addr];
    // VRAM access increments VRAM address
    increment_ppu_addr();
    // Update bus latch
    ppu.reg.bus_latch = *data;
}

static void wr_ppu_status(u8 data) {
    if (ppu.ready) {
        ppu.reg.ppu_ctrl = data;
        // t: ...BA.. ........ = d: ......BA
        ASSIGN_BITS(
          ppu.reg.addr_latch,
          PPU_VRAM_NT_SEL_MSK,
          PPU_VRAM_NT_SEL_POS,
          BITS(data, PPU_DATA_NT_SEL_MSK, PPU_DATA_NT_SEL_POS));
    }
}

static void wr_ppu_mask(u8 data) {
    if (ppu.ready) ppu.reg.ppu_mask = data;
}

static void wr_oam_addr(u8 data) {
    ppu.reg.oam_addr = data;
}

static void wr_oam_data(u8 data) {
    // TODO: glitchy writes in sprite evaluation
    oam[ppu.reg.oam_addr++] = data;
}

static void wr_ppu_scroll(u8 data) {
    if (ppu.ready) {
        if (!ppu.reg.addr_latch_sel) {
            // t: ....... ...HGFED = d: HGFED...
            ASSIGN_BITS(
              ppu.reg.addr_latch,
              PPU_VRAM_COARSE_X_MSK,
              PPU_VRAM_COARSE_X_POS,
              BITS(data, PPU_DATA_COARSE_X_MSK, PPU_DATA_COARSE_X_POS));
            // x:              CBA = d: .....CBA
            ASSIGN_BITS(
              ppu.reg.fine_x,
              PPU_FINE_X_MSK,
              PPU_FINE_X_POS,
              BITS(data, PPU_FINE_X_MSK, PPU_FINE_X_POS));
        } else {
            // t: CBA..HG FED..... = d: HGFEDCBA
            ASSIGN_BITS(
              ppu.reg.addr_latch,
              PPU_VRAM_COARSE_Y_MSK,
              PPU_VRAM_COARSE_Y_POS,
              BITS(data, PPU_DATA_COARSE_Y_MSK, PPU_DATA_COARSE_Y_POS));
            ASSIGN_BITS(
              ppu.reg.addr_latch,
              PPU_VRAM_FINE_Y_MSK,
              PPU_VRAM_FINE_Y_POS,
              BITS(data, PPU_DATA_FINE_Y_MSK, PPU_DATA_FINE_Y_POS));
        }
        ppu.reg.addr_latch_sel = !ppu.reg.addr_latch_sel;
    }
}

static void wr_ppu_addr(u8 data) {
    if (ppu.ready) {
        if (!ppu.reg.addr_latch_sel) {
            // t: .FEDCBA ........ = d: ..FEDCBA
            // t: X...... ........ = 0
            ASSIGN_BITS(
              ppu.reg.addr_latch,
              PPU_VRAM_BYTE_H_MSK,
              PPU_VRAM_BYTE_H_POS,
              BITS(data, PPU_DATA_BYTE_H_MSK, PPU_DATA_BYTE_H_POS));
            CLEAR_NTH_BIT(ppu.reg.addr_latch, PPU_VRAM_WIDTH + 1);
        } else {
            // t: ....... HGFEDCBA = d: HGFEDCBA
            // v                   = t
            ASSIGN_BITS(
              ppu.reg.addr_latch,
              PPU_VRAM_BYTE_L_MSK,
              PPU_VRAM_BYTE_L_POS,
              data);
            ppu.reg.vram_addr = ppu.reg.addr_latch;
        }
        ppu.reg.addr_latch_sel = !ppu.reg.addr_latch_sel;
    }
}

static void wr_ppu_data(u8 data) {
    vram[ppu.reg.vram_addr] = data;
    // VRAM access increments VRAM address
    increment_ppu_addr();
}

void ppu_init(void) {
    ppu.ready = false;
    ppu.cycle = 0;

    ppu.reg.ppu_ctrl = 0x00;
    ppu.reg.ppu_mask = 0x00;
    ppu.reg.ppu_status = 0x00;
    ppu.reg.oam_addr = 0x00;
    ppu.reg.oam_dma = 0x00;
    ppu.reg.bus_latch = 0x00;
    ppu.reg.addr_latch = 0x00;
    ppu.reg.vram_addr = 0x00;
    ppu.reg.fine_x = 0x00;
}

void ppu_reset(void) {
    ppu.ready = false;
    ppu.cycle = 0;

    ppu.reg.ppu_ctrl = 0x00;
    ppu.reg.ppu_mask = 0x00;
    CLEAR_BITS(ppu.reg.ppu_status, 0x7F);
    // OAM_ADDR unchanged
    // PPU_ADDR unchanged
    ppu.reg.oam_dma = 0x00;
    ppu.reg.bus_latch = 0x00;
    ppu.reg.addr_latch = 0x00;
    ppu.reg.vram_addr = 0x00;
    ppu.reg.fine_x = 0x00;
}

void ppu_tick(void) {
    if (++ppu.cycle > PPU_RESET_COMPLETE_CYCLE) ppu.ready = true;
}

u8 ppu_rd(u16 addr) {
    u8 data;
    switch (addr) {
        // Valid reads update the bus latch
        case PPU_STATUS_OFFSET:
            rd_ppu_status(&data);
        case OAM_DATA_OFFSET:
            rd_oam_data(&data);
            break;
        case PPU_DATA_OFFSET:
            rd_ppu_data(&data);
            break;
        default:
            // Reading a write-only register returns the current latch value
            data = ppu.reg.bus_latch;
            break;
    }
    return data;
}

void ppu_wr(u16 addr, u8 data) {
    switch (addr) {
        case PPU_CTRL_OFFSET:
            wr_ppu_status(data);
            break;
        case PPU_MASK_OFFSET:
            wr_ppu_mask(data);
            break;
        case OAM_ADDR_OFFSET:
            wr_oam_addr(data);
            break;
        case OAM_DATA_OFFSET:
            wr_oam_data(data);
            break;
        case PPU_SCROLL_OFFSET:
            wr_ppu_scroll(data);
            break;
        case PPU_ADDR_OFFSET:
            wr_ppu_addr(data);
            break;
        case PPU_DATA_OFFSET:
            wr_ppu_data(data);
            break;
        case OAM_DMA_OFFSET:
            // TODO:
            break;
        default:
            break;
    }
    // Any write operation updates the bus latch
    ppu.reg.bus_latch = data;
}

void ppu_set_mirror(ppu_mirror_t mirror) {
    ppu.mirror = mirror;
}
