#include "cpu.h"

#include "bitmask.h"
#include "log.h"
#include "memory.h"
#include "nes.h"

// For passing addressing modes as instruction arguments
typedef u16 (*mode)();

// Processor status flag definitions
enum {
    STATUS_CARRY,       // [0] C: Carry flag
    STATUS_ZERO,        // [1] Z: Zero flag
    STATUS_INT_DISABLE, // [2] I: Interrupt disable
    STATUS_DECIMAL,     // [3] D: Decimal mode, can be set/cleared but not used
    STATUS_BREAK,       // [4] B: Break command
    STATUS_UNUSED,      // [5] -: Not used, wired to 1
    STATUS_OVERFLOW,    // [6] V: Overflow flag
    STATUS_NEGATIVE,    // [7] N: Negative flag
};

static void tick(nes_t* state) {
    // TODO:
    // ppu_tick(state);
    // ppu_tick(state);
    // ppu_tick(state);
    state->cpu.cycle++;
}

/* Stack operations */

static void push(nes_t* state, u8 data) {
    memory_write(state, NES_STACK_OFFSET | state->cpu.s--, data);
}

static u8 pull(nes_t* state) {
    return memory_read(state, NES_STACK_OFFSET | ++state->cpu.s);
}

/* Flag adjustment */

static inline void update_c(nes_t* state, u16 r) {
    ASSIGN_NTH_BIT(state->cpu.p, STATUS_CARRY, r > 0xFF);
}

static inline void update_z(nes_t* state, u8 d) {
    ASSIGN_NTH_BIT(state->cpu.p, STATUS_ZERO, d == 0);
}

static inline void update_v(nes_t* state, u8 d1, u8 d2, u16 r) {
    ASSIGN_NTH_BIT(state->cpu.p, STATUS_OVERFLOW, (0xFF ^ d1 ^ d2) & (d1 ^ r) & 0x80);
}

static inline void update_n(nes_t* state, u8 d) {
    ASSIGN_NTH_BIT(state->cpu.p, STATUS_NEGATIVE, NTH_BIT(d, 7));
}

/* Interrupts */

static void interrupt_nmi(nes_t* state) {
    // Throw away fetched instruction
    tick(state);
    // Suppress PC increment
    tick(state);
    push(state, state->cpu.pc >> 8);
    tick(state);
    push(state, state->cpu.pc & 0xFF);
    tick(state);
    push(state, state->cpu.p | (1 << STATUS_UNUSED));
    tick(state);
    SET_NTH_BIT(state->cpu.p, STATUS_INT_DISABLE);
    u8 addrl = memory_read(state, NES_NMI_HANDLE_OFFSET);
    tick(state);
    u8 addrh = memory_read(state, NES_NMI_HANDLE_OFFSET + 1);
    state->cpu.pc = addrl | (addrh << 8);
    // CPU clears NMI after handling
    cpu_set_nmi(state, 0);
    tick(state);
}

static void interrupt_reset(nes_t* state) {
    // Throw away fetched instruction
    tick(state);
    // Suppress PC increment
    tick(state);
    // Suppress the 3 writes to the stack
    state->cpu.s -= 3;
    tick(state);
    tick(state);
    tick(state);
    SET_NTH_BIT(state->cpu.p, STATUS_INT_DISABLE);
    u8 addrl = memory_read(state, NES_RESET_HANDLE_OFFSET);
    tick(state);
    u8 addrh = memory_read(state, NES_RESET_HANDLE_OFFSET + 1);
    state->cpu.pc = addrl | (addrh << 8);
    tick(state);
}

static void interrupt_irq(nes_t* state) {
    // Throw away fetched instruction
    tick(state);
    // Suppress PC increment
    tick(state);
    push(state, state->cpu.pc >> 8);
    tick(state);
    push(state, state->cpu.pc & 0xFF);
    tick(state);
    push(state, state->cpu.p | (1 << STATUS_UNUSED));
    tick(state);
    SET_NTH_BIT(state->cpu.p, STATUS_INT_DISABLE);
    u8 addrl = memory_read(state, NES_IRQ_BRK_HANDLE_OFFSET);
    tick(state);
    u8 addrh = memory_read(state, NES_IRQ_BRK_HANDLE_OFFSET + 1);
    state->cpu.pc = addrl | (addrh << 8);
    tick(state);
}

static void BRK(nes_t* state) {
    state->cpu.pc++;
    tick(state);
    push(state, state->cpu.pc >> 8);
    tick(state);
    push(state, state->cpu.pc & 0xFF);
    push(state, state->cpu.p | (1 << STATUS_BREAK) | (1 << STATUS_UNUSED));
    tick(state);
    SET_NTH_BIT(state->cpu.p, STATUS_INT_DISABLE);
    u8 addrl = memory_read(state, NES_IRQ_BRK_HANDLE_OFFSET);
    tick(state);
    u8 addrh = memory_read(state, NES_IRQ_BRK_HANDLE_OFFSET + 1);
    state->cpu.pc = addrl | (addrh << 8);
    tick(state);
}

/* Addressing modes */

// Immediate:
// - Return current PC and increment PC (immediate stored here)
static u16 addr_imm(nes_t* state) {
    return state->cpu.pc++;
}

// ZP:
// - Read the immediate, increment PC
// - Return the immediate
static u16 addr_zp(nes_t* state) {
    u16 addr = memory_read(state, addr_imm(state));
    tick(state);
    return addr;
}

// ZP, X:
// - Read the immediate, increment PC
// - Calculate imm + X, include wraparound
// - Return the new address
static u16 addr_zpx(nes_t* state) {
    u16 addr = (addr_zp(state) + state->cpu.x) % 0x100;
    tick(state);
    return addr;
}

// ZP, Y:
// - Read the immediate, increment PC
// - Calculate imm + Y, include wraparound
// - Return the new address
static u16 addr_zpy(nes_t* state) {
    u16 addr = (addr_zp(state) + state->cpu.y) % 0x100;
    tick(state);
    return addr;
}

// Absolute:
// - Read the immediate, increment PC
// - Merge new immediate with old immediate, increment PC
// - Return the merged address
static u16 addr_absl(nes_t* state) {
    u8 addrl = (u8)addr_zp(state);
    u8 addrh = (u8)addr_zp(state);
    return addrl | (addrh << 8);
}

// Absolute, X:
// - Read the immediate, increment PC
// - Read the new immediate, add the old immediate with X, increment PC
// - If the sum of old imm and X overflows, reread the address next tick
// - Merge old imm + X with new imm, return the merged address
static u16 addr_absx_rd(nes_t* state) {
    u16 addrl = addr_zp(state);
    u8 addrh = memory_read(state, addr_imm(state));
    addrl += state->cpu.x;
    tick(state);
    if ((addrl & 0xFF00) != 0) {
        addrl %= 0x100;
        addrh++;
        tick(state);
    }
    return addrl | (addrh << 8);
}

