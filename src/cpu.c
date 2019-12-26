#include "cpu.h"

#include "bitmask.h"
#include "cartridge.h"
#include "nes.h"
#include "ppu.h"

#include <stdio.h>

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

// Registers, interrupt flags, cycle tracker
static cpu_state_t cpu;
// Internal CPU memory
static u8 ram[RAM_SIZE];

static void tick(void) {
    ppu_tick();
    ppu_tick();
    ppu_tick();
    cpu.cycle++;
}

/* Read/Write operations */

static u8 rd(u16 addr) {
    switch (addr) {
        case 0x0000 ... 0x1FFF:
            return ram[addr % RAM_SIZE];
        case 0x2000 ... 0x3FFF:
            return ppu_rd(addr);
        case 0x4000 ... 0x4013:
            // APU
            return 0;
        case 0x4014:
            return ppu_rd(addr);
        case 0x4015:
            // APU
            return 0;
        case 0x4016:
            // return controller_rd(0);
            return 0;
        case 0x4017:
            // return controller_rd(1);
            return 0;
        case 0x4018 ... 0xFFFF:
            return cartridge_prg_rd(addr);
    }
}

static void wr(u16 addr, u8 data) {
    switch (addr) {
        case 0x0000 ... 0x1FFF:
            ram[addr % RAM_SIZE] = data;
            return;
        case 0x2000 ... 0x3FFF:
            ppu_wr(0x2000 + addr % PPU_REGISTER_FILE_SIZE, data);
            return;
        case 0x4000 ... 0x4013:
            // APU
            return;
        case OAM_DMA_OFFSET:
            for (int i = 0; i < 0x100; i++)
                ppu_wr(OAM_DATA_OFFSET, rd((data << 2) | i));
            return;
        case 0x4015:
            // APU
            return;
        case 0x4016:
            // controller_wr(data);
            return;
        case 0x4017:
            return;
        case 0x4018 ... 0xFFFF:
            cartridge_prg_wr(addr, data);
            return;
    }
}

/* Stack operations */

static void push(u8 data) {
    wr(STACK_OFFSET | cpu.registers.s--, data);
}

static u8 pull(void) {
    return rd(STACK_OFFSET | ++cpu.registers.s);
}

/* Flag adjustment */

static inline void updateC(u16 r) {
    ASSIGN_NTH_BIT(cpu.registers.p, STATUS_CARRY, r > 0xFF);
}

static inline void updateZ(u8 d) {
    ASSIGN_NTH_BIT(cpu.registers.p, STATUS_ZERO, d == 0);
}

static inline void updateV(u8 d1, u8 d2, u16 r) {
    ASSIGN_NTH_BIT(
      cpu.registers.p,
      STATUS_OVERFLOW,
      NTH_BIT((0xFF ^ d1 ^ d2) & (d1 ^ r), 7));
}

static inline void updateN(u8 d) {
    ASSIGN_NTH_BIT(cpu.registers.p, STATUS_NEGATIVE, NTH_BIT(d, 7));
}

/* Interrupts */

static void INT_NMI(void) {
    // Throw away fetched instruction
    tick();
    // Suppress PC increment
    tick();
    push(cpu.registers.pc >> 8);
    tick();
    push(cpu.registers.pc & 0xFF);
    tick();
    push(cpu.registers.p | (1 << STATUS_UNUSED));
    tick();
    SET_NTH_BIT(cpu.registers.p, STATUS_INT_DISABLE);
    u8 addrl = rd(NMI_HANDLE_OFFSET);
    tick();
    u8 addrh = rd(NMI_HANDLE_OFFSET + 1);
    cpu.registers.pc = addrl | (addrh << 8);
    // CPU clears NMI after handling
    cpu_set_NMI(0);
    tick();
}

static void INT_RESET(void) {
    // Throw away fetched instruction
    tick();
    // Suppress PC increment
    tick();
    // Suppress the 3 writes to the stack
    cpu.registers.s -= 3;
    tick();
    tick();
    tick();
    SET_NTH_BIT(cpu.registers.p, STATUS_INT_DISABLE);
    u8 addrl = rd(RESET_HANDLE_OFFSET);
    tick();
    u8 addrh = rd(RESET_HANDLE_OFFSET + 1);
    cpu.registers.pc = addrl | (addrh << 8);
    tick();
}

static void INT_IRQ(void) {
    // Throw away fetched instruction
    tick();
    // Suppress PC increment
    tick();
    push(cpu.registers.pc >> 8);
    tick();
    push(cpu.registers.pc & 0xFF);
    tick();
    push(cpu.registers.p | (1 << STATUS_UNUSED));
    tick();
    SET_NTH_BIT(cpu.registers.p, STATUS_INT_DISABLE);
    u8 addrl = rd(IRQ_BRK_HANDLE_OFFSET);
    tick();
    u8 addrh = rd(IRQ_BRK_HANDLE_OFFSET + 1);
    cpu.registers.pc = addrl | (addrh << 8);
    tick();
}

