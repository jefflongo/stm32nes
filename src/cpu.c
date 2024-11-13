#include "cpu.h"

#include "bitmask.h"
#include "cartridge.h"
#include "log.h"
#include "nes.h"
#include "ppu.h"

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
// System RAM
static u8 ram[NES_RAM_SIZE];

static void tick(void) {
    // ppu_tick();
    // ppu_tick();
    // ppu_tick();
    cpu.cycle++;
}

/* Read/Write operations */

static u8 rd(u16 addr) {
    if (addr < 0x2000) {
        return ram[addr % NES_RAM_SIZE];
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
        return cartridge_prg_rd(addr);
    }
}

static void wr(u16 addr, u8 data) {
    if (addr < 0x2000) {
        ram[addr % NES_RAM_SIZE] = data;
    } else if (addr < 0x4000) {
        // TODO: ppu_reg_access(addr % 0x100, 0, WRITE);
    } else if (addr <= 0x4015) {
        // TODO: APU, Peripherals..
    } else if (addr == 0x4016) {
        // TODO: controller_wr(data);
    } else if (addr == 0x4017) {
        // TODO
    } else {
        cartridge_prg_wr(addr, data);
    }
}

/* Stack operations */

static void push(u8 data) {
    wr(NES_STACK_OFFSET | cpu.S--, data);
}

static u8 pull(void) {
    return rd(NES_STACK_OFFSET | ++cpu.S);
}

/* Flag adjustment */

static inline void updateC(u16 r) {
    ASSIGN_NTH_BIT(cpu.P, STATUS_CARRY, r > 0xFF);
}

static inline void updateZ(u8 d) {
    ASSIGN_NTH_BIT(cpu.P, STATUS_ZERO, d == 0);
}

static inline void updateV(u8 d1, u8 d2, u16 r) {
    ASSIGN_NTH_BIT(cpu.P, STATUS_OVERFLOW, (0xFF ^ d1 ^ d2) & (d1 ^ r) & 0x80);
}

static inline void updateN(u8 d) {
    ASSIGN_NTH_BIT(cpu.P, STATUS_NEGATIVE, NTH_BIT(d, 7));
}

/* Interrupts */

static void INT_NMI(void) {
    // Throw away fetched instruction
    tick();
    // Suppress PC increment
    tick();
    push(cpu.PC >> 8);
    tick();
    push(cpu.PC & 0xFF);
    tick();
    push(cpu.P | (1 << STATUS_UNUSED));
    tick();
    SET_NTH_BIT(cpu.P, STATUS_INT_DISABLE);
    u8 addrl = rd(NES_NMI_HANDLE_OFFSET);
    tick();
    u8 addrh = rd(NES_NMI_HANDLE_OFFSET + 1);
    cpu.PC = addrl | (addrh << 8);
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
    cpu.S -= 3;
    tick();
    tick();
    tick();
    SET_NTH_BIT(cpu.P, STATUS_INT_DISABLE);
    u8 addrl = rd(NES_RESET_HANDLE_OFFSET);
    tick();
    u8 addrh = rd(NES_RESET_HANDLE_OFFSET + 1);
    cpu.PC = addrl | (addrh << 8);
    tick();
}

static void INT_IRQ(void) {
    // Throw away fetched instruction
    tick();
    // Suppress PC increment
    tick();
    push(cpu.PC >> 8);
    tick();
    push(cpu.PC & 0xFF);
    tick();
    push(cpu.P | (1 << STATUS_UNUSED));
    tick();
    SET_NTH_BIT(cpu.P, STATUS_INT_DISABLE);
    u8 addrl = rd(NES_IRQ_BRK_HANDLE_OFFSET);
    tick();
    u8 addrh = rd(NES_IRQ_BRK_HANDLE_OFFSET + 1);
    cpu.PC = addrl | (addrh << 8);
    tick();
}

