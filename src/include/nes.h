#ifndef NES_H_
#define NES_H_

/* NES Memory Layout */

// $0000 - $00FF(256 bytes)   - Zero Page
// $0100 - $01FF(256 bytes)   - Stack memory
// $0200 - $07FF(1536 bytes)  - RAM
// $0800 - $0FFF(2048 bytes)  - Mirror of $000 - $07FF
// $1000 - $17FF(2048 bytes)  - Mirror of $000 - $07FF
// $1800 - $1FFF(2048 bytes)  - Mirror of $000 - $07FF
// $2000 - $2007(8 bytes)     - PPU registers
// $2008 - $3FFF(8184 bytes)  - Mirror of $2000 - $2007 (repeated)
// $4000 - $401F(32 bytes)    - I/O registers
// $4020 - $5FFF(8160 bytes)  - Expansion ROM
// $6000 - $7FFF(8192 bytes)  - SRAM
// $8000 - $FFFF(32768 bytes) - PRG-ROM
// $FFFA - $FFFB(2 bytes)     - NMI handler routine
// $FFFC - $FFFD(2 bytes)     - Power on reset handler routine
// $FFFE - $FFFF(2 bytes)     - IRQ/BRK handler routine

// RAM
#define STACK_OFFSET 0x100
// PPU
#define PPU_CTRL_OFFSET 0x2000
#define PPU_MASK_OFFSET 0x2001
#define PPU_STATUS_OFFSET 0x2002
#define OAM_ADDR_OFFSET 0x2003
#define OAM_DATA_OFFSET 0x2004
#define PPU_SCROLL_OFFSET 0x2005
#define PPU_ADDR_OFFSET 0x2006
#define PPU_DATA_OFFSET 0x2007
#define OAM_DMA_OFFSET 0x4014
// CARTRIDGE
#define PRG_RAM_OFFSET 0x6000
#define PRG_DATA_OFFSET 0x8000
// INTERRUPT
#define NMI_HANDLE_OFFSET 0xFFFA
#define RESET_HANDLE_OFFSET 0xFFFC
#define IRQ_BRK_HANDLE_OFFSET 0xFFFE

/* PPU Memory Layout */

// $0000 - $0FFF(4096 bytes)  - Pattern table 0
// $1000 - $1FFF(4096 bytes)  - Pattern table 1
// $2000 - $23FF(1024 bytes)  - Nametable 0
// $2400 - $27FF(1024 bytes)  - Nametable 1
// $2800 - $2BFF(1024 bytes)  - Nametable 2
// $2C00 - $2FFF(1024 bytes)  - Nametable 3
// $3000 - $3EFF(3840 bytes)  - Mirror of $2000 - $2EFF (repeated)
// $3F00 - $3F1F(32 bytes)    - Palette RAM indices
// $3F20 - $3FFF(224 bytes)   - Mirror of $3F00 - $3F1F (repeated)

/* NES Memory Sizes */

// RAM
#define RAM_SIZE 0x800
// PPU
#define PPU_REGISTER_FILE_SIZE 0x08
#define PPU_VRAM_SIZE 0x800
#define PPU_CGRAM_SIZE 0x20
#define OAM_PRIMARY_SIZE 0x100
#define OAM_SECONDARY_SIZE 0x20
// CARTRIDGE
#define HEADER_SIZE 0x10
#define PRG_DATA_UNIT_SIZE 0x4000
#define PRG_RAM_UNIT_SIZE 0x2000
#define PRG_SLOT_SIZE 0x2000
#define CHR_SLOT_SIZE 0x400

/* Register Masks*/