// Must incur a tick regardless of page boundary cross
static u16 addr_absx_wr(nes_t* state) {
    u16 addrl = addr_zp(state);
    u8 addrh = memory_read(state, addr_imm(state));
    addrl += state->cpu.x;
    tick(state);
    if ((addrl & 0xFF00) != 0) {
        addrl %= 0x100;
        addrh++;
    }
    tick(state);
    return addrl | (addrh << 8);
}

// Absolute, Y:
// - Read the immediate, increment PC
// - Read the new immediate, add the old immediate with Y, increment PC
// - If the sum of old imm and Y overflows, reread the address next tick
// - Merge old imm + Y with new imm, return the merged address
static u16 addr_absy_rd(nes_t* state) {
    u16 addrl = addr_zp(state);
    u8 addrh = memory_read(state, addr_imm(state));
    addrl += state->cpu.y;
    tick(state);
    if ((addrl & 0xFF00) != 0) {
        addrl %= 0x100;
        addrh++;
        tick(state);
    }
    return addrl | (addrh << 8);
}

// Must incur a tick regardless of page boundary cross
static u16 addr_absy_wr(nes_t* state) {
    u16 addrl = addr_zp(state);
    u8 addrh = memory_read(state, addr_imm(state));
    addrl += state->cpu.y;
    tick(state);
    if ((addrl & 0xFF00) != 0) {
        addrl %= 0x100;
        addrh++;
    }
    tick(state);
    return addrl | (addrh << 8);
}

// Absolute Indirect (JMP only):
// - Read imm (pointer low), increment PC
// - Read imm (pointer high), increment PC
// - Read low byte from pointer
// - Read high byte from pointer (wrap around) and return the merged address
static u16 addr_ind(nes_t* state) {
    u8 ptrl = (u8)addr_zp(state);
    u8 ptrh = (u8)addr_zp(state);
    u16 ptr = ptrl | (ptrh << 8);
    u8 addrl = memory_read(state, ptr);
    tick(state);
    u8 addrh = memory_read(state, (ptr & 0xFF00) | ((ptr + 1) % 0x100));
    tick(state);
    return addrl | (addrh << 8);
}

// X, Indirect (Indexed Indirect):
// - Read imm (pointer), increment PC
// - Read address at imm + X on zero page
// - Read low byte from pointer
// - Read high byte from pointer and return the merged address
static u16 addr_xind(nes_t* state) {
    u8 ptr = (u8)addr_zpx(state);
    u8 addrl = memory_read(state, ptr);
    tick(state);
    u8 addrh = memory_read(state, (ptr + 1) % 0x100);
    tick(state);
    return addrl | (addrh << 8);
}

// Indirect, Y (Indirect Indexed):
// - Read imm (pointer), increment PC
// - Read low byte from pointer on zero page
// - Read high byte from pointer on zero page, add Y to low byte
// - If the sum of low byte and X overflows, reread the address next tick
// - Return the merged address
static u16 addr_indy_rd(nes_t* state) {
    u8 ptr = (u8)addr_zp(state);
    u16 addrl = memory_read(state, ptr);
    tick(state);
    u8 addrh = memory_read(state, (ptr + 1) % 0x100);
    addrl += state->cpu.y;
    tick(state);
    if ((addrl & 0xFF00) != 0) {
        addrl %= 0x100;
        addrh = (addrh + 1);
        tick(state);
    }
    return addrl | (addrh << 8);
}

// Must incur a tick regardless of page boundary cross
static u16 addr_indy_wr(nes_t* state) {
    u8 ptr = (u8)addr_zp(state);
    u16 addrl = memory_read(state, ptr);
    tick(state);
    u8 addrh = memory_read(state, (ptr + 1) % 0x100);
    addrl += state->cpu.y;
    tick(state);
    if ((addrl & 0xFF00) != 0) {
        addrl %= 0x100;
        addrh = (addrh + 1);
    }
    tick(state);
    return addrl | (addrh << 8);
}

// Relative (Assuming branch taken):
// - Read imm (offset), increment PC
// - Add offset to PC
// - If adding the offset overflowed the low byte of PC, add a cycle
static u16 addr_rel(nes_t* state) {
    s8 imm = (s8)addr_zp(state);
    u16 addr = state->cpu.pc + imm;
    tick(state);
    if ((addr & 0x100) != (state->cpu.pc & 0x100)) tick(state);
    return addr;
}

/* Instructions */

// Load / Store operations
static void instr_lda(nes_t* state, mode m) {
    u8 d = memory_read(state, m(state));
    update_z(state, d);
    update_n(state, d);
    state->cpu.a = d;
    tick(state);
}

static void instr_ldx(nes_t* state, mode m) {
    u8 d = memory_read(state, m(state));
    update_z(state, d);
    update_n(state, d);
    state->cpu.x = d;
    tick(state);
}

static void instr_ldy(nes_t* state, mode m) {
    u8 d = memory_read(state, m(state));
    update_z(state, d);
    update_n(state, d);
    state->cpu.y = d;
    tick(state);
}

static void instr_sta(nes_t* state, mode m) {
    memory_write(state, m(state), state->cpu.a);
    tick(state);
}

static void instr_stx(nes_t* state, mode m) {
    memory_write(state, m(state), state->cpu.x);
    tick(state);
}

static void instr_sty(nes_t* state, mode m) {
    memory_write(state, m(state), state->cpu.y);
    tick(state);
}

static void instr_txa(nes_t* state) {
    update_z(state, state->cpu.x);
    update_n(state, state->cpu.x);
    state->cpu.a = state->cpu.x;
    tick(state);
}

static void instr_txs(nes_t* state) {
    state->cpu.s = state->cpu.x;
    tick(state);
}

static void instr_tya(nes_t* state) {
    update_z(state, state->cpu.y);
    update_n(state, state->cpu.y);
    state->cpu.a = state->cpu.y;
    tick(state);
}

static void instr_tax(nes_t* state) {
    update_z(state, state->cpu.a);
    update_n(state, state->cpu.a);
    state->cpu.x = state->cpu.a;
    tick(state);
}

static void instr_tay(nes_t* state) {
    update_z(state, state->cpu.a);
    update_n(state, state->cpu.a);
    state->cpu.y = state->cpu.a;
    tick(state);
}

static void instr_tsx(nes_t* state) {
    update_z(state, state->cpu.s);
    update_n(state, state->cpu.s);
    state->cpu.x = state->cpu.s;
    tick(state);
}

// Stack operations
static void instr_php(nes_t* state) {
    // Throw away next byte
    tick(state);
    push(state, state->cpu.p | (1 << STATUS_BREAK) | (1 << STATUS_UNUSED));
    tick(state);
}