static void BRK(void) {
    cpu.registers.pc++;
    tick();
    push(cpu.registers.pc >> 8);
    tick();
    push(cpu.registers.pc & 0xFF);
    push(cpu.registers.p | (1 << STATUS_BREAK) | (1 << STATUS_UNUSED));
    tick();
    SET_NTH_BIT(cpu.registers.p, STATUS_INT_DISABLE);
    u8 addrl = rd(IRQ_BRK_HANDLE_OFFSET);
    tick();
    u8 addrh = rd(IRQ_BRK_HANDLE_OFFSET + 1);
    cpu.registers.pc = addrl | (addrh << 8);
    tick();
}

/* Addressing modes */

// Immediate:
// - Return current PC and increment PC (immediate stored here)
static u16 imm(void) {
    return cpu.registers.pc++;
}
// ZP:
// - Read the immediate, increment PC
// - Return the immediate
static u16 zp(void) {
    u16 addr = rd(imm());
    tick();
    return addr;
}
// ZP, X:
// - Read the immediate, increment PC
// - Calculate imm + X, include wraparound
// - Return the new address
static u16 zpx(void) {
    u16 addr = (zp() + cpu.registers.x) % 0x100;
    tick();
    return addr;
}
// ZP, Y:
// - Read the immediate, increment PC
// - Calculate imm + Y, include wraparound
// - Return the new address
static u16 zpy(void) {
    u16 addr = (zp() + cpu.registers.y) % 0x100;
    tick();
    return addr;
}
// Absolute:
// - Read the immediate, increment PC
// - Merge new immediate with old immediate, increment PC
// - Return the merged address
static u16 absl(void) {
    u8 addrl = (u8)zp();
    u8 addrh = (u8)zp();
    return addrl | (addrh << 8);
}
// Absolute, X:
// - Read the immediate, increment PC
// - Read the new immediate, add the old immediate with X, increment PC
// - If the sum of old imm and X overflows, reread the address next tick
// - Merge old imm + X with new imm, return the merged address
static u16 absx_rd(void) {
    u16 addrl = zp();
    u8 addrh = rd(imm());
    addrl += cpu.registers.x;
    tick();
    if ((addrl & 0xFF00) != 0) {
        addrl %= 0x100;
        addrh++;
        tick();
    }
    return addrl | (addrh << 8);
}
// Must incur a tick regardless of page boundary cross
static u16 absx_wr(void) {
    u16 addrl = zp();
    u8 addrh = rd(imm());
    addrl += cpu.registers.x;
    tick();
    if ((addrl & 0xFF00) != 0) {
        addrl %= 0x100;
        addrh++;
    }
    tick();
    return addrl | (addrh << 8);
}
// Absolute, Y:
// - Read the immediate, increment PC
// - Read the new immediate, add the old immediate with Y, increment PC
// - If the sum of old imm and Y overflows, reread the address next tick
// - Merge old imm + Y with new imm, return the merged address
static u16 absy_rd(void) {
    u16 addrl = zp();
    u8 addrh = rd(imm());
    addrl += cpu.registers.y;
    tick();
    if ((addrl & 0xFF00) != 0) {
        addrl %= 0x100;
        addrh++;
        tick();
    }
    return addrl | (addrh << 8);
}
// Must incur a tick regardless of page boundary cross
static u16 absy_wr(void) {
    u16 addrl = zp();
    u8 addrh = rd(imm());
    addrl += cpu.registers.y;
    tick();
    if ((addrl & 0xFF00) != 0) {
        addrl %= 0x100;
        addrh++;
    }
    tick();
    return addrl | (addrh << 8);
}
// Absolute Indirect (JMP only):
// - Read imm (pointer low), increment PC
// - Read imm (pointer high), increment PC
// - Read low byte from pointer
// - Read high byte from pointer (wrap around) and return the merged address
static u16 ind(void) {
    u8 ptrl = (u8)zp();
    u8 ptrh = (u8)zp();
    u16 ptr = ptrl | (ptrh << 8);
    u8 addrl = rd(ptr);
    tick();
    u8 addrh = rd((ptr & 0xFF00) | ((ptr + 1) % 0x100));
    tick();
    return addrl | (addrh << 8);
}
// X, Indirect (Indexed Indirect):
// - Read imm (pointer), increment PC
// - Read address at imm + X on zero page
// - Read low byte from pointer
// - Read high byte from pointer and return the merged address
static u16 xind(void) {
    u8 ptr = (u8)zpx();
    u8 addrl = rd(ptr);
    tick();
    u8 addrh = rd((ptr + 1) % 0x100);
    tick();
    return addrl | (addrh << 8);
}
// Indirect, Y (Indirect Indexed):
// - Read imm (pointer), increment PC
// - Read low byte from pointer on zero page
// - Read high byte from pointer on zero page, add Y to low byte
// - If the sum of low byte and X overflows, reread the address next tick
// - Return the merged address
static u16 indy_rd(void) {
    u8 ptr = (u8)zp();
    u16 addrl = rd(ptr);
    tick();
    u8 addrh = rd((ptr + 1) % 0x100);
    addrl += cpu.registers.y;
    tick();
    if ((addrl & 0xFF00) != 0) {
        addrl %= 0x100;
        addrh = (addrh + 1);
        tick();
    }
    return addrl | (addrh << 8);
}
// Must incur a tick regardless of page boundary cross
static u16 indy_wr(void) {
    u8 ptr = (u8)zp();
    u16 addrl = rd(ptr);
    tick();
    u8 addrh = rd((ptr + 1) % 0x100);
    addrl += cpu.registers.y;
    tick();
    if ((addrl & 0xFF00) != 0) {
        addrl %= 0x100;
        addrh = (addrh + 1);
    }
    tick();
    return addrl | (addrh << 8);
}
// Relative(Assuming branch taken:
// - Read imm (offset), increment PC
// - Add offset to PC
// - If adding the offset overflowed the low byte of PC, add a cycle
static u16 rel(void) {
    s8 imm = (s8)zp();
    u16 addr = cpu.registers.pc + imm;
    tick();
    if ((addr & 0x100) != (cpu.registers.pc & 0x100)) tick();
    return addr;
}

/* Instructions */

// Load / Store operations
static void LDA(mode m) {
    u8 d = rd(m());
    updateZ(d);
    updateN(d);
    cpu.registers.a = d;
    tick();
}

static void LDX(mode m) {
    u8 d = rd(m());
    updateZ(d);
    updateN(d);
    cpu.registers.x = d;
    tick();
}

static void LDY(mode m) {
    u8 d = rd(m());
    updateZ(d);
    updateN(d);
    cpu.registers.y = d;
    tick();
}

static void STA(mode m) {
    wr(m(), cpu.registers.a);
    tick();
}

static void STX(mode m) {
    wr(m(), cpu.registers.x);
    tick();
}

static void STY(mode m) {
    wr(m(), cpu.registers.y);
    tick();
}

static void TXA(void) {
    updateZ(cpu.registers.x);
    updateN(cpu.registers.x);
    cpu.registers.a = cpu.registers.x;
    tick();
}

static void TXS(void) {
    cpu.registers.s = cpu.registers.x;
    tick();
}

static void TYA(void) {
    updateZ(cpu.registers.y);
    updateN(cpu.registers.y);
    cpu.registers.a = cpu.registers.y;
    tick();
}

static void TAX(void) {
    updateZ(cpu.registers.a);
    updateN(cpu.registers.a);
    cpu.registers.x = cpu.registers.a;
    tick();
}

static void TAY(void) {
    updateZ(cpu.registers.a);
    updateN(cpu.registers.a);
    cpu.registers.y = cpu.registers.a;
    tick();
}

static void TSX(void) {
    updateZ(cpu.registers.s);
    updateN(cpu.registers.s);
    cpu.registers.x = cpu.registers.s;
    tick();
}

// Stack operations
static void PHP(void) {
    // Throw away next byte
    tick();
    push(cpu.registers.p | (1 << STATUS_BREAK) | (1 << STATUS_UNUSED));
    tick();
}

static void PLP(void) {
    // Throw away next byte
    tick();
    // S increment
    tick();
    cpu.registers.p = (pull() & ~(1 << STATUS_BREAK)) | (1 << STATUS_UNUSED);
    tick();
}

static void PHA(void) {
    // Throw away next byte
    tick();
    push(cpu.registers.a);
    tick();
}

static void PLA(void) {
    // Throw away next byte
    tick();
    // S increment
    tick();
    cpu.registers.a = pull();
    updateZ(cpu.registers.a);
    updateN(cpu.registers.a);
    tick();
}

// Arithmetic / Logical operations
static void _ADC(mode m) {
    u8 d = rd(m());
    u16 s = cpu.registers.a + d + NTH_BIT(cpu.registers.p, STATUS_CARRY);
    updateC(s);
    updateZ((u8)s);
    updateV(cpu.registers.a, d, s);
    updateN((u8)s);
    cpu.registers.a = (u8)s;
    tick();
}

static void SBC(mode m) {
    u8 d = rd(m());
    u16 s =
      cpu.registers.a + (d ^ 0xFF) + NTH_BIT(cpu.registers.p, STATUS_CARRY);
    updateC(s);
    updateZ((u8)s);
    updateV(cpu.registers.a, (d ^ 0xFF), s);
    updateN((u8)s);
    cpu.registers.a = (u8)s;
    tick();
}

static void AND(mode m) {
    u8 d = rd(m());
    cpu.registers.a &= d;
    updateZ(cpu.registers.a);
    updateN(cpu.registers.a);
    tick();
}

static void EOR(mode m) {
    u8 d = rd(m());
    cpu.registers.a ^= d;
    updateZ(cpu.registers.a);
    updateN(cpu.registers.a);
    tick();
}