// PPUCTRL [-/W]
// [0:1]: Nametable base address (0: 0x2000, 1: 0x2400, 2: 0x2800, 3: 0x2C00)
// [2]  : VRAM address increment (0: add 1 [across], 1: add 32 [down])
// [3]  : Sprite pattern table address [8x8 only] (0: 0x0000, 1: 0x1000)
// [4]  : Background pattern table address (0: 0x0000, 1: 0x1000)
// [5]  : Sprite size (0: 8x8 pixels, 1: 8x16 pixels)
// [6]  : PPU master/slave select for EXT pins (0: slave, 1: master)
// [7]  : Generate NMI at start of vertical blank interval (0: off, 1: on)
#define PPU_CTRL_NT_ADDR_POS 0
#define PPU_CTRL_NT_ADDR_MSK 0x03 << PPU_CTRL_NT_ADDR_POS
#define PPU_CTRL_VRAM_INCR_POS 2
#define PPU_CTRL_VRAM_INCR_MSK 1 << PPU_CTRL_VRAM_INCR_POS
#define PPU_CTRL_SPR_PT_ADDR_POS 3
#define PPU_CTRL_SPR_PT_ADDR_MSK 1 << PPU_CTRL_SPR_PT_ADDR_POS
#define PPU_CTRL_BG_PT_ADDR_POS 4
#define PPU_CTRL_BG_PT_ADDR_MSK 1 << PPU_CTRL_BG_PT_ADDR_POS
#define PPU_CTRL_SPR_SIZE_POS 5
#define PPU_CTRL_SPR_SIZE_MSK 1 << PPU_CTRL_SPR_SIZE_POS
#define PPU_CTRL_MST_SLV_SEL_POS 6
#define PPU_CTRL_MST_SLV_SEL_MSK 1 << PPU_CTRL_MST_SLV_SEL_POS
#define PPU_CTRL_VBLANK_NMI_POS 7
#define PPU_CTRL_VBLANK_NMI_MSK 1 << PPU_CTRL_VBLANK_NMI_POS

// PPUMASK [-/W]
// [0]: Grayscale (0: normal color, 1: grayscale)
// [1]: Show background in leftmost 8 pixels of screen (0: hide, 1: show)
// [2]: Show spirtes in leftmost 8 pixels of screen (0: hide, 1: show)
// [3]: Show background (0: hide, 1: show)
// [4]: Show sprites (0: hide, 1: show)
// [5]: Emphasize red
// [6]: Emphasize green
// [7]: Emphasize blue
#define PPU_MASK_GRAYSCALE_POS 0
#define PPU_MASK_GRAYSCALE_MSK 1 << PPU_MASK_GRAYSCALE_POS
#define PPU_MASK_BG_LEFT_POS 1
#define PPU_MASK_BG_LEFT_MSK 1 << PPU_MASK_BG_LEFT_POS
#define PPU_MASK_SPR_LEFT_POS 2
#define PPU_MASK_SPR_LEFT_MSK 1 << PPU_MASK_SPR_LEFT_POS
#define PPU_MASK_BG_POS 3
#define PPU_MASK_BG_MSK 1 << PPU_MASK_BG_POS
#define PPU_MASK_SPR_POS 4
#define PPU_MASK_SPR_MSK 1 << PPU_MASK_SPR_POS
#define PPU_MASK_RED_POS 5
#define PPU_MASK_RED_MASK 1 << PPU_MASK_RED_POS
#define PPU_MASK_GREEN_POS 6
#define PPU_MASK_GREEN_MASK 1 << PPU_MASK_GREEN_POS
#define PPU_MASK_BLUE_POS 7
#define PPU_MASK_BLUE_MASK 1 << PPU_MASK_BLUE_POS

// PPUSTATUS [R/-]
// [0:4]: LSBs of previously written into PPU register
// [5]  : Sprite overflow (set at sprite evaluation, cleared at prerender dot 1)
// [6]  : Sprite 0 hit (sprite 0 overlaps bg, cleared at prerender dot 1)
// [7]  : Vertical blank has started (0: not in vblank, 1: in vblank)
#define PPU_STATUS_BUS_POS 0
#define PPU_STATUS_BUS_MSK 0x1F
#define PPU_STATUS_SPR_OVF_POS 5
#define PPU_STATUS_SPR_OVF_MSK 1 << PPU_STATUS_SPR_OVF_POS
#define PPU_STATUS_SPR_HIT_POS 6
#define PPU_STATUS_SPR_HIT_MSK 1 << PPU_STATUS_SPR_HIT_POS
#define PPU_STATUS_VBLANK_POS 7
#define PPU_STATUS_VBLANK_MSK 1 << PPU_STATUS_VBLANK_POS