static void instr_plp(nes_t* state) {
    // Throw away next byte
    tick(state);
    // S increment
    tick(state);
    state->cpu.p = (pull(state) & ~(1 << STATUS_BREAK)) | (1 << STATUS_UNUSED);
    tick(state);
}

static void instr_pha(nes_t* state) {
    // Throw away next byte
    tick(state);
    push(state, state->cpu.a);
    tick(state);
}

static void instr_pla(nes_t* state) {
    // Throw away next byte
    tick(state);
    // S increment
    tick(state);
    state->cpu.a = pull(state);
    update_z(state, state->cpu.a);
    update_n(state, state->cpu.a);
    tick(state);
}

// Arithmetic / Logical operations
static void instr_adc(nes_t* state, mode m) {
    u8 d = memory_read(state, m(state));
    u16 s = state->cpu.a + d + NTH_BIT(state->cpu.p, STATUS_CARRY);
    update_c(state, s);
    update_z(state, (u8)s);
    update_v(state, state->cpu.a, d, s);
    update_n(state, (u8)s);
    state->cpu.a = (u8)s;
    tick(state);
}

static void instr_sbc(nes_t* state, mode m) {
    u8 d = memory_read(state, m(state));
    u16 s = state->cpu.a + (d ^ 0xFF) + NTH_BIT(state->cpu.p, STATUS_CARRY);
    update_c(state, s);
    update_z(state, (u8)s);
    update_v(state, state->cpu.a, (d ^ 0xFF), s);
    update_n(state, (u8)s);
    state->cpu.a = (u8)s;
    tick(state);
}

static void instr_and(nes_t* state, mode m) {
    u8 d = memory_read(state, m(state));
    state->cpu.a &= d;
    update_z(state, state->cpu.a);
    update_n(state, state->cpu.a);
    tick(state);
}

static void instr_eor(nes_t* state, mode m) {
    u8 d = memory_read(state, m(state));
    state->cpu.a ^= d;
    update_z(state, state->cpu.a);
    update_n(state, state->cpu.a);
    tick(state);
}

static void instr_ora(nes_t* state, mode m) {
    u8 d = memory_read(state, m(state));
    state->cpu.a |= d;
    update_z(state, state->cpu.a);
    update_n(state, state->cpu.a);
    tick(state);
}

static void instr_bit(nes_t* state, mode m) {
    u8 d = memory_read(state, m(state));
    update_z(state, state->cpu.a & d);
    ASSIGN_NTH_BIT(state->cpu.p, STATUS_NEGATIVE, NTH_BIT(d, 7));
    ASSIGN_NTH_BIT(state->cpu.p, STATUS_OVERFLOW, NTH_BIT(d, 6));
    tick(state);
}

// Compares
static void instr_cmp(nes_t* state, mode m) {
    u8 d = memory_read(state, m(state));
    u16 s = state->cpu.a + (d ^ 0xFF) + 1;
    update_c(state, s);
    update_z(state, (u8)s);
    update_n(state, (u8)s);
    tick(state);
}

static void instr_cpx(nes_t* state, mode m) {
    u8 d = memory_read(state, m(state));
    u16 s = state->cpu.x + (d ^ 0xFF) + 1;
    update_c(state, s);
    update_z(state, (u8)s);
    update_n(state, (u8)s);
    tick(state);
}

static void instr_cpy(nes_t* state, mode m) {
    u8 d = memory_read(state, m(state));
    u16 s = state->cpu.y + (d ^ 0xFF) + 1;
    update_c(state, s);
    update_z(state, (u8)s);
    update_n(state, (u8)s);
    tick(state);
}

// Increments / Decrements
static void instr_inc(nes_t* state, mode m) {
    u16 addr = m(state);
    u8 d = memory_read(state, addr);
    tick(state);
    d++;
    update_z(state, d);
    update_n(state, d);
    tick(state);
    memory_write(state, addr, d);
    tick(state);
}

static void instr_inx(nes_t* state) {
    state->cpu.x++;
    update_z(state, state->cpu.x);
    update_n(state, state->cpu.x);
    tick(state);
}

static void instr_iny(nes_t* state) {
    state->cpu.y++;
    update_z(state, state->cpu.y);
    update_n(state, state->cpu.y);
    tick(state);
}

static void instr_dec(nes_t* state, mode m) {
    u16 addr = m(state);
    u8 d = memory_read(state, addr);
    tick(state);
    d--;
    update_z(state, d);
    update_n(state, d);
    tick(state);
    memory_write(state, addr, d);
    tick(state);
}

static void instr_dex(nes_t* state) {
    state->cpu.x--;
    update_z(state, state->cpu.x);
    update_n(state, state->cpu.x);
    tick(state);
}

static void instr_dey(nes_t* state) {
    state->cpu.y--;
    update_z(state, state->cpu.y);
    update_n(state, state->cpu.y);
    tick(state);
}

// Shifts
static void instr_asl(nes_t* state, mode m) {
    u16 addr = m(state);
    u8 d = memory_read(state, addr);
    tick(state);
    ASSIGN_NTH_BIT(state->cpu.p, STATUS_CARRY, NTH_BIT(d, 7));
    d <<= 1;
    update_z(state, d);
    update_n(state, d);
    tick(state);
    memory_write(state, addr, d);
    tick(state);
}

static void instr_asl_a(nes_t* state) {
    ASSIGN_NTH_BIT(state->cpu.p, STATUS_CARRY, NTH_BIT(state->cpu.a, 7));
    state->cpu.a <<= 1;
    update_z(state, state->cpu.a);
    update_n(state, state->cpu.a);
    tick(state);
}

static void instr_lsr(nes_t* state, mode m) {
    u16 addr = m(state);
    u8 d = memory_read(state, addr);
    tick(state);
    ASSIGN_NTH_BIT(state->cpu.p, STATUS_CARRY, NTH_BIT(d, 0));
    d >>= 1;
    update_z(state, d);
    update_n(state, d);
    tick(state);
    memory_write(state, addr, d);
    tick(state);
}

static void instr_lsr_a(nes_t* state) {
    ASSIGN_NTH_BIT(state->cpu.p, STATUS_CARRY, NTH_BIT(state->cpu.a, 0));
    state->cpu.a >>= 1;
    update_z(state, state->cpu.a);
    update_n(state, state->cpu.a);
    tick(state);
}

static void instr_rol(nes_t* state, mode m) {
    u16 addr = m(state);
    u8 d = memory_read(state, addr);
    tick(state);
    bool c = NTH_BIT(state->cpu.p, STATUS_CARRY);
    ASSIGN_NTH_BIT(state->cpu.p, STATUS_CARRY, NTH_BIT(d, 7));
    d = (d << 1) | c;
    update_z(state, d);
    update_n(state, d);
    tick(state);
    memory_write(state, addr, d);
    tick(state);
}

static void instr_rol_a(nes_t* state) {
    bool c = NTH_BIT(state->cpu.p, STATUS_CARRY);
    ASSIGN_NTH_BIT(state->cpu.p, STATUS_CARRY, NTH_BIT(state->cpu.a, 7));
    state->cpu.a = (state->cpu.a << 1) | c;
    update_z(state, state->cpu.a);
    update_n(state, state->cpu.a);
    tick(state);
}

static void instr_ror(nes_t* state, mode m) {
    u16 addr = m(state);
    u8 d = memory_read(state, addr);
    tick(state);
    bool c = NTH_BIT(state->cpu.p, STATUS_CARRY);
    ASSIGN_NTH_BIT(state->cpu.p, STATUS_CARRY, NTH_BIT(d, 0));
    d = (d >> 1) | (c << 7);
    update_z(state, d);
    update_n(state, d);
    tick(state);
    memory_write(state, addr, d);
    tick(state);
}

static void instr_ror_a(nes_t* state) {
    bool c = NTH_BIT(state->cpu.p, STATUS_CARRY);
    ASSIGN_NTH_BIT(state->cpu.p, STATUS_CARRY, NTH_BIT(state->cpu.a, 0));
    state->cpu.a = (state->cpu.a >> 1) | (c << 7);
    update_z(state, state->cpu.a);
    update_n(state, state->cpu.a);
    tick(state);
}

// Jumps / calls
static void instr_jmp(nes_t* state, mode m) {
    state->cpu.pc = m(state);
}

static void instr_jsr(nes_t* state) {
    u8 addrl = memory_read(state, state->cpu.pc);
    state->cpu.pc += 1;
    tick(state);
    tick(state);
    push(state, state->cpu.pc >> 8);
    tick(state);
    push(state, state->cpu.pc & 0xFF);
    tick(state);
    u8 addrh = memory_read(state, state->cpu.pc);
    state->cpu.pc = addrl | (addrh << 8);
    tick(state);
}

static void instr_rts(nes_t* state) {
    // Throw away next byte
    tick(state);
    // S increment
    tick(state);
    u8 addrl = pull(state);
    tick(state);
    u8 addrh = pull(state);
    state->cpu.pc = addrl | (addrh << 8);
    tick(state);
    state->cpu.pc += 1;
    tick(state);
}

static void instr_rti(nes_t* state) {
    // Throw away next byte
    tick(state);
    // S increment
    tick(state);
    state->cpu.p = (pull(state) & ~(1 << STATUS_BREAK)) | (1 << STATUS_UNUSED);
    tick(state);
    u8 addrl = pull(state);
    tick(state);
    u8 addrh = pull(state);
    state->cpu.pc = addrl | (addrh << 8);
    tick(state);
}

// Branches
static void instr_bpl(nes_t* state, mode m) {
    if (!NTH_BIT(state->cpu.p, STATUS_NEGATIVE)) {
        state->cpu.pc = m(state);
    } else {
        state->cpu.pc++;
        tick(state);
    }
}

static void instr_bmi(nes_t* state, mode m) {
    if (NTH_BIT(state->cpu.p, STATUS_NEGATIVE)) {
        state->cpu.pc = m(state);
    } else {
        state->cpu.pc++;
        tick(state);
    }
}

static void instr_bvc(nes_t* state, mode m) {
    if (!NTH_BIT(state->cpu.p, STATUS_OVERFLOW)) {
        state->cpu.pc = m(state);
    } else {
        state->cpu.pc++;
        tick(state);
    }
}

static void instr_bvs(nes_t* state, mode m) {
    if (NTH_BIT(state->cpu.p, STATUS_OVERFLOW)) {
        state->cpu.pc = m(state);
    } else {
        state->cpu.pc++;
        tick(state);
    }
}

static void instr_bcc(nes_t* state, mode m) {
    if (!NTH_BIT(state->cpu.p, STATUS_CARRY)) {
        state->cpu.pc = m(state);
    } else {
        state->cpu.pc++;
        tick(state);
    }
}

static void instr_bcs(nes_t* state, mode m) {
    if (NTH_BIT(state->cpu.p, STATUS_CARRY)) {
        state->cpu.pc = m(state);
    } else {
        state->cpu.pc++;
        tick(state);
    }
}

static void instr_bne(nes_t* state, mode m) {
    if (!NTH_BIT(state->cpu.p, STATUS_ZERO)) {
        state->cpu.pc = m(state);
    } else {
        state->cpu.pc++;
        tick(state);
    }
}

static void instr_beq(nes_t* state, mode m) {
    if (NTH_BIT(state->cpu.p, STATUS_ZERO)) {
        state->cpu.pc = m(state);
    } else {
        state->cpu.pc++;
        tick(state);
    }
}

// Status register operations
static void instr_clc(nes_t* state) {
    CLEAR_NTH_BIT(state->cpu.p, STATUS_CARRY);
    tick(state);
}

static void instr_cli(nes_t* state) {
    CLEAR_NTH_BIT(state->cpu.p, STATUS_INT_DISABLE);
    tick(state);
}

static void instr_clv(nes_t* state) {
    CLEAR_NTH_BIT(state->cpu.p, STATUS_OVERFLOW);
    tick(state);
}

static void instr_cld(nes_t* state) {
    CLEAR_NTH_BIT(state->cpu.p, STATUS_DECIMAL);
    tick(state);
}

static void instr_sec(nes_t* state) {
    SET_NTH_BIT(state->cpu.p, STATUS_CARRY);
    tick(state);
}

static void instr_sei(nes_t* state) {
    SET_NTH_BIT(state->cpu.p, STATUS_INT_DISABLE);
    tick(state);
}

static void instr_sed(nes_t* state) {
    SET_NTH_BIT(state->cpu.p, STATUS_DECIMAL);
    tick(state);
}

// System functions
static void instr_nop(nes_t* state) {
    tick(state);
}

// Illegal opcodes
static void instr_skb(nes_t* state, mode m) {
    m(state);
    tick(state);
}

static void instr_lax(nes_t* state, mode m) {
    u8 d = memory_read(state, m(state));
    update_z(state, d);
    update_n(state, d);
    state->cpu.a = d;
    state->cpu.x = d;
    tick(state);
}

static void instr_sax(nes_t* state, mode m) {
    u16 addr = m(state);
    memory_write(state, addr, state->cpu.a & state->cpu.x);
    tick(state);
}

static void instr_axs(nes_t* state, mode m) {
    u8 d = memory_read(state, m(state));
    u16 s = (state->cpu.a & state->cpu.x) + (d ^ 0xFF) + 1;
    update_c(state, s);
    update_z(state, (u8)s);
    update_n(state, (u8)s);
    state->cpu.x = (u8)s;
    tick(state);
}