static void ORA(mode m) {
    u8 d = rd(m());
    cpu.registers.a |= d;
    updateZ(cpu.registers.a);
    updateN(cpu.registers.a);
    tick();
}

static void BIT(mode m) {
    u8 d = rd(m());
    updateZ(cpu.registers.a & d);
    ASSIGN_NTH_BIT(cpu.registers.p, STATUS_NEGATIVE, NTH_BIT(d, 7));
    ASSIGN_NTH_BIT(cpu.registers.p, STATUS_OVERFLOW, NTH_BIT(d, 6));
    tick();
}

// Compares
static void CMP(mode m) {
    u8 d = rd(m());
    u16 s = cpu.registers.a + (d ^ 0xFF) + 1;
    updateC(s);
    updateZ((u8)s);
    updateN((u8)s);
    tick();
}

static void CPX(mode m) {
    u8 d = rd(m());
    u16 s = cpu.registers.x + (d ^ 0xFF) + 1;
    updateC(s);
    updateZ((u8)s);
    updateN((u8)s);
    tick();
}

static void CPY(mode m) {
    u8 d = rd(m());
    u16 s = cpu.registers.y + (d ^ 0xFF) + 1;
    updateC(s);
    updateZ((u8)s);
    updateN((u8)s);
    tick();
}

// Increments / Decrements
static void INC(mode m) {
    u16 addr = m();
    u8 d = rd(addr);
    tick();
    d++;
    updateZ(d);
    updateN(d);
    tick();
    wr(addr, d);
    tick();
}

static void INX(void) {
    cpu.registers.x++;
    updateZ(cpu.registers.x);
    updateN(cpu.registers.x);
    tick();
}

static void INY(void) {
    cpu.registers.y++;
    updateZ(cpu.registers.y);
    updateN(cpu.registers.y);
    tick();
}

static void DEC(mode m) {
    u16 addr = m();
    u8 d = rd(addr);
    tick();
    d--;
    updateZ(d);
    updateN(d);
    tick();
    wr(addr, d);
    tick();
}

static void DEX(void) {
    cpu.registers.x--;
    updateZ(cpu.registers.x);
    updateN(cpu.registers.x);
    tick();
}

static void DEY(void) {
    cpu.registers.y--;
    updateZ(cpu.registers.y);
    updateN(cpu.registers.y);
    tick();
}

// Shifts
static void ASL(mode m) {
    u16 addr = m();
    u8 d = rd(addr);
    tick();
    ASSIGN_NTH_BIT(cpu.registers.p, STATUS_CARRY, NTH_BIT(d, 7));
    d <<= 1;
    updateZ(d);
    updateN(d);
    tick();
    wr(addr, d);
    tick();
}

static void ASL_A(void) {
    ASSIGN_NTH_BIT(cpu.registers.p, STATUS_CARRY, NTH_BIT(cpu.registers.a, 7));
    cpu.registers.a <<= 1;
    updateZ(cpu.registers.a);
    updateN(cpu.registers.a);
    tick();
}

static void LSR(mode m) {
    u16 addr = m();
    u8 d = rd(addr);
    tick();
    ASSIGN_NTH_BIT(cpu.registers.p, STATUS_CARRY, NTH_BIT(d, 0));
    d >>= 1;
    updateZ(d);
    updateN(d);
    tick();
    wr(addr, d);
    tick();
}

static void LSR_A(void) {
    ASSIGN_NTH_BIT(cpu.registers.p, STATUS_CARRY, NTH_BIT(cpu.registers.a, 0));
    cpu.registers.a >>= 1;
    updateZ(cpu.registers.a);
    updateN(cpu.registers.a);
    tick();
}

static void ROL(mode m) {
    u16 addr = m();
    u8 d = rd(addr);
    tick();
    bool c = NTH_BIT(cpu.registers.p, STATUS_CARRY);
    ASSIGN_NTH_BIT(cpu.registers.p, STATUS_CARRY, NTH_BIT(d, 7));
    d = (d << 1) | c;
    updateZ(d);
    updateN(d);
    tick();
    wr(addr, d);
    tick();
}

static void ROL_A(void) {
    bool c = NTH_BIT(cpu.registers.p, STATUS_CARRY);
    ASSIGN_NTH_BIT(cpu.registers.p, STATUS_CARRY, NTH_BIT(cpu.registers.a, 7));
    cpu.registers.a = (cpu.registers.a << 1) | c;
    updateZ(cpu.registers.a);
    updateN(cpu.registers.a);
    tick();
}

static void ROR(mode m) {
    u16 addr = m();
    u8 d = rd(addr);
    tick();
    bool c = NTH_BIT(cpu.registers.p, STATUS_CARRY);
    ASSIGN_NTH_BIT(cpu.registers.p, STATUS_CARRY, NTH_BIT(d, 0));
    d = (d >> 1) | (c << 7);
    updateZ(d);
    updateN(d);
    tick();
    wr(addr, d);
    tick();
}