static void BRK(void) {
    cpu.PC++;
    tick();
    push(cpu.PC >> 8);
    tick();
    push(cpu.PC & 0xFF);
    push(cpu.P | (1 << STATUS_BREAK) | (1 << STATUS_UNUSED));
    tick();
    SET_NTH_BIT(cpu.P, STATUS_INT_DISABLE);
    u8 addrl = rd(NES_IRQ_BRK_HANDLE_OFFSET);
    tick();
    u8 addrh = rd(NES_IRQ_BRK_HANDLE_OFFSET + 1);
    cpu.PC = addrl | (addrh << 8);
    tick();
}

/* Addressing modes */

// Immediate:
// - Return current PC and increment PC (immediate stored here)
static u16 imm(void) {
    return cpu.PC++;
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
    u16 addr = (zp() + cpu.X) % 0x100;
    tick();
    return addr;
}
// ZP, Y:
// - Read the immediate, increment PC
// - Calculate imm + Y, include wraparound
// - Return the new address
static u16 zpy(void) {
    u16 addr = (zp() + cpu.Y) % 0x100;
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
    addrl += cpu.X;
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
    addrl += cpu.X;
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
    addrl += cpu.Y;
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
    addrl += cpu.Y;
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
    addrl += cpu.Y;
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
    addrl += cpu.Y;
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
    u16 addr = cpu.PC + imm;
    tick();
    if ((addr & 0x100) != (cpu.PC & 0x100)) tick();
    return addr;
}

/* Instructions */

// Load / Store operations
static void LDA(mode m) {
    u8 d = rd(m());
    updateZ(d);
    updateN(d);
    cpu.A = d;
    tick();
}

static void LDX(mode m) {
    u8 d = rd(m());
    updateZ(d);
    updateN(d);
    cpu.X = d;
    tick();
}

static void LDY(mode m) {
    u8 d = rd(m());
    updateZ(d);
    updateN(d);
    cpu.Y = d;
    tick();
}

static void STA(mode m) {
    wr(m(), cpu.A);
    tick();
}

static void STX(mode m) {
    wr(m(), cpu.X);
    tick();
}

static void STY(mode m) {
    wr(m(), cpu.Y);
    tick();
}

static void TXA(void) {
    updateZ(cpu.X);
    updateN(cpu.X);
    cpu.A = cpu.X;
    tick();
}

static void TXS(void) {
    cpu.S = cpu.X;
    tick();
}

static void TYA(void) {
    updateZ(cpu.Y);
    updateN(cpu.Y);
    cpu.A = cpu.Y;
    tick();
}

static void TAX(void) {
    updateZ(cpu.A);
    updateN(cpu.A);
    cpu.X = cpu.A;
    tick();
}

static void TAY(void) {
    updateZ(cpu.A);
    updateN(cpu.A);
    cpu.Y = cpu.A;
    tick();
}

static void TSX(void) {
    updateZ(cpu.S);
    updateN(cpu.S);
    cpu.X = cpu.S;
    tick();
}

// Stack operations
static void PHP(void) {
    // Throw away next byte
    tick();
    push(cpu.P | (1 << STATUS_BREAK) | (1 << STATUS_UNUSED));
    tick();
}

static void PLP(void) {
    // Throw away next byte
    tick();
    // S increment
    tick();
    cpu.P = (pull() & ~(1 << STATUS_BREAK)) | (1 << STATUS_UNUSED);
    tick();
}

static void PHA(void) {
    // Throw away next byte
    tick();
    push(cpu.A);
    tick();
}

static void PLA(void) {
    // Throw away next byte
    tick();
    // S increment
    tick();
    cpu.A = pull();
    updateZ(cpu.A);
    updateN(cpu.A);
    tick();
}

// Arithmetic / Logical operations
static void _ADC(mode m) {
    u8 d = rd(m());
    u16 s = cpu.A + d + NTH_BIT(cpu.P, STATUS_CARRY);
    updateC(s);
    updateZ((u8)s);
    updateV(cpu.A, d, s);
    updateN((u8)s);
    cpu.A = (u8)s;
    tick();
}

static void SBC(mode m) {
    u8 d = rd(m());
    u16 s = cpu.A + (d ^ 0xFF) + NTH_BIT(cpu.P, STATUS_CARRY);
    updateC(s);
    updateZ((u8)s);
    updateV(cpu.A, (d ^ 0xFF), s);
    updateN((u8)s);
    cpu.A = (u8)s;
    tick();
}