static void instr_dcp(nes_t* state, mode m) {
    u16 addr = m(state);
    u8 d = memory_read(state, addr);
    tick(state);
    d--;
    u16 s = state->cpu.a + (d ^ 0xFF) + 1;
    update_c(state, s);
    update_z(state, (u8)s);
    update_n(state, (u8)s);
    tick(state);
    memory_write(state, addr, d);
    tick(state);
}

static void instr_isc(nes_t* state, mode m) {
    u16 addr = m(state);
    u8 d = memory_read(state, addr);
    tick(state);
    d++;
    u16 s = state->cpu.a + (d ^ 0xFF) + NTH_BIT(state->cpu.p, STATUS_CARRY);
    update_c(state, s);
    update_z(state, (u8)s);
    update_v(state, state->cpu.a, (d ^ 0xFF), s);
    update_n(state, (u8)s);
    state->cpu.a = (u8)s;
    tick(state);
    memory_write(state, addr, d);
    tick(state);
}

static void instr_slo(nes_t* state, mode m) {
    u16 addr = m(state);
    u8 d = memory_read(state, addr);
    tick(state);
    ASSIGN_NTH_BIT(state->cpu.p, STATUS_CARRY, NTH_BIT(d, 7));
    d <<= 1;
    state->cpu.a |= d;
    update_z(state, state->cpu.a);
    update_n(state, state->cpu.a);
    tick(state);
    memory_write(state, addr, d);
    tick(state);
}

static void instr_rla(nes_t* state, mode m) {
    u16 addr = m(state);
    u8 d = memory_read(state, addr);
    tick(state);
    bool c = NTH_BIT(state->cpu.p, STATUS_CARRY);
    ASSIGN_NTH_BIT(state->cpu.p, STATUS_CARRY, NTH_BIT(d, 7));
    d = (d << 1) | c;
    state->cpu.a &= d;
    update_z(state, state->cpu.a);
    update_n(state, state->cpu.a);
    tick(state);
    memory_write(state, addr, d);
    tick(state);
}

static void instr_sre(nes_t* state, mode m) {
    u16 addr = m(state);
    u8 d = memory_read(state, addr);
    tick(state);
    ASSIGN_NTH_BIT(state->cpu.p, STATUS_CARRY, NTH_BIT(d, 0));
    d >>= 1;
    state->cpu.a ^= d;
    update_z(state, state->cpu.a);
    update_n(state, state->cpu.a);
    tick(state);
    memory_write(state, addr, d);
    tick(state);
}

static void instr_rra(nes_t* state, mode m) {
    u16 addr = m(state);
    u8 d = memory_read(state, addr);
    tick(state);
    bool c = NTH_BIT(state->cpu.p, STATUS_CARRY);
    ASSIGN_NTH_BIT(state->cpu.p, STATUS_CARRY, NTH_BIT(d, 0));
    d = (d >> 1) | (c << 7);
    u16 s = state->cpu.a + d + NTH_BIT(state->cpu.p, STATUS_CARRY);
    update_c(state, s);
    update_z(state, (u8)s);
    update_v(state, state->cpu.a, d, s);
    update_n(state, (u8)s);
    state->cpu.a = (u8)s;
    tick(state);
    memory_write(state, addr, d);
    tick(state);
}

/* CPU Execution */