// OAMADDR [-/W]
// [0:7]: OAM address to access

// OAMDATA [R/W]
// [0:7]: Read/Write OAM data from/to OAMADDR

// PPUSCROLL [-/W]
// [0:7]: Write once for horizontal offset, again for vertical offset

// PPUADDR [-/W]
// [0:7]: VRAM address to access, (upper byte->lower byte)

// PPUDATA [R/W]
// [0:7]: Read/Write data from/to PPUADDR

// OAMDMA [-/W]
// [0:7]: Copies 256 bytes of data from (0x[0:7]00 to 0x[0:7]FF) to PPU OAM

// Internal PPU Registers

// VRAM Address (scroll)
// [0:4]  : Coarse x scroll
// [5:9]  : Coarse y scroll
// [10:11]: Nametable select
// [12:14]: Fine y scroll
#define PPU_VRAM_COARSE_X_POS 0
#define PPU_VRAM_COARSE_X_MSK 0x1F << PPU_VRAM_COARSE_X_POS
#define PPU_DATA_COARSE_X_POS 3
#define PPU_DATA_COARSE_X_MSK 0x1F << PPU_DATA_COARSE_X_POS
#define PPU_VRAM_COARSE_Y_POS 5
#define PPU_VRAM_COARSE_Y_MSK 0x1F << PPU_VRAM_COARSE_Y_POS
#define PPU_DATA_COARSE_Y_POS 3
#define PPU_DATA_COARSE_Y_MSK 0x1F << PPU_DATA_COARSE_Y_POS
#define PPU_VRAM_NT_SEL_POS 10
#define PPU_VRAM_NT_SEL_MSK 0x03 << PPU_VRAM_NT_SEL_POS
#define PPU_DATA_NT_SEL_POS 0
#define PPU_DATA_NT_SEL_MSK 0x03 << PPU_DATA_NT_SEL_POS
#define PPU_VRAM_FINE_Y_POS 12
#define PPU_VRAM_FINE_Y_MSK 0x07 << PPU_VRAM_FINE_Y_POS
#define PPU_DATA_FINE_Y_POS 0
#define PPU_DATA_FINE_Y_MSK 0x07 << PPU_DATA_FINE_Y_POS

// VRAM Address (RAM)
#define PPU_VRAM_WIDTH 14
#define PPU_VRAM_BYTE_L_POS 0
#define PPU_VRAM_BYTE_L_MSK 0xFF << PPU_VRAM_BYTE_L_POS
#define PPU_DATA_BYTE_L_POS 0
#define PPU_DATA_BYTE_L_MSK 0xFF << PPU_DATA_BYTE_L_POS
#define PPU_VRAM_BYTE_H_POS 8
#define PPU_VRAM_BYTE_H_MSK 0x3F << PPU_VRAM_BYTE_H_POS
#define PPU_DATA_BYTE_H_POS 0
#define PPU_DATA_BYTE_H_MSK 0x3F << PPU_DATA_BYTE_H_POS

// Fine x scroll
// [0:2]: Fine x scroll
#define PPU_FINE_X_POS 0
#define PPU_FINE_X_MSK 0x07 << PPU_FINE_X_POS

/* NES Video Resolution */

/* Misc */
#define PPU_RESET_COMPLETE_CYCLE 29658 * 3

// DISPLAY
#define DISPLAY_WIDTH 256
#define DISPLAY_HEIGHT 240

#endif // NES_H_