#ifndef PPU_H_
#define PPU_H_

#include "types.h"

typedef enum {
    WRITE,
    READ,
} ppu_rw_t;

typedef enum {
    HORIZONTAL,
    VERTICAL,
} ppu_mirror_t;

typedef enum {
    VISIBLE,
    POST,
    NMI,
    PRE,
} ppu_scanline_t;

void ppu_set_mirror(ppu_mirror_t mode);
u16 ppu_nt_mirror(u16 addr);
u8 ppu_rd(u16 addr);
void ppu_wr(u16 addr, u8 v);
u8 ppu_reg_access(u16 index, u8 v, ppu_rw_t rw);
u16 ppu_get_nt_addr();
u16 ppu_get_at_addr();
u16 ppu_get_bg_addr();
void ppu_h_scroll();
void ppu_v_scroll();
void ppu_h_update();
void ppu_v_update();
void ppu_reload_shift();
void ppu_clear_oam();
void ppu_eval_sprites();
void ppu_load_sprites();
void ppu_update_pixels();
void ppu_tick_scanline(ppu_scanline_t type);
void ppu_tick();
void ppu_reset();

#endif /* PPU_H_ */