static void ROR_A(void) {
    bool c = NTH_BIT(cpu.registers.p, STATUS_CARRY);
    ASSIGN_NTH_BIT(cpu.registers.p, STATUS_CARRY, NTH_BIT(cpu.registers.a, 0));
    cpu.registers.a = (cpu.registers.a >> 1) | (c << 7);
    updateZ(cpu.registers.a);
    updateN(cpu.registers.a);
    tick();
}

// Jumps / calls
static void JMP(mode m) {
    cpu.registers.pc = m();
}

static void JSR(void) {
    u8 addrl = rd(cpu.registers.pc);
    cpu.registers.pc++;
    tick();
    tick();
    push(cpu.registers.pc >> 8);
    tick();
    push(cpu.registers.pc & 0xFF);
    tick();
    u8 addrh = rd(cpu.registers.pc);
    cpu.registers.pc = addrl | (addrh << 8);
    tick();
}

static void RTS(void) {
    // Throw away next byte
    tick();
    // S increment
    tick();
    u8 addrl = pull();
    tick();
    u8 addrh = pull();
    cpu.registers.pc = addrl | (addrh << 8);
    tick();
    cpu.registers.pc++;
    tick();
}

static void RTI(void) {
    // Throw away next byte
    tick();
    // S increment
    tick();
    cpu.registers.p = (pull() & ~(1 << STATUS_BREAK)) | (1 << STATUS_UNUSED);
    tick();
    u8 addrl = pull();
    tick();
    u8 addrh = pull();
    cpu.registers.pc = addrl | (addrh << 8);
    tick();
}

// Branches
static void BPL(mode m) {
    if (!NTH_BIT(cpu.registers.p, STATUS_NEGATIVE)) {
        cpu.registers.pc = m();
    } else {
        cpu.registers.pc++;
        tick();
    }
}

static void BMI(mode m) {
    if (NTH_BIT(cpu.registers.p, STATUS_NEGATIVE)) {
        cpu.registers.pc = m();
    } else {
        cpu.registers.pc++;
        tick();
    }
}

static void BVC(mode m) {
    if (!NTH_BIT(cpu.registers.p, STATUS_OVERFLOW)) {
        cpu.registers.pc = m();
    } else {
        cpu.registers.pc++;
        tick();
    }
}

static void BVS(mode m) {
    if (NTH_BIT(cpu.registers.p, STATUS_OVERFLOW)) {
        cpu.registers.pc = m();
    } else {
        cpu.registers.pc++;
        tick();
    }
}

static void BCC(mode m) {
    if (!NTH_BIT(cpu.registers.p, STATUS_CARRY)) {
        cpu.registers.pc = m();
    } else {
        cpu.registers.pc++;
        tick();
    }
}

static void BCS(mode m) {
    if (NTH_BIT(cpu.registers.p, STATUS_CARRY)) {
        cpu.registers.pc = m();
    } else {
        cpu.registers.pc++;
        tick();
    }
}

static void BNE(mode m) {
    if (!NTH_BIT(cpu.registers.p, STATUS_ZERO)) {
        cpu.registers.pc = m();
    } else {
        cpu.registers.pc++;
        tick();
    }
}

static void BEQ(mode m) {
    if (NTH_BIT(cpu.registers.p, STATUS_ZERO)) {
        cpu.registers.pc = m();
    } else {
        cpu.registers.pc++;
        tick();
    }
}

// Status register operations
static void CLC(void) {
    CLEAR_NTH_BIT(cpu.registers.p, STATUS_CARRY);
    tick();
}

static void CLI(void) {
    CLEAR_NTH_BIT(cpu.registers.p, STATUS_INT_DISABLE);
    tick();
}

static void CLV(void) {
    CLEAR_NTH_BIT(cpu.registers.p, STATUS_OVERFLOW);
    tick();
}

static void CLD(void) {
    CLEAR_NTH_BIT(cpu.registers.p, STATUS_DECIMAL);
    tick();
}

static void SEC(void) {
    SET_NTH_BIT(cpu.registers.p, STATUS_CARRY);
    tick();
}

static void SEI(void) {
    SET_NTH_BIT(cpu.registers.p, STATUS_INT_DISABLE);
    tick();
}

static void SED(void) {
    SET_NTH_BIT(cpu.registers.p, STATUS_DECIMAL);
    tick();
}

// System functions
static void NOP(void) {
    tick();
}

// Illegal opcodes
static void SKB(mode m) {
    m();
    tick();
}

static void LAX(mode m) {
    u8 d = rd(m());
    updateZ(d);
    updateN(d);
    cpu.registers.a = d;
    cpu.registers.x = d;
    tick();
}

static void SAX(mode m) {
    u16 addr = m();
    wr(addr, cpu.registers.a & cpu.registers.x);
    tick();
}

static void AXS(mode m) {
    u8 d = rd(m());
    u16 s = (cpu.registers.a & cpu.registers.x) + (d ^ 0xFF) + 1;
    updateC(s);
    updateZ((u8)s);
    updateN((u8)s);
    cpu.registers.x = (u8)s;
    tick();
}