static void AND(mode m) {
    u8 d = rd(m());
    cpu.A &= d;
    updateZ(cpu.A);
    updateN(cpu.A);
    tick();
}

static void EOR(mode m) {
    u8 d = rd(m());
    cpu.A ^= d;
    updateZ(cpu.A);
    updateN(cpu.A);
    tick();
}

static void ORA(mode m) {
    u8 d = rd(m());
    cpu.A |= d;
    updateZ(cpu.A);
    updateN(cpu.A);
    tick();
}

static void BIT(mode m) {
    u8 d = rd(m());
    updateZ(cpu.A & d);
    ASSIGN_NTH_BIT(cpu.P, STATUS_NEGATIVE, NTH_BIT(d, 7));
    ASSIGN_NTH_BIT(cpu.P, STATUS_OVERFLOW, NTH_BIT(d, 6));
    tick();
}

// Compares
static void CMP(mode m) {
    u8 d = rd(m());
    u16 s = cpu.A + (d ^ 0xFF) + 1;
    updateC(s);
    updateZ((u8)s);
    updateN((u8)s);
    tick();
}

static void CPX(mode m) {
    u8 d = rd(m());
    u16 s = cpu.X + (d ^ 0xFF) + 1;
    updateC(s);
    updateZ((u8)s);
    updateN((u8)s);
    tick();
}

static void CPY(mode m) {
    u8 d = rd(m());
    u16 s = cpu.Y + (d ^ 0xFF) + 1;
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
    cpu.X++;
    updateZ(cpu.X);
    updateN(cpu.X);
    tick();
}

static void INY(void) {
    cpu.Y++;
    updateZ(cpu.Y);
    updateN(cpu.Y);
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
    cpu.X--;
    updateZ(cpu.X);
    updateN(cpu.X);
    tick();
}

static void DEY(void) {
    cpu.Y--;
    updateZ(cpu.Y);
    updateN(cpu.Y);
    tick();
}

// Shifts
static void ASL(mode m) {
    u16 addr = m();
    u8 d = rd(addr);
    tick();
    ASSIGN_NTH_BIT(cpu.P, STATUS_CARRY, NTH_BIT(d, 7));
    d <<= 1;
    updateZ(d);
    updateN(d);
    tick();
    wr(addr, d);
    tick();
}

static void ASL_A(void) {
    ASSIGN_NTH_BIT(cpu.P, STATUS_CARRY, NTH_BIT(cpu.A, 7));
    cpu.A <<= 1;
    updateZ(cpu.A);
    updateN(cpu.A);
    tick();
}

static void LSR(mode m) {
    u16 addr = m();
    u8 d = rd(addr);
    tick();
    ASSIGN_NTH_BIT(cpu.P, STATUS_CARRY, NTH_BIT(d, 0));
    d >>= 1;
    updateZ(d);
    updateN(d);
    tick();
    wr(addr, d);
    tick();
}

static void LSR_A(void) {
    ASSIGN_NTH_BIT(cpu.P, STATUS_CARRY, NTH_BIT(cpu.A, 0));
    cpu.A >>= 1;
    updateZ(cpu.A);
    updateN(cpu.A);
    tick();
}

static void ROL(mode m) {
    u16 addr = m();
    u8 d = rd(addr);
    tick();
    bool c = NTH_BIT(cpu.P, STATUS_CARRY);
    ASSIGN_NTH_BIT(cpu.P, STATUS_CARRY, NTH_BIT(d, 7));
    d = (d << 1) | c;
    updateZ(d);
    updateN(d);
    tick();
    wr(addr, d);
    tick();
}

static void ROL_A(void) {
    bool c = NTH_BIT(cpu.P, STATUS_CARRY);
    ASSIGN_NTH_BIT(cpu.P, STATUS_CARRY, NTH_BIT(cpu.A, 7));
    cpu.A = (cpu.A << 1) | c;
    updateZ(cpu.A);
    updateN(cpu.A);
    tick();
}

static void ROR(mode m) {
    u16 addr = m();
    u8 d = rd(addr);
    tick();
    bool c = NTH_BIT(cpu.P, STATUS_CARRY);
    ASSIGN_NTH_BIT(cpu.P, STATUS_CARRY, NTH_BIT(d, 0));
    d = (d >> 1) | (c << 7);
    updateZ(d);
    updateN(d);
    tick();
    wr(addr, d);
    tick();
}

static void ROR_A(void) {
    bool c = NTH_BIT(cpu.P, STATUS_CARRY);
    ASSIGN_NTH_BIT(cpu.P, STATUS_CARRY, NTH_BIT(cpu.A, 0));
    cpu.A = (cpu.A >> 1) | (c << 7);
    updateZ(cpu.A);
    updateN(cpu.A);
    tick();
}

// Jumps / calls
static void JMP(mode m) {
    cpu.PC = m();
}

static void JSR(void) {
    u8 addrl = rd(cpu.PC);
    cpu.PC += 1;
    tick();
    tick();
    push(cpu.PC >> 8);
    tick();
    push(cpu.PC & 0xFF);
    tick();
    u8 addrh = rd(cpu.PC);
    cpu.PC = addrl | (addrh << 8);
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
    cpu.PC = addrl | (addrh << 8);
    tick();
    cpu.PC += 1;
    tick();
}

static void RTI(void) {
    // Throw away next byte
    tick();
    // S increment
    tick();
    cpu.P = (pull() & ~(1 << STATUS_BREAK)) | (1 << STATUS_UNUSED);
    tick();
    u8 addrl = pull();
    tick();
    u8 addrh = pull();
    cpu.PC = addrl | (addrh << 8);
    tick();
}

// Branches
static void BPL(mode m) {
    if (!NTH_BIT(cpu.P, STATUS_NEGATIVE)) {
        cpu.PC = m();
    } else {
        cpu.PC++;
        tick();
    }
}

static void BMI(mode m) {
    if (NTH_BIT(cpu.P, STATUS_NEGATIVE)) {
        cpu.PC = m();
    } else {
        cpu.PC++;
        tick();
    }
}

static void BVC(mode m) {
    if (!NTH_BIT(cpu.P, STATUS_OVERFLOW)) {
        cpu.PC = m();
    } else {
        cpu.PC++;
        tick();
    }
}

static void BVS(mode m) {
    if (NTH_BIT(cpu.P, STATUS_OVERFLOW)) {
        cpu.PC = m();
    } else {
        cpu.PC++;
        tick();
    }
}

static void BCC(mode m) {
    if (!NTH_BIT(cpu.P, STATUS_CARRY)) {
        cpu.PC = m();
    } else {
        cpu.PC++;
        tick();
    }
}

static void BCS(mode m) {
    if (NTH_BIT(cpu.P, STATUS_CARRY)) {
        cpu.PC = m();
    } else {
        cpu.PC++;
        tick();
    }
}

static void BNE(mode m) {
    if (!NTH_BIT(cpu.P, STATUS_ZERO)) {
        cpu.PC = m();
    } else {
        cpu.PC++;
        tick();
    }
}

static void BEQ(mode m) {
    if (NTH_BIT(cpu.P, STATUS_ZERO)) {
        cpu.PC = m();
    } else {
        cpu.PC++;
        tick();
    }
}

// Status register operations
static void CLC(void) {
    CLEAR_NTH_BIT(cpu.P, STATUS_CARRY);
    tick();
}

static void CLI(void) {
    CLEAR_NTH_BIT(cpu.P, STATUS_INT_DISABLE);
    tick();
}

static void CLV(void) {
    CLEAR_NTH_BIT(cpu.P, STATUS_OVERFLOW);
    tick();
}

static void CLD(void) {
    CLEAR_NTH_BIT(cpu.P, STATUS_DECIMAL);
    tick();
}

static void SEC(void) {
    SET_NTH_BIT(cpu.P, STATUS_CARRY);
    tick();
}

static void SEI(void) {
    SET_NTH_BIT(cpu.P, STATUS_INT_DISABLE);
    tick();
}