static void execute_instruction(nes_t* state) {
    // Fetch
    u8 op = memory_read(state, state->cpu.pc++);
    tick(state);

#define INSTRUCTION_CASE_DEFAULT(op, fn)                                                           \
    case op: {                                                                                     \
        fn(state);                                                                                 \
        break;                                                                                     \
    }

#define INSTRUCTION_CASE_VARIANT(op, fn, variant)                                                  \
    case op: {                                                                                     \
        fn(state, variant);                                                                        \
        break;                                                                                     \
    }

    // Decode/Execute
    switch (op) {
        INSTRUCTION_CASE_DEFAULT(0x00, BRK)
        INSTRUCTION_CASE_VARIANT(0x01, instr_ora, addr_xind)
        INSTRUCTION_CASE_VARIANT(0x05, instr_ora, addr_zp)
        INSTRUCTION_CASE_VARIANT(0x06, instr_asl, addr_zp)
        INSTRUCTION_CASE_DEFAULT(0x08, instr_php)
        INSTRUCTION_CASE_VARIANT(0x09, instr_ora, addr_imm)
        INSTRUCTION_CASE_DEFAULT(0x0A, instr_asl_a)
        INSTRUCTION_CASE_VARIANT(0x0D, instr_ora, addr_absl)
        INSTRUCTION_CASE_VARIANT(0x0E, instr_asl, addr_absl)
        INSTRUCTION_CASE_VARIANT(0x10, instr_bpl, addr_rel)
        INSTRUCTION_CASE_VARIANT(0x11, instr_ora, addr_indy_rd)
        INSTRUCTION_CASE_VARIANT(0x15, instr_ora, addr_zpx)
        INSTRUCTION_CASE_VARIANT(0x16, instr_asl, addr_zpx)
        INSTRUCTION_CASE_DEFAULT(0x18, instr_clc)
        INSTRUCTION_CASE_VARIANT(0x19, instr_ora, addr_absy_rd)
        INSTRUCTION_CASE_VARIANT(0x1D, instr_ora, addr_absx_rd)
        INSTRUCTION_CASE_VARIANT(0x1E, instr_asl, addr_absx_wr)
        INSTRUCTION_CASE_DEFAULT(0x20, instr_jsr)
        INSTRUCTION_CASE_VARIANT(0x21, instr_and, addr_xind)
        INSTRUCTION_CASE_VARIANT(0x24, instr_bit, addr_zp)
        INSTRUCTION_CASE_VARIANT(0x25, instr_and, addr_zp)
        INSTRUCTION_CASE_VARIANT(0x26, instr_rol, addr_zp)
        INSTRUCTION_CASE_DEFAULT(0x28, instr_plp)
        INSTRUCTION_CASE_VARIANT(0x29, instr_and, addr_imm)
        INSTRUCTION_CASE_DEFAULT(0x2A, instr_rol_a)
        INSTRUCTION_CASE_VARIANT(0x2C, instr_bit, addr_absl)
        INSTRUCTION_CASE_VARIANT(0x2D, instr_and, addr_absl)
        INSTRUCTION_CASE_VARIANT(0x2E, instr_rol, addr_absl)
        INSTRUCTION_CASE_VARIANT(0x30, instr_bmi, addr_rel)
        INSTRUCTION_CASE_VARIANT(0x31, instr_and, addr_indy_rd)
        INSTRUCTION_CASE_VARIANT(0x35, instr_and, addr_zpx)
        INSTRUCTION_CASE_VARIANT(0x36, instr_rol, addr_zpx)
        INSTRUCTION_CASE_DEFAULT(0x38, instr_sec)
        INSTRUCTION_CASE_VARIANT(0x39, instr_and, addr_absy_rd)
        INSTRUCTION_CASE_VARIANT(0x3D, instr_and, addr_absx_rd)
        INSTRUCTION_CASE_VARIANT(0x3E, instr_rol, addr_absx_wr)
        INSTRUCTION_CASE_DEFAULT(0x40, instr_rti)
        INSTRUCTION_CASE_VARIANT(0x41, instr_eor, addr_xind)
        INSTRUCTION_CASE_VARIANT(0x45, instr_eor, addr_zp)
        INSTRUCTION_CASE_VARIANT(0x46, instr_lsr, addr_zp)
        INSTRUCTION_CASE_DEFAULT(0x48, instr_pha)
        INSTRUCTION_CASE_VARIANT(0x49, instr_eor, addr_imm)
        INSTRUCTION_CASE_DEFAULT(0x4A, instr_lsr_a)
        INSTRUCTION_CASE_VARIANT(0x4C, instr_jmp, addr_absl)
        INSTRUCTION_CASE_VARIANT(0x4D, instr_eor, addr_absl)
        INSTRUCTION_CASE_VARIANT(0x4E, instr_lsr, addr_absl)
        INSTRUCTION_CASE_VARIANT(0x50, instr_bvc, addr_rel)
        INSTRUCTION_CASE_VARIANT(0x51, instr_eor, addr_indy_rd)
        INSTRUCTION_CASE_VARIANT(0x55, instr_eor, addr_zpx)
        INSTRUCTION_CASE_VARIANT(0x56, instr_lsr, addr_zpx)
        INSTRUCTION_CASE_DEFAULT(0x58, instr_cli)
        INSTRUCTION_CASE_VARIANT(0x59, instr_eor, addr_absy_rd)
        INSTRUCTION_CASE_VARIANT(0x5D, instr_eor, addr_absx_rd)
        INSTRUCTION_CASE_VARIANT(0x5E, instr_lsr, addr_absx_wr)
        INSTRUCTION_CASE_DEFAULT(0x60, instr_rts)
        INSTRUCTION_CASE_VARIANT(0x61, instr_adc, addr_xind)
        INSTRUCTION_CASE_VARIANT(0x65, instr_adc, addr_zp)
        INSTRUCTION_CASE_VARIANT(0x66, instr_ror, addr_zp)
        INSTRUCTION_CASE_DEFAULT(0x68, instr_pla)
        INSTRUCTION_CASE_VARIANT(0x69, instr_adc, addr_imm)
        INSTRUCTION_CASE_DEFAULT(0x6A, instr_ror_a)
        INSTRUCTION_CASE_VARIANT(0x6C, instr_jmp, addr_ind)
        INSTRUCTION_CASE_VARIANT(0x6D, instr_adc, addr_absl)
        INSTRUCTION_CASE_VARIANT(0x6E, instr_ror, addr_absl)
        INSTRUCTION_CASE_VARIANT(0x70, instr_bvs, addr_rel)
        INSTRUCTION_CASE_VARIANT(0x71, instr_adc, addr_indy_rd)
        INSTRUCTION_CASE_VARIANT(0x75, instr_adc, addr_zpx)
        INSTRUCTION_CASE_VARIANT(0x76, instr_ror, addr_zpx)
        INSTRUCTION_CASE_DEFAULT(0x78, instr_sei)
        INSTRUCTION_CASE_VARIANT(0x79, instr_adc, addr_absy_rd)
        INSTRUCTION_CASE_VARIANT(0x7D, instr_adc, addr_absx_rd)
        INSTRUCTION_CASE_VARIANT(0x7E, instr_ror, addr_absx_wr)
        INSTRUCTION_CASE_VARIANT(0x81, instr_sta, addr_xind)
        INSTRUCTION_CASE_VARIANT(0x84, instr_sty, addr_zp)
        INSTRUCTION_CASE_VARIANT(0x85, instr_sta, addr_zp)
        INSTRUCTION_CASE_VARIANT(0x86, instr_stx, addr_zp)
        INSTRUCTION_CASE_DEFAULT(0x88, instr_dey)
        INSTRUCTION_CASE_DEFAULT(0x8A, instr_txa)
        INSTRUCTION_CASE_VARIANT(0x8C, instr_sty, addr_absl)
        INSTRUCTION_CASE_VARIANT(0x8D, instr_sta, addr_absl)
        INSTRUCTION_CASE_VARIANT(0x8E, instr_stx, addr_absl)
        INSTRUCTION_CASE_VARIANT(0x90, instr_bcc, addr_rel)
        INSTRUCTION_CASE_VARIANT(0x91, instr_sta, addr_indy_wr)
        INSTRUCTION_CASE_VARIANT(0x94, instr_sty, addr_zpx)
        INSTRUCTION_CASE_VARIANT(0x95, instr_sta, addr_zpx)
        INSTRUCTION_CASE_VARIANT(0x96, instr_stx, addr_zpy)
        INSTRUCTION_CASE_DEFAULT(0x98, instr_tya)
        INSTRUCTION_CASE_VARIANT(0x99, instr_sta, addr_absy_wr)
        INSTRUCTION_CASE_DEFAULT(0x9A, instr_txs)
        INSTRUCTION_CASE_VARIANT(0x9D, instr_sta, addr_absx_wr)
        INSTRUCTION_CASE_VARIANT(0xA0, instr_ldy, addr_imm)
        INSTRUCTION_CASE_VARIANT(0xA1, instr_lda, addr_xind)
        INSTRUCTION_CASE_VARIANT(0xA2, instr_ldx, addr_imm)
        INSTRUCTION_CASE_VARIANT(0xA4, instr_ldy, addr_zp)
        INSTRUCTION_CASE_VARIANT(0xA5, instr_lda, addr_zp)
        INSTRUCTION_CASE_VARIANT(0xA6, instr_ldx, addr_zp)
        INSTRUCTION_CASE_DEFAULT(0xA8, instr_tay)
        INSTRUCTION_CASE_VARIANT(0xA9, instr_lda, addr_imm)
        INSTRUCTION_CASE_DEFAULT(0xAA, instr_tax)
        INSTRUCTION_CASE_VARIANT(0xAC, instr_ldy, addr_absl)
        INSTRUCTION_CASE_VARIANT(0xAD, instr_lda, addr_absl)
        INSTRUCTION_CASE_VARIANT(0xAE, instr_ldx, addr_absl)
        INSTRUCTION_CASE_VARIANT(0xB0, instr_bcs, addr_rel)
        INSTRUCTION_CASE_VARIANT(0xB1, instr_lda, addr_indy_rd)
        INSTRUCTION_CASE_VARIANT(0xB4, instr_ldy, addr_zpx)
        INSTRUCTION_CASE_VARIANT(0xB5, instr_lda, addr_zpx)
        INSTRUCTION_CASE_VARIANT(0xB6, instr_ldx, addr_zpy)
        INSTRUCTION_CASE_DEFAULT(0xB8, instr_clv)
        INSTRUCTION_CASE_VARIANT(0xB9, instr_lda, addr_absy_rd)
        INSTRUCTION_CASE_DEFAULT(0xBA, instr_tsx)
        INSTRUCTION_CASE_VARIANT(0xBC, instr_ldy, addr_absx_rd)
        INSTRUCTION_CASE_VARIANT(0xBD, instr_lda, addr_absx_rd)
        INSTRUCTION_CASE_VARIANT(0xBE, instr_ldx, addr_absy_rd)
        INSTRUCTION_CASE_VARIANT(0xC0, instr_cpy, addr_imm)
        INSTRUCTION_CASE_VARIANT(0xC1, instr_cmp, addr_xind)
        INSTRUCTION_CASE_VARIANT(0xC4, instr_cpy, addr_zp)
        INSTRUCTION_CASE_VARIANT(0xC5, instr_cmp, addr_zp)
        INSTRUCTION_CASE_VARIANT(0xC6, instr_dec, addr_zp)
        INSTRUCTION_CASE_DEFAULT(0xC8, instr_iny)
        INSTRUCTION_CASE_VARIANT(0xC9, instr_cmp, addr_imm)
        INSTRUCTION_CASE_DEFAULT(0xCA, instr_dex)
        INSTRUCTION_CASE_VARIANT(0xCC, instr_cpy, addr_absl)
        INSTRUCTION_CASE_VARIANT(0xCD, instr_cmp, addr_absl)
        INSTRUCTION_CASE_VARIANT(0xCE, instr_dec, addr_absl)
        INSTRUCTION_CASE_VARIANT(0xD0, instr_bne, addr_rel)
        INSTRUCTION_CASE_VARIANT(0xD1, instr_cmp, addr_indy_rd)
        INSTRUCTION_CASE_VARIANT(0xD5, instr_cmp, addr_zpx)
        INSTRUCTION_CASE_VARIANT(0xD6, instr_dec, addr_zpx)
        INSTRUCTION_CASE_DEFAULT(0xD8, instr_cld)
        INSTRUCTION_CASE_VARIANT(0xD9, instr_cmp, addr_absy_rd)
        INSTRUCTION_CASE_VARIANT(0xDD, instr_cmp, addr_absx_rd)
        INSTRUCTION_CASE_VARIANT(0xDE, instr_dec, addr_absx_wr)
        INSTRUCTION_CASE_VARIANT(0xE0, instr_cpx, addr_imm)
        INSTRUCTION_CASE_VARIANT(0xE1, instr_sbc, addr_xind)
        INSTRUCTION_CASE_VARIANT(0xE4, instr_cpx, addr_zp)
        INSTRUCTION_CASE_VARIANT(0xE5, instr_sbc, addr_zp)
        INSTRUCTION_CASE_VARIANT(0xE6, instr_inc, addr_zp)
        INSTRUCTION_CASE_DEFAULT(0xE8, instr_inx)
        INSTRUCTION_CASE_VARIANT(0xE9, instr_sbc, addr_imm)
        INSTRUCTION_CASE_DEFAULT(0xEA, instr_nop)
        INSTRUCTION_CASE_VARIANT(0xEC, instr_cpx, addr_absl)
        INSTRUCTION_CASE_VARIANT(0xED, instr_sbc, addr_absl)
        INSTRUCTION_CASE_VARIANT(0xEE, instr_inc, addr_absl)
        INSTRUCTION_CASE_VARIANT(0xF0, instr_beq, addr_rel)
        INSTRUCTION_CASE_VARIANT(0xF1, instr_sbc, addr_indy_rd)
        INSTRUCTION_CASE_VARIANT(0xF5, instr_sbc, addr_zpx)
        INSTRUCTION_CASE_VARIANT(0xF6, instr_inc, addr_zpx)
        INSTRUCTION_CASE_DEFAULT(0xF8, instr_sed)
        INSTRUCTION_CASE_VARIANT(0xF9, instr_sbc, addr_absy_rd)
        INSTRUCTION_CASE_VARIANT(0xFD, instr_sbc, addr_absx_rd)
        INSTRUCTION_CASE_VARIANT(0xFE, instr_inc, addr_absx_wr)
        // Illegal opcodes
        INSTRUCTION_CASE_VARIANT(0x03, instr_slo, addr_xind)
        INSTRUCTION_CASE_VARIANT(0x07, instr_slo, addr_zp)
        INSTRUCTION_CASE_VARIANT(0x0F, instr_slo, addr_absl)
        INSTRUCTION_CASE_VARIANT(0x13, instr_slo, addr_indy_rd)
        INSTRUCTION_CASE_VARIANT(0x17, instr_slo, addr_zpx)
        INSTRUCTION_CASE_VARIANT(0x1B, instr_slo, addr_absy_rd)
        INSTRUCTION_CASE_VARIANT(0x1F, instr_slo, addr_absx_rd)
        INSTRUCTION_CASE_VARIANT(0x23, instr_rla, addr_xind)
        INSTRUCTION_CASE_VARIANT(0x27, instr_rla, addr_zp)
        INSTRUCTION_CASE_VARIANT(0x2F, instr_rla, addr_absl)
        INSTRUCTION_CASE_VARIANT(0x33, instr_rla, addr_indy_rd)
        INSTRUCTION_CASE_VARIANT(0x37, instr_rla, addr_zpx)
        INSTRUCTION_CASE_VARIANT(0x3B, instr_rla, addr_absy_rd)
        INSTRUCTION_CASE_VARIANT(0x3F, instr_rla, addr_absx_rd)
        INSTRUCTION_CASE_VARIANT(0x43, instr_sre, addr_xind)
        INSTRUCTION_CASE_VARIANT(0x47, instr_sre, addr_zp)
        INSTRUCTION_CASE_VARIANT(0x4F, instr_sre, addr_absl)
        INSTRUCTION_CASE_VARIANT(0x53, instr_sre, addr_indy_rd)
        INSTRUCTION_CASE_VARIANT(0x57, instr_sre, addr_zpx)
        INSTRUCTION_CASE_VARIANT(0x5B, instr_sre, addr_absy_rd)
        INSTRUCTION_CASE_VARIANT(0x5F, instr_sre, addr_absx_rd)
        INSTRUCTION_CASE_VARIANT(0x63, instr_rra, addr_xind)
        INSTRUCTION_CASE_VARIANT(0x67, instr_rra, addr_zp)
        INSTRUCTION_CASE_VARIANT(0x6F, instr_rra, addr_absl)
        INSTRUCTION_CASE_VARIANT(0x73, instr_rra, addr_indy_rd)
        INSTRUCTION_CASE_VARIANT(0x77, instr_rra, addr_zpx)
        INSTRUCTION_CASE_VARIANT(0x7B, instr_rra, addr_absy_rd)
        INSTRUCTION_CASE_VARIANT(0x7F, instr_rra, addr_absx_rd)
        INSTRUCTION_CASE_VARIANT(0x83, instr_sax, addr_xind)
        INSTRUCTION_CASE_VARIANT(0x87, instr_sax, addr_zp)
        INSTRUCTION_CASE_VARIANT(0x8F, instr_sax, addr_absl)
        INSTRUCTION_CASE_VARIANT(0x97, instr_sax, addr_zpy)
        INSTRUCTION_CASE_VARIANT(0xA3, instr_lax, addr_xind)
        INSTRUCTION_CASE_VARIANT(0xA7, instr_lax, addr_zp)
        INSTRUCTION_CASE_VARIANT(0xAB, instr_lax, addr_imm)
        INSTRUCTION_CASE_VARIANT(0xAF, instr_lax, addr_absl)
        INSTRUCTION_CASE_VARIANT(0xB3, instr_lax, addr_indy_rd)
        INSTRUCTION_CASE_VARIANT(0xB7, instr_lax, addr_zpy)
        INSTRUCTION_CASE_VARIANT(0xBF, instr_lax, addr_absy_rd)
        INSTRUCTION_CASE_VARIANT(0xC3, instr_dcp, addr_xind)
        INSTRUCTION_CASE_VARIANT(0xC7, instr_dcp, addr_zp)
        INSTRUCTION_CASE_VARIANT(0xCB, instr_axs, addr_imm)
        INSTRUCTION_CASE_VARIANT(0xCF, instr_dcp, addr_absl)
        INSTRUCTION_CASE_VARIANT(0xD3, instr_dcp, addr_indy_rd)
        INSTRUCTION_CASE_VARIANT(0xD7, instr_dcp, addr_zpx)
        INSTRUCTION_CASE_VARIANT(0xDB, instr_dcp, addr_absy_rd)
        INSTRUCTION_CASE_VARIANT(0xDF, instr_dcp, addr_absx_rd)
        INSTRUCTION_CASE_VARIANT(0xE3, instr_isc, addr_xind)
        INSTRUCTION_CASE_VARIANT(0xE7, instr_isc, addr_zp)
        INSTRUCTION_CASE_VARIANT(0xEB, instr_sbc, addr_imm)
        INSTRUCTION_CASE_VARIANT(0xEF, instr_isc, addr_absl)
        INSTRUCTION_CASE_VARIANT(0xF3, instr_isc, addr_indy_rd)
        INSTRUCTION_CASE_VARIANT(0xF7, instr_isc, addr_zpx)
        INSTRUCTION_CASE_VARIANT(0xFB, instr_isc, addr_absy_rd)
        INSTRUCTION_CASE_VARIANT(0xFF, instr_isc, addr_absx_rd)
        INSTRUCTION_CASE_DEFAULT(0x1A, instr_nop)
        INSTRUCTION_CASE_DEFAULT(0x3A, instr_nop)
        INSTRUCTION_CASE_DEFAULT(0x5A, instr_nop)
        INSTRUCTION_CASE_DEFAULT(0x7A, instr_nop)
        INSTRUCTION_CASE_DEFAULT(0xDA, instr_nop)
        INSTRUCTION_CASE_DEFAULT(0xFA, instr_nop)
        INSTRUCTION_CASE_VARIANT(0x04, instr_skb, addr_zp)
        INSTRUCTION_CASE_VARIANT(0x44, instr_skb, addr_zp)
        INSTRUCTION_CASE_VARIANT(0x64, instr_skb, addr_zp)
        INSTRUCTION_CASE_VARIANT(0x14, instr_skb, addr_zpx)
        INSTRUCTION_CASE_VARIANT(0x34, instr_skb, addr_zpx)
        INSTRUCTION_CASE_VARIANT(0x54, instr_skb, addr_zpx)
        INSTRUCTION_CASE_VARIANT(0x74, instr_skb, addr_zpx)
        INSTRUCTION_CASE_VARIANT(0xD4, instr_skb, addr_zpx)
        INSTRUCTION_CASE_VARIANT(0xF4, instr_skb, addr_zpx)
        INSTRUCTION_CASE_VARIANT(0x80, instr_skb, addr_imm)
        INSTRUCTION_CASE_VARIANT(0x82, instr_skb, addr_imm)
        INSTRUCTION_CASE_VARIANT(0x89, instr_skb, addr_imm)
        INSTRUCTION_CASE_VARIANT(0xC2, instr_skb, addr_imm)
        INSTRUCTION_CASE_VARIANT(0xE2, instr_skb, addr_imm)
        INSTRUCTION_CASE_VARIANT(0x0C, instr_skb, addr_absl)
        INSTRUCTION_CASE_VARIANT(0x1C, instr_skb, addr_absx_rd)
        INSTRUCTION_CASE_VARIANT(0x3C, instr_skb, addr_absx_rd)
        INSTRUCTION_CASE_VARIANT(0x5C, instr_skb, addr_absx_rd)
        INSTRUCTION_CASE_VARIANT(0x7C, instr_skb, addr_absx_rd)
        INSTRUCTION_CASE_VARIANT(0xDC, instr_skb, addr_absx_rd)
        INSTRUCTION_CASE_VARIANT(0xFC, instr_skb, addr_absx_rd)
        default:
            LOG("Unsupported instruction: 0x%02X\n", op);
            instr_nop(state);
            break;
    }

#undef INSTRUCTION_CASE_DEFAULT
#undef INSTRUCTION_CASE_VARIANT
}

void cpu_init(nes_t* state) {
    state->cpu.a = 0x00;
    state->cpu.x = 0x00;
    state->cpu.y = 0x00;
    state->cpu.s = 0x00;
    state->cpu.p = 0x00 | (1 << STATUS_INT_DISABLE) | (1 << STATUS_UNUSED);
    state->cpu.nmi = 0;
    state->cpu.irq = 0;
    state->cpu.cycle = 0;
    interrupt_reset(state);
}

void cpu_step(nes_t* state) {
    if (state->cpu.nmi) {
        interrupt_nmi(state);
    } else if (state->cpu.irq && !NTH_BIT(state->cpu.p, STATUS_INT_DISABLE)) {
        interrupt_irq(state);
    }
    execute_instruction(state);
}

void cpu_set_nmi(nes_t* state, bool enable) {
    state->cpu.nmi = enable;
}

void cpu_set_irq(nes_t* state, bool enable) {
    state->cpu.irq = enable;
}