static void DCP(mode m) {
    u16 addr = m();
    u8 d = rd(addr);
    tick();
    d--;
    u16 s = cpu.registers.a + (d ^ 0xFF) + 1;
    updateC(s);
    updateZ((u8)s);
    updateN((u8)s);
    tick();
    wr(addr, d);
    tick();
}

static void ISC(mode m) {
    u16 addr = m();
    u8 d = rd(addr);
    tick();
    d++;
    u16 s =
      cpu.registers.a + (d ^ 0xFF) + NTH_BIT(cpu.registers.p, STATUS_CARRY);
    updateC(s);
    updateZ((u8)s);
    updateV(cpu.registers.a, (d ^ 0xFF), s);
    updateN((u8)s);
    cpu.registers.a = (u8)s;
    tick();
    wr(addr, d);
    tick();
}

static void SLO(mode m) {
    u16 addr = m();
    u8 d = rd(addr);
    tick();
    ASSIGN_NTH_BIT(cpu.registers.p, STATUS_CARRY, NTH_BIT(d, 7));
    d <<= 1;
    cpu.registers.a |= d;
    updateZ(cpu.registers.a);
    updateN(cpu.registers.a);
    tick();
    wr(addr, d);
    tick();
}

static void RLA(mode m) {
    u16 addr = m();
    u8 d = rd(addr);
    tick();
    bool c = NTH_BIT(cpu.registers.p, STATUS_CARRY);
    ASSIGN_NTH_BIT(cpu.registers.p, STATUS_CARRY, NTH_BIT(d, 7));
    d = (d << 1) | c;
    cpu.registers.a &= d;
    updateZ(cpu.registers.a);
    updateN(cpu.registers.a);
    tick();
    wr(addr, d);
    tick();
}

static void SRE(mode m) {
    u16 addr = m();
    u8 d = rd(addr);
    tick();
    ASSIGN_NTH_BIT(cpu.registers.p, STATUS_CARRY, NTH_BIT(d, 0));
    d >>= 1;
    cpu.registers.a ^= d;
    updateZ(cpu.registers.a);
    updateN(cpu.registers.a);
    tick();
    wr(addr, d);
    tick();
}

static void RRA(mode m) {
    u16 addr = m();
    u8 d = rd(addr);
    tick();
    bool c = NTH_BIT(cpu.registers.p, STATUS_CARRY);
    ASSIGN_NTH_BIT(cpu.registers.p, STATUS_CARRY, NTH_BIT(d, 0));
    d = (d >> 1) | (c << 7);
    u16 s = cpu.registers.a + d + NTH_BIT(cpu.registers.p, STATUS_CARRY);
    updateC(s);
    updateZ((u8)s);
    updateV(cpu.registers.a, d, s);
    updateN((u8)s);
    cpu.registers.a = (u8)s;
    tick();
    wr(addr, d);
    tick();
}

/* CPU Execution */