static void SED(void) {
    SET_NTH_BIT(cpu.P, STATUS_DECIMAL);
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
    cpu.A = d;
    cpu.X = d;
    tick();
}

static void SAX(mode m) {
    u16 addr = m();
    wr(addr, cpu.A & cpu.X);
    tick();
}

static void AXS(mode m) {
    u8 d = rd(m());
    u16 s = (cpu.A & cpu.X) + (d ^ 0xFF) + 1;
    updateC(s);
    updateZ((u8)s);
    updateN((u8)s);
    cpu.X = (u8)s;
    tick();
}

static void DCP(mode m) {
    u16 addr = m();
    u8 d = rd(addr);
    tick();
    d--;
    u16 s = cpu.A + (d ^ 0xFF) + 1;
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
    u16 s = cpu.A + (d ^ 0xFF) + NTH_BIT(cpu.P, STATUS_CARRY);
    updateC(s);
    updateZ((u8)s);
    updateV(cpu.A, (d ^ 0xFF), s);
    updateN((u8)s);
    cpu.A = (u8)s;
    tick();
    wr(addr, d);
    tick();
}

static void SLO(mode m) {
    u16 addr = m();
    u8 d = rd(addr);
    tick();
    ASSIGN_NTH_BIT(cpu.P, STATUS_CARRY, NTH_BIT(d, 7));
    d <<= 1;
    cpu.A |= d;
    updateZ(cpu.A);
    updateN(cpu.A);
    tick();
    wr(addr, d);
    tick();
}

static void RLA(mode m) {
    u16 addr = m();
    u8 d = rd(addr);
    tick();
    bool c = NTH_BIT(cpu.P, STATUS_CARRY);
    ASSIGN_NTH_BIT(cpu.P, STATUS_CARRY, NTH_BIT(d, 7));
    d = (d << 1) | c;
    cpu.A &= d;
    updateZ(cpu.A);
    updateN(cpu.A);
    tick();
    wr(addr, d);
    tick();
}

static void SRE(mode m) {
    u16 addr = m();
    u8 d = rd(addr);
    tick();
    ASSIGN_NTH_BIT(cpu.P, STATUS_CARRY, NTH_BIT(d, 0));
    d >>= 1;
    cpu.A ^= d;
    updateZ(cpu.A);
    updateN(cpu.A);
    tick();
    wr(addr, d);
    tick();
}

static void RRA(mode m) {
    u16 addr = m();
    u8 d = rd(addr);
    tick();
    bool c = NTH_BIT(cpu.P, STATUS_CARRY);
    ASSIGN_NTH_BIT(cpu.P, STATUS_CARRY, NTH_BIT(d, 0));
    d = (d >> 1) | (c << 7);
    u16 s = cpu.A + d + NTH_BIT(cpu.P, STATUS_CARRY);
    updateC(s);
    updateZ((u8)s);
    updateV(cpu.A, d, s);
    updateN((u8)s);
    cpu.A = (u8)s;
    tick();
    wr(addr, d);
    tick();
}

/* CPU Execution */

static void exec_inst(void) {
    // Fetch
    u8 op = rd(cpu.PC++);
    tick();
    // Decode/Execute
    switch (op) {
        case 0x00:
            BRK();
            break;
        case 0x01:
            ORA(xind);
            break;
        case 0x05:
            ORA(zp);
            break;
        case 0x06:
            ASL(zp);
            break;
        case 0x08:
            PHP();
            break;
        case 0x09:
            ORA(imm);
            break;
        case 0x0A:
            ASL_A();
            break;
        case 0x0D:
            ORA(absl);
            break;
        case 0x0E:
            ASL(absl);
            break;
        case 0x10:
            BPL(rel);
            break;
        case 0x11:
            ORA(indy_rd);
            break;
        case 0x15:
            ORA(zpx);
            break;
        case 0x16:
            ASL(zpx);
            break;
        case 0x18:
            CLC();
            break;
        case 0x19:
            ORA(absy_rd);
            break;
        case 0x1D:
            ORA(absx_rd);
            break;
        case 0x1E:
            ASL(absx_wr);
            break;
        case 0x20:
            JSR();
            break;
        case 0x21:
            AND(xind);
            break;
        case 0x24:
            BIT(zp);
            break;
        case 0x25:
            AND(zp);
            break;
        case 0x26:
            ROL(zp);
            break;
        case 0x28:
            PLP();
            break;
        case 0x29:
            AND(imm);
            break;
        case 0x2A:
            ROL_A();
            break;
        case 0x2C:
            BIT(absl);
            break;
        case 0x2D:
            AND(absl);
            break;
        case 0x2E:
            ROL(absl);
            break;
        case 0x30:
            BMI(rel);
            break;
        case 0x31:
            AND(indy_rd);
            break;
        case 0x35:
            AND(zpx);
            break;
        case 0x36:
            ROL(zpx);
            break;
        case 0x38:
            SEC();
            break;
        case 0x39:
            AND(absy_rd);
            break;
        case 0x3D:
            AND(absx_rd);
            break;
        case 0x3E:
            ROL(absx_wr);
            break;
        case 0x40:
            RTI();
            break;
        case 0x41:
            EOR(xind);
            break;
        case 0x45:
            EOR(zp);
            break;
        case 0x46:
            LSR(zp);
            break;
        case 0x48:
            PHA();
            break;
        case 0x49:
            EOR(imm);
            break;
        case 0x4A:
            LSR_A();
            break;
        case 0x4C:
            JMP(absl);
            break;
        case 0x4D:
            EOR(absl);
            break;
        case 0x4E:
            LSR(absl);
            break;
        case 0x50:
            BVC(rel);
            break;
        case 0x51:
            EOR(indy_rd);
            break;
        case 0x55:
            EOR(zpx);
            break;
        case 0x56:
            LSR(zpx);
            break;
        case 0x58:
            CLI();
            break;
        case 0x59:
            EOR(absy_rd);
            break;
        case 0x5D:
            EOR(absx_rd);
            break;
        case 0x5E:
            LSR(absx_wr);
            break;
        case 0x60:
            RTS();
            break;
        case 0x61:
            _ADC(xind);
            break;
        case 0x65:
            _ADC(zp);
            break;
        case 0x66:
            ROR(zp);
            break;
        case 0x68:
            PLA();
            break;
        case 0x69:
            _ADC(imm);
            break;
        case 0x6A:
            ROR_A();
            break;
        case 0x6C:
            JMP(ind);
            break;
        case 0x6D:
            _ADC(absl);
            break;
        case 0x6E:
            ROR(absl);
            break;
        case 0x70:
            BVS(rel);
            break;
        case 0x71:
            _ADC(indy_rd);
            break;
        case 0x75:
            _ADC(zpx);
            break;
        case 0x76:
            ROR(zpx);
            break;
        case 0x78:
            SEI();
            break;
        case 0x79:
            _ADC(absy_rd);
            break;
        case 0x7D:
            _ADC(absx_rd);
            break;
        case 0x7E:
            ROR(absx_wr);
            break;
        case 0x81:
            STA(xind);
            break;
        case 0x84:
            STY(zp);
            break;
        case 0x85:
            STA(zp);
            break;
        case 0x86:
            STX(zp);
            break;
        case 0x88:
            DEY();
            break;
        case 0x8A:
            TXA();
            break;
        case 0x8C:
            STY(absl);
            break;
        case 0x8D:
            STA(absl);
            break;
        case 0x8E:
            STX(absl);
            break;
        case 0x90:
            BCC(rel);
            break;
        case 0x91:
            STA(indy_wr);
            break;
        case 0x94:
            STY(zpx);
            break;
        case 0x95:
            STA(zpx);
            break;
        case 0x96:
            STX(zpy);
            break;
        case 0x98:
            TYA();
            break;
        case 0x99:
            STA(absy_wr);
            break;
        case 0x9A:
            TXS();
            break;
        case 0x9D:
            STA(absx_wr);
            break;
        case 0xA0:
            LDY(imm);
            break;
        case 0xA1:
            LDA(xind);
            break;
        case 0xA2:
            LDX(imm);
            break;
        case 0xA4:
            LDY(zp);
            break;
        case 0xA5:
            LDA(zp);
            break;
        case 0xA6:
            LDX(zp);
            break;
        case 0xA8:
            TAY();
            break;
        case 0xA9:
            LDA(imm);
            break;
        case 0xAA:
            TAX();
            break;
        case 0xAC:
            LDY(absl);
            break;
        case 0xAD:
            LDA(absl);
            break;
        case 0xAE:
            LDX(absl);
            break;
        case 0xB0:
            BCS(rel);
            break;
        case 0xB1:
            LDA(indy_rd);
            break;
        case 0xB4:
            LDY(zpx);
            break;
        case 0xB5:
            LDA(zpx);
            break;
        case 0xB6:
            LDX(zpy);
            break;
        case 0xB8:
            CLV();
            break;
        case 0xB9:
            LDA(absy_rd);
            break;
        case 0xBA:
            TSX();
            break;
        case 0xBC:
            LDY(absx_rd);
            break;
        case 0xBD:
            LDA(absx_rd);
            break;
        case 0xBE:
            LDX(absy_rd);
            break;
        case 0xC0:
            CPY(imm);
            break;
        case 0xC1:
            CMP(xind);
            break;
        case 0xC4:
            CPY(zp);
            break;
        case 0xC5:
            CMP(zp);
            break;
        case 0xC6:
            DEC(zp);
            break;
        case 0xC8:
            INY();
            break;
        case 0xC9:
            CMP(imm);
            break;
        case 0xCA:
            DEX();
            break;
        case 0xCC:
            CPY(absl);
            break;
        case 0xCD:
            CMP(absl);
            break;
        case 0xCE:
            DEC(absl);
            break;
        case 0xD0:
            BNE(rel);
            break;
        case 0xD1:
            CMP(indy_rd);
            break;
        case 0xD5:
            CMP(zpx);
            break;
        case 0xD6:
            DEC(zpx);
            break;
        case 0xD8:
            CLD();
            break;
        case 0xD9:
            CMP(absy_rd);
            break;
        case 0xDD:
            CMP(absx_rd);
            break;
        case 0xDE:
            DEC(absx_wr);
            break;
        case 0xE0:
            CPX(imm);
            break;
        case 0xE1:
            SBC(xind);
            break;
        case 0xE4:
            CPX(zp);
            break;
        case 0xE5:
            SBC(zp);
            break;
        case 0xE6:
            INC(zp);
            break;
        case 0xE8:
            INX();
            break;
        case 0xE9:
            SBC(imm);
            break;
        case 0xEA:
            NOP();
            break;
        case 0xEC:
            CPX(absl);
            break;
        case 0xED:
            SBC(absl);
            break;
        case 0xEE:
            INC(absl);
            break;
        case 0xF0:
            BEQ(rel);
            break;
        case 0xF1:
            SBC(indy_rd);
            break;
        case 0xF5:
            SBC(zpx);
            break;
        case 0xF6:
            INC(zpx);
            break;
        case 0xF8:
            SED();
            break;
        case 0xF9:
            SBC(absy_rd);
            break;
        case 0xFD:
            SBC(absx_rd);
            break;
        case 0xFE:
            INC(absx_wr);
            break;
        // Illegal opcodes
        case 0x03:
            SLO(xind);
            break;
        case 0x07:
            SLO(zp);
            break;
        case 0x0F:
            SLO(absl);
            break;
        case 0x13:
            SLO(indy_rd);
            break;
        case 0x17:
            SLO(zpx);
            break;
        case 0x1B:
            SLO(absy_rd);
            break;
        case 0x1F:
            SLO(absx_rd);
            break;
        case 0x23:
            RLA(xind);
            break;
        case 0x27:
            RLA(zp);
            break;
        case 0x2F:
            RLA(absl);
            break;
        case 0x33:
            RLA(indy_rd);
            break;
        case 0x37:
            RLA(zpx);
            break;
        case 0x3B:
            RLA(absy_rd);
            break;
        case 0x3F:
            RLA(absx_rd);
            break;
        case 0x43:
            SRE(xind);
            break;
        case 0x47:
            SRE(zp);
            break;
        case 0x4F:
            SRE(absl);
            break;
        case 0x53:
            SRE(indy_rd);
            break;
        case 0x57:
            SRE(zpx);
            break;
        case 0x5B:
            SRE(absy_rd);
            break;
        case 0x5F:
            SRE(absx_rd);
            break;
        case 0x63:
            RRA(xind);
            break;
        case 0x67:
            RRA(zp);
            break;
        case 0x6F:
            RRA(absl);
            break;
        case 0x73:
            RRA(indy_rd);
            break;
        case 0x77:
            RRA(zpx);
            break;
        case 0x7B:
            RRA(absy_rd);
            break;
        case 0x7F:
            RRA(absx_rd);
            break;
        case 0x83:
            SAX(xind);
            break;
        case 0x87:
            SAX(zp);
            break;
        case 0x8F:
            SAX(absl);
            break;
        case 0x97:
            SAX(zpy);
            break;
        case 0xA3:
            LAX(xind);
            break;
        case 0xA7:
            LAX(zp);
            break;
        case 0xAB:
            LAX(imm);
            break;
        case 0xAF:
            LAX(absl);
            break;
        case 0xB3:
            LAX(indy_rd);
            break;
        case 0xB7:
            LAX(zpy);
            break;
        case 0xBF:
            LAX(absy_rd);
            break;
        case 0xC3:
            DCP(xind);
            break;
        case 0xC7:
            DCP(zp);
            break;
        case 0xCB:
            AXS(imm);
            break;
        case 0xCF:
            DCP(absl);
            break;
        case 0xD3:
            DCP(indy_rd);
            break;
        case 0xD7:
            DCP(zpx);
            break;
        case 0xDB:
            DCP(absy_rd);
            break;
        case 0xDF:
            DCP(absx_rd);
            break;
        case 0xE3:
            ISC(xind);
            break;
        case 0xE7:
            ISC(zp);
            break;
        case 0xEB:
            SBC(imm);
            break;
        case 0xEF:
            ISC(absl);
            break;
        case 0xF3:
            ISC(indy_rd);
            break;
        case 0xF7:
            ISC(zpx);
            break;
        case 0xFB:
            ISC(absy_rd);
            break;
        case 0xFF:
            ISC(absx_rd);
            break;
        case 0x1A:
        case 0x3A:
        case 0x5A:
        case 0x7A:
        case 0xDA:
        case 0xFA:
            NOP();
            break;
        case 0x04:
        case 0x44:
        case 0x64:
            SKB(zp);
            break;
        case 0x14:
        case 0x34:
        case 0x54:
        case 0x74:
        case 0xD4:
        case 0xF4:
            SKB(zpx);
            break;
        case 0x80:
        case 0x82:
        case 0x89:
        case 0xC2:
        case 0xE2:
            SKB(imm);
            break;
        case 0x0C:
            SKB(absl);
            break;
        case 0x1C:
        case 0x3C:
        case 0x5C:
        case 0x7C:
        case 0xDC:
        case 0xFC:
            SKB(absx_rd);
            break;
        default:
            LOG("Unsupported instruction: 0x%02X\n", op);
            NOP();
            break;
    }
}

void cpu_init(void) {
    cpu.A = 0x00;
    cpu.X = 0x00;
    cpu.Y = 0x00;
    cpu.S = 0x00;
    cpu.P = 0x00 | (1 << STATUS_INT_DISABLE) | (1 << STATUS_UNUSED);
    cpu.nmi = 0;
    cpu.irq = 0;
    INT_RESET();
}

void cpu_run(void) {
    if (cpu.nmi) {
        INT_NMI();
    } else if (cpu.irq && !NTH_BIT(cpu.P, STATUS_INT_DISABLE)) {
        INT_IRQ();
    }
    exec_inst();
}

void cpu_set_NMI(bool enable) {
    cpu.nmi = enable;
}

void cpu_set_IRQ(bool enable) {
    cpu.irq = enable;
}

void cpu_get_state(cpu_state_t* state) {
    *state = cpu;
}