static void exec_inst(void) {
    // Fetch
    u8 op = rd(cpu.registers.pc++);
    tick();
    // Decode/Execute
    switch (op) {
        case 0x00:
            return BRK();
        case 0x01:
            return ORA(xind);
        case 0x05:
            return ORA(zp);
        case 0x06:
            return ASL(zp);
        case 0x08:
            return PHP();
        case 0x09:
            return ORA(imm);
        case 0x0A:
            return ASL_A();
        case 0x0D:
            return ORA(absl);
        case 0x0E:
            return ASL(absl);
        case 0x10:
            return BPL(rel);
        case 0x11:
            return ORA(indy_rd);
        case 0x15:
            return ORA(zpx);
        case 0x16:
            return ASL(zpx);
        case 0x18:
            return CLC();
        case 0x19:
            return ORA(absy_rd);
        case 0x1D:
            return ORA(absx_rd);
        case 0x1E:
            return ASL(absx_wr);
        case 0x20:
            return JSR();
        case 0x21:
            return AND(xind);
        case 0x24:
            return BIT(zp);
        case 0x25:
            return AND(zp);
        case 0x26:
            return ROL(zp);
        case 0x28:
            return PLP();
        case 0x29:
            return AND(imm);
        case 0x2A:
            return ROL_A();
        case 0x2C:
            return BIT(absl);
        case 0x2D:
            return AND(absl);
        case 0x2E:
            return ROL(absl);
        case 0x30:
            return BMI(rel);
        case 0x31:
            return AND(indy_rd);
        case 0x35:
            return AND(zpx);
        case 0x36:
            return ROL(zpx);
        case 0x38:
            return SEC();
        case 0x39:
            return AND(absy_rd);
        case 0x3D:
            return AND(absx_rd);
        case 0x3E:
            return ROL(absx_wr);
        case 0x40:
            return RTI();
        case 0x41:
            return EOR(xind);
        case 0x45:
            return EOR(zp);
        case 0x46:
            return LSR(zp);
        case 0x48:
            return PHA();
        case 0x49:
            return EOR(imm);
        case 0x4A:
            return LSR_A();
        case 0x4C:
            return JMP(absl);
        case 0x4D:
            return EOR(absl);
        case 0x4E:
            return LSR(absl);
        case 0x50:
            return BVC(rel);
        case 0x51:
            return EOR(indy_rd);
        case 0x55:
            return EOR(zpx);
        case 0x56:
            return LSR(zpx);
        case 0x58:
            return CLI();
        case 0x59:
            return EOR(absy_rd);
        case 0x5D:
            return EOR(absx_rd);
        case 0x5E:
            return LSR(absx_wr);
        case 0x60:
            return RTS();
        case 0x61:
            return _ADC(xind);
        case 0x65:
            return _ADC(zp);
        case 0x66:
            return ROR(zp);
        case 0x68:
            return PLA();
        case 0x69:
            return _ADC(imm);
        case 0x6A:
            return ROR_A();
        case 0x6C:
            return JMP(ind);
        case 0x6D:
            return _ADC(absl);
        case 0x6E:
            return ROR(absl);
        case 0x70:
            return BVS(rel);
        case 0x71:
            return _ADC(indy_rd);
        case 0x75:
            return _ADC(zpx);
        case 0x76:
            return ROR(zpx);
        case 0x78:
            return SEI();
        case 0x79:
            return _ADC(absy_rd);
        case 0x7D:
            return _ADC(absx_rd);
        case 0x7E:
            return ROR(absx_wr);
        case 0x81:
            return STA(xind);
        case 0x84:
            return STY(zp);
        case 0x85:
            return STA(zp);
        case 0x86:
            return STX(zp);
        case 0x88:
            return DEY();
        case 0x8A:
            return TXA();
        case 0x8C:
            return STY(absl);
        case 0x8D:
            return STA(absl);
        case 0x8E:
            return STX(absl);
        case 0x90:
            return BCC(rel);
        case 0x91:
            return STA(indy_wr);
        case 0x94:
            return STY(zpx);
        case 0x95:
            return STA(zpx);
        case 0x96:
            return STX(zpy);
        case 0x98:
            return TYA();
        case 0x99:
            return STA(absy_wr);
        case 0x9A:
            return TXS();
        case 0x9D:
            return STA(absx_wr);
        case 0xA0:
            return LDY(imm);
        case 0xA1:
            return LDA(xind);
        case 0xA2:
            return LDX(imm);
        case 0xA4:
            return LDY(zp);
        case 0xA5:
            return LDA(zp);
        case 0xA6:
            return LDX(zp);
        case 0xA8:
            return TAY();
        case 0xA9:
            return LDA(imm);
        case 0xAA:
            return TAX();
        case 0xAC:
            return LDY(absl);
        case 0xAD:
            return LDA(absl);
        case 0xAE:
            return LDX(absl);
        case 0xB0:
            return BCS(rel);
        case 0xB1:
            return LDA(indy_rd);
        case 0xB4:
            return LDY(zpx);
        case 0xB5:
            return LDA(zpx);
        case 0xB6:
            return LDX(zpy);
        case 0xB8:
            return CLV();
        case 0xB9:
            return LDA(absy_rd);
        case 0xBA:
            return TSX();
        case 0xBC:
            return LDY(absx_rd);
        case 0xBD:
            return LDA(absx_rd);
        case 0xBE:
            return LDX(absy_rd);
        case 0xC0:
            return CPY(imm);
        case 0xC1:
            return CMP(xind);
        case 0xC4:
            return CPY(zp);
        case 0xC5:
            return CMP(zp);
        case 0xC6:
            return DEC(zp);
        case 0xC8:
            return INY();
        case 0xC9:
            return CMP(imm);
        case 0xCA:
            return DEX();
        case 0xCC:
            return CPY(absl);
        case 0xCD:
            return CMP(absl);
        case 0xCE:
            return DEC(absl);
        case 0xD0:
            return BNE(rel);
        case 0xD1:
            return CMP(indy_rd);
        case 0xD5:
            return CMP(zpx);
        case 0xD6:
            return DEC(zpx);
        case 0xD8:
            return CLD();
        case 0xD9:
            return CMP(absy_rd);
        case 0xDD:
            return CMP(absx_rd);
        case 0xDE:
            return DEC(absx_wr);
        case 0xE0:
            return CPX(imm);
        case 0xE1:
            return SBC(xind);
        case 0xE4:
            return CPX(zp);
        case 0xE5:
            return SBC(zp);
        case 0xE6:
            return INC(zp);
        case 0xE8:
            return INX();
        case 0xE9:
            return SBC(imm);
        case 0xEA:
            return NOP();
        case 0xEC:
            return CPX(absl);
        case 0xED:
            return SBC(absl);
        case 0xEE:
            return INC(absl);
        case 0xF0:
            return BEQ(rel);
        case 0xF1:
            return SBC(indy_rd);
        case 0xF5:
            return SBC(zpx);
        case 0xF6:
            return INC(zpx);
        case 0xF8:
            return SED();
        case 0xF9:
            return SBC(absy_rd);
        case 0xFD:
            return SBC(absx_rd);
        case 0xFE:
            return INC(absx_wr);
        // Illegal opcodes
        case 0x03:
            return SLO(xind);
        case 0x07:
            return SLO(zp);
        case 0x0F:
            return SLO(absl);
        case 0x13:
            return SLO(indy_rd);
        case 0x17:
            return SLO(zpx);
        case 0x1B:
            return SLO(absy_rd);
        case 0x1F:
            return SLO(absx_rd);
        case 0x23:
            return RLA(xind);
        case 0x27:
            return RLA(zp);
        case 0x2F:
            return RLA(absl);
        case 0x33:
            return RLA(indy_rd);
        case 0x37:
            return RLA(zpx);
        case 0x3B:
            return RLA(absy_rd);
        case 0x3F:
            return RLA(absx_rd);
        case 0x43:
            return SRE(xind);
        case 0x47:
            return SRE(zp);
        case 0x4F:
            return SRE(absl);
        case 0x53:
            return SRE(indy_rd);
        case 0x57:
            return SRE(zpx);
        case 0x5B:
            return SRE(absy_rd);
        case 0x5F:
            return SRE(absx_rd);
        case 0x63:
            return RRA(xind);
        case 0x67:
            return RRA(zp);
        case 0x6F:
            return RRA(absl);
        case 0x73:
            return RRA(indy_rd);
        case 0x77:
            return RRA(zpx);
        case 0x7B:
            return RRA(absy_rd);
        case 0x7F:
            return RRA(absx_rd);
        case 0x83:
            return SAX(xind);
        case 0x87:
            return SAX(zp);
        case 0x8F:
            return SAX(absl);
        case 0x97:
            return SAX(zpy);
        case 0xA3:
            return LAX(xind);
        case 0xA7:
            return LAX(zp);
        case 0xAB:
            return LAX(imm);
        case 0xAF:
            return LAX(absl);
        case 0xB3:
            return LAX(indy_rd);
        case 0xB7:
            return LAX(zpy);
        case 0xBF:
            return LAX(absy_rd);
        case 0xC3:
            return DCP(xind);
        case 0xC7:
            return DCP(zp);
        case 0xCB:
            return AXS(imm);
        case 0xCF:
            return DCP(absl);
        case 0xD3:
            return DCP(indy_rd);
        case 0xD7:
            return DCP(zpx);
        case 0xDB:
            return DCP(absy_rd);
        case 0xDF:
            return DCP(absx_rd);
        case 0xE3:
            return ISC(xind);
        case 0xE7:
            return ISC(zp);
        case 0xEB:
            return SBC(imm);
        case 0xEF:
            return ISC(absl);
        case 0xF3:
            return ISC(indy_rd);
        case 0xF7:
            return ISC(zpx);
        case 0xFB:
            return ISC(absy_rd);
        case 0xFF:
            return ISC(absx_rd);
        case 0x1A:
        case 0x3A:
        case 0x5A:
        case 0x7A:
        case 0xDA:
        case 0xFA:
            return NOP();
        case 0x04:
        case 0x44:
        case 0x64:
            return SKB(zp);
        case 0x14:
        case 0x34:
        case 0x54:
        case 0x74:
        case 0xD4:
        case 0xF4:
            return SKB(zpx);
        case 0x80:
        case 0x82:
        case 0x89:
        case 0xC2:
        case 0xE2:
            return SKB(imm);
        case 0x0C:
            return SKB(absl);
        case 0x1C:
        case 0x3C:
        case 0x5C:
        case 0x7C:
        case 0xDC:
        case 0xFC:
            return SKB(absx_rd);
        default:
            printf("Unsupported instruction: 0x%02X\n", op);
            return NOP();
    }
}

void cpu_init(void) {
    cpu.cycle = 0;
    cpu.registers.a = 0x00;
    cpu.registers.x = 0x00;
    cpu.registers.y = 0x00;
    cpu.registers.s = 0x00;
    cpu.registers.p = 0x00 | (1 << STATUS_INT_DISABLE) | (1 << STATUS_UNUSED);
    cpu.interrupt.nmi = 0;
    cpu.interrupt.irq = 0;
    INT_RESET();
}

void cpu_reset(void) {
    cpu_init();
}

void cpu_run(void) {
    if (cpu.interrupt.nmi) {
        INT_NMI();
    } else if (
      cpu.interrupt.irq && !NTH_BIT(cpu.registers.p, STATUS_INT_DISABLE)) {
        INT_IRQ();
    }
    exec_inst();
}

void cpu_set_NMI(bool enable) {
    cpu.interrupt.nmi = enable;
}

void cpu_set_IRQ(bool enable) {
    cpu.interrupt.irq = enable;
}

void cpu_get_state(cpu_state_t* state) {
    *state = cpu;
}