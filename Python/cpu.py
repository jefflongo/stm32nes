## Registers ##

# uint_16, Program counter register
PC = 0x0000
# uint_8, Stack pointer register
S = 0xFF
# uint_8, Accumulator register
A = 0x00
# uint_8, GP register, can modify stack pointer
X = 0x00
# uint_8, GP register, cannot modify stack pointer
Y = 0x00
# uint_8, Processor status flags:
# [0] C: Carry flag
# [1] Z: Zero flag
# [2] I: Interrupt disable
# [3] D: Decimal mode, can be set/cleared but not used
# [4] B: Break command
# [5] -: Not used
# [6] V: Overflow flag
# [7] N: Negative flag
P = 0x30

## Memory ##

# uint_8, 2 kb internal ram
# $0000-$00FF (256 bytes)   - Zero Page
# $0100-$01FF (256 bytes)   - Stack memory
# $0200-$07FF (1536 bytes)  - RAM
ram = [0] * 0x800
# $0800-$0FFF (2048 bytes)  - Mirror of $000-07FF
# $1000-$17FF (2048 bytes)  - Mirror of $000-07FF
# $1800-$1FFF (2048 bytes)  - Mirror of $000-07FF
# $2000-$2007 (8 bytes)     - I/O registers
# $2008-$3FFF (8184 bytes)  - Mirror of $2000-$2007 (repeated)
# $4000-$401F (32 bytes)    - I/O registers
# $4020-$5FFF (8160 bytes)  - Expansion ROM
# $6000-$7FFF (8192 bytes)  - SRAM
# $8000-$FFFF (32768 bytes) - PRG-ROM
# $FFFA-$FFFB (2 bytes)     - NMI handler routine
# $FFFC-$FFFD (2 bytes)     - Power on reset handler routine
# $FFFE-$FFFF (2 bytes)     - BRK handler routine

## CPU tick ##

def tick():
    pass

## Read/Write ##

def rd(addr):
    global ram
    return ram[addr]

def wr(addr, data):
    global ram
    ram[addr % 0x800] = data

## Stack operations ##

def push(data):
    global S
    wr(0x100 | S, data)
    S -= 1

def pull():
    global S
    S += 1
    return rd(0x100 | S)

## Flag adjustment ##

def setC():
    global P
    P |= 0x01

def clrC():
    global P
    P &= 0xFE

def getC():
    global P
    return P & 0x01

def setZ():
    global P
    P |= 0x02

def clrZ():
    global P
    P &= 0xFD

def getZ():
    global P
    return (P >> 1) & 0x01

def setI():
    global P
    P |= 0x04

def clrI():
    global P
    P &= 0xFB

def getI():
    return (P >> 2) & 0x01

def setD():
    global P
    P |= 0x08

def clrD():
    global P
    P &= 0xF7

def getD():
    global P
    return (P >> 3) & 0x01

def setV():
    global P
    P |= 0x40

def clrV():
    global P
    P &= 0xBF

def getV():
    return (P >> 6) & 0x01

def setN():
    global P
    P |= 0x80

def clrN():
    global P
    P &= 0x7F

def getN():
    global P
    return (P >> 7) & 0x01

def updateC(d):
    if (d > 0xFF):
        setC()
    else:
        clrC()

def updateZ(d):
    d &= 0xFF
    if (d == 0):
        setZ()
    else:
        clrZ()

def updateV(d1, d2, r):
    if ((~(d1^d2) & (d1^r) & 0x80) >> 7):
        setV()
    else:
        clrV()

def updateN(d):
    if ((d & 0x80) >> 7):
        setN()
    else:
        clrN()

## Addressing modes ##

# Immediate:
# - Return current PC and increment PC (immediate stored here)
def imm():
    global PC
    PC += 1
    return PC - 1
# ZP:
# - Read the immediate, increment PC
# - Return the immediate
def zp():
    global PC
    addr = rd(PC)
    PC += 1
    tick()
    return addr
# ZP,X:
# - Read the immediate, increment PC
# - Calculate imm + X, include wraparound
# - Return the new address
def zpx():
    global PC
    addr = rd(PC)
    PC += 1
    tick()
    addr = (rd(addr) + X) % 0x100
    tick()
    return addr
# ZP,Y:
# - Read the immediate, increment PC
# - Calculate imm + Y, include wraparound
# - Return the new address
def zpy():
    global PC
    addr = rd(PC)
    PC += 1
    tick()
    addr = (rd(addr) + Y) % 0x100
    tick()
    return addr
# Absolute:
# - Read the immediate, increment PC
# - Merge new immediate with old immediate, increment PC
# - Return the merged address
def abs():
    global PC
    addr = rd(PC)
    PC += 1
    tick()
    addr |= (rd(PC) << 8)
    PC += 1
    tick()
    return addr
# Absolute,X:
# - Read the immediate, increment PC
# - Read the new immediate, add the old immediate with X, increment PC
# - If the sum of old imm and X overflows, reread the address next tick
# - Merge old imm + X with new imm, return the merged address
def absx():
    global PC
    addrl = rd(PC)
    PC += 1
    tick()
    addrh = rd(PC)
    addrl += X
    PC += 1
    tick()
    if (addrl & 0xFF00 != 0):
        tick()
    return addrl + (addrh << 8)
# Absolute,Y:
# - Read the immediate, increment PC
# - Read the new immediate, add the old immediate with Y, increment PC
# - If the sum of old imm and Y overflows, reread the address next tick
# - Merge old imm + Y with new imm, return the merged address
def absy():
    global PC
    addrl = rd(PC)
    PC += 1
    tick()
    addrh = rd(PC)
    addrl += Y
    PC += 1
    tick()
    if (addrl & 0xFF00 != 0):
        tick()
    return addrl + (addrh << 8)
# Absolute Indirect (JMP only):
# - Read imm (pointer low), increment PC
# - Read imm (pointer high), increment PC
# - Read low byte from pointer
# - Read high byte from pointer (wrap around) and return the merged address
def ind():
    global PC
    ptrl = rd(PC)
    PC += 1
    tick()
    ptrh = rd(PC)
    PC += 1
    tick()
    ptr = ptrl | (ptrh << 8)
    addrl = rd(ptr)
    tick()
    addrh = rd((ptr & 0xFF00) | ((ptr + 1) % 0x100))
    tick()
    return addrl | (addrh << 8)
# X,Indirect:
# - Read imm (pointer), increment PC
# - Read address at imm + X on zero page
# - Read low byte from pointer
# - Read high byte from pointer and return the merged address
def xind():
    global PC
    ptr = rd(PC)
    PC += 1
    tick()
    ptr = (rd(ptr) + X) % 0x100
    tick()
    addrl = rd(ptr)
    tick()
    addrh = rd((ptr + 1) % 0x100)
    return addrl | (addrh << 8)
# Y,Indirect:
# - Read imm (pointer), increment PC
# - Read address at imm + Y on zero page
# - Read low byte from pointer
# - Read high byte from pointer and return the merged address
def yind():
    global PC
    ptr = rd(PC)
    PC += 1
    tick()
    ptr = (rd(ptr) + Y) % 0x100
    tick()
    addrl = rd(ptr)
    tick()
    addrh = rd((ptr + 1) % 0x100)
    return addrl | (addrh << 8)
# Indirect,X:
# - Read imm (pointer), increment PC
# - Read low byte from pointer on zero page
# - Read high byte from pointer on zero page, add X to low byte
# - If the sum of low byte and X overflows, reread the address next tick
# - Return the merged address
def indx():
    global PC
    ptr = rd(PC)
    PC += 1
    tick()
    addrl = rd(ptr)
    tick()
    addrh = rd((ptr + 1) % 0x100)
    addrl += X
    tick()
    if (addrl & 0xFF00 != 0):
        tick()
    return addrl + (addrh << 8)
# Indirect,Y:
# - Read imm (pointer), increment PC
# - Read low byte from pointer on zero page
# - Read high byte from pointer on zero page, add Y to low byte
# - If the sum of low byte and X overflows, reread the address next tick
# - Return the merged address
def indy():
    global PC
    ptr = rd(PC)
    PC += 1
    tick()
    addrl = rd(ptr)
    tick()
    addrh = rd((ptr + 1) % 0x100)
    addrl += Y
    tick()
    if (addrl & 0xFF00 != 0):
        tick()
    return addrl + (addrh << 8)
# Relative (Assuming branch taken):
# - Read imm (offset), increment PC
# - Add offset to PC
# - If adding the offset overflowed the low byte of PC, add a cycle
def rel():
    global PC
    imm = rd(PC)
    PC += 1
    tick()
    addr = PC + imm
    tick()
    if ((addr & 0x100) != (PC & 0x100)):
        tick()
    return addr
    
## Instructions ## 

# Load/Store operations
def LDA(m):
    global PC, S, A, X, Y, P
    addr = m()
    d = rd(addr)
    updateZ(d)
    updateN(d)
    A = d
    tick()

def LDX(m):
    global PC, S, A, X, Y, P
    addr = m()
    d = rd(addr)
    updateZ(d)
    updateN(d)
    X = d
    tick()

def LDY(m):
    global PC, S, A, X, Y, P
    addr = m()
    d = rd(addr)
    updateZ(d)
    updateN(d)
    Y = d
    tick()

def STA(m):
    global PC, S, A, X, Y, P
    addr = m()
    wr(addr, A)
    tick()

def STX(m):
    global PC, S, A, X, Y, P
    addr = m()
    wr(addr, X)
    tick()

def STY(m):
    global PC, S, A, X, Y, P
    addr = m()
    wr(addr, Y)
    tick()

def TXA():
    global PC, S, A, X, Y, P
    updateZ(X)
    updateN(X)
    A = X
    tick()

def TXS():
    global PC, S, A, X, Y, P
    S = X
    tick()

def TYA():
    global PC, S, A, X, Y, P
    updateZ(Y)
    updateN(Y)
    A = Y
    tick()

def TAX():
    global PC, S, A, X, Y, P
    updateZ(A)
    updateN(A)
    X = A
    tick()

def TAY():
    global PC, S, A, X, Y, P
    updateZ(A)
    updateN(A)
    Y = A
    tick()

def TSX():
    global PC, S, A, X, Y, P
    updateZ(S)
    updateN(S)
    X = S
    tick()

# Stack operations
def PHP():
    global PC, S, A, X, Y, P
    tick()
    push(P | 0x10)
    tick()
    
def PLP():
    global PC, S, A, X, Y, P
    tick()
    tick()
    P = (P & 0x30) | (pull() & 0xCF)
    tick()

def PHA():
    global PC, S, A, X, Y, P
    tick()
    push(A)
    tick()

def PLA():
    global PC, S, A, X, Y, P
    tick()
    tick()
    A = pull()
    tick()

# Arithmetic/Logical operations
def ADC(m):
    global PC, S, A, X, Y, P
    addr = m()
    d = rd(addr)
    s = A + d + (P & 0x01) # uint16
    updateC(s)
    updateZ(s)
    updateV(A, d, s)
    updateN(s)
    A = s # cast s to uint8
    tick()

def SBC(m):
    global PC, S, A, X, Y, P
    addr = m()
    d = rd(addr)
    s = A + ~d + (P & 0x01) # uint16
    updateC(s)
    updateZ(s)
    updateV(A, d, s)
    updateN(s)
    A = s # cast s to uint8
    tick()

def AND(m):
    global PC, S, A, X, Y, P
    addr = m()
    d = rd(addr)
    A &= d
    updateZ(A)
    updateN(A)
    tick()

def EOR(m):
    global PC, S, A, X, Y, P
    addr = m()
    d = rd(addr)
    A ^= d
    updateZ(A)
    updateN(A)
    tick()

def ORA(m):
    global PC, S, A, X, Y, P
    addr = m()
    d = rd(addr)
    A |= d
    updateZ(A)
    updateN(A)
    tick()

def BIT(m):
    global PC, S, A, X, Y, P
    addr = m()
    d = rd(addr)
    updateZ(A & d)
    P = (P & 0x3F) | (d & 0xC0)
    tick()

# Compares
def CMP(m):
    global PC, S, A, X, Y, P
    addr = m()
    d = rd(addr)
    s = A + ~d + 1
    updateC(s)
    updateZ(s)
    updateN(s)
    tick()

def CPX(m):
    global PC, S, A, X, Y, P
    addr = m()
    d = rd(addr)
    s = X + ~d + 1
    updateC(s)
    updateZ(s)
    updateN(s)
    tick()

def CPY(m):
    global PC, S, A, X, Y, P
    addr = m()
    d = rd(addr)
    s = Y + ~d + 1
    updateC(s)
    updateZ(s)
    updateN(s)
    tick()

# Increments/Decrements
def INC(m):
    global PC, S, A, X, Y, P
    addr = m()
    d = rd(addr)
    tick()
    d += 1
    updateZ(d)
    updateN(d)
    tick()
    wr(addr, d)
    tick()

def INX():
    global PC, S, A, X, Y, P
    X += 1
    updateZ(X)
    updateN(X)
    tick()

def INY():
    global PC, S, A, X, Y, P
    Y += 1
    updateZ(Y)
    updateN(Y)
    tick()

def DEC(m):
    global PC, S, A, X, Y, P
    addr = m()
    d = rd(addr)
    tick()
    d -= 1
    updateZ(d)
    updateN(d)
    tick()
    wr(addr, d)
    tick()

def DEX():
    global PC, S, A, X, Y, P
    X -= 1
    updateZ(X)
    updateN(X)
    tick()

def DEY():
    global PC, S, A, X, Y, P
    Y -= 1
    updateZ(Y)
    updateN(Y)
    tick()

# Shifts
def ASL(m):
    global PC, S, A, X, Y, P
    addr = m()
    d = rd(addr)
    tick()
    if ((d & 0x80) >> 7):
        setC()
    else:
        clrC()
    d <<= 1
    updateZ(d)
    updateN(d)
    tick()
    wr(addr, d)
    tick()

def ASL_A():
    global PC, S, A, X, Y, P
    if ((A & 0x80) >> 7):
        setC()
    else:
        clrC()
    A <<= 1
    updateZ(A)
    updateN(A)
    tick()

def LSR(m):
    global PC, S, A, X, Y, P
    addr = m()
    d = rd(addr)
    tick()
    if (d & 0x01):
        setC()
    else:
        clrC()
    d >>= 1
    updateZ(d)
    tick()
    wr(addr, d)
    tick()

def LSR_A():
    global PC, S, A, X, Y, P
    if (A & 0x01):
        setC()
    else:
        clrC()
    A >>= 1
    updateZ(A)
    tick()

def ROL(m):
    global PC, S, A, X, Y, P
    addr = m()
    d = rd(addr)
    tick()
    c = P & 0x01
    if ((d & 0x80) >> 7):
        setC()
    else:
        clrC()
    d = (d << 1) | c
    updateZ(d)
    updateN(d)
    tick()
    wr(addr, d)
    tick()

def ROL_A():
    global PC, S, A, X, Y, P
    c = P & 0x01
    if ((A & 0x80) >> 7):
        setC()
    else:
        clrC()
    A = (A << 1) | c
    updateZ(A)
    updateN(A)
    tick()

def ROR(m):
    global PC, S, A, X, Y, P
    addr = m()
    d = rd(addr)
    tick()
    c = P & 0x01
    if (d & 0x01):
        setC()
    else:
        clrC()
    d = (d >> 1) | (c << 7)
    updateZ(d)
    updateN(d)
    tick()
    wr(addr, d)
    tick()

def ROR_A():
    global PC, S, A, X, Y, P
    c = P & 0x01
    if (A & 0x01):
        setC()
    else:
        clrC()
    A = (A >> 1) | (c << 7)
    updateZ(A)
    updateN(A)
    tick()

# Jumps/calls
def JMP(m):
    global PC, S, A, X, Y, P
    addr = m()
    PC = addr

def JSR():
    global PC, S, A, X, Y, P
    addrl = rd(PC)
    PC += 1
    tick()
    tick()
    push(PC >> 8)
    tick()
    push(PC & 0xFF)
    tick()
    addrh = rd(PC)
    PC = addrl | (addrh << 8)
    tick()

def RTS():
    global PC, S, A, X, Y, P
    tick()
    tick()
    addrl = pull()
    tick()
    addrh = pull()
    tick()
    PC = addrl | (addrh << 8)
    tick()
    PC += 1
    tick()

def RTI():
    global PC, S, A, X, Y, P
    tick()
    tick()
    P = (P & 0x30) | (pull() & 0xCF)
    tick()
    addrl = pull()
    tick()
    addrh = pull()
    PC = addrl | (addrh << 8)
    tick()
    
# Branches
def BPL(m):
    global PC, S, A, X, Y, P
    if (not getN()):
        addr = m()
        PC = addr
    else:
        PC += 1
        tick()

def BMI(m):
    global PC, S, A, X, Y, P
    if (getN()):
        addr = m()
        PC = addr
    else:
        PC += 1
        tick()

def BVC(m):
    global PC, S, A, X, Y, P
    if (not getV()):
        addr = m()
        PC = addr
    else:
        PC += 1
        tick()

def BVS(m):
    global PC, S, A, X, Y, P
    if (getV()):
        addr = m()
        PC = addr
    else:
        PC += 1
        tick()

def BCC(m):
    global PC, S, A, X, Y, P
    if (not getC()):
        addr = m()
        PC = addr
    else:
        PC += 1
        tick()

def BCS(m):
    global PC, S, A, X, Y, P
    if (getC()):
        addr = m()
        PC = addr
    else:
        PC += 1
        tick()

def BNE(m):
    global PC, S, A, X, Y, P
    if (not getZ()):
        addr = m()
        PC = addr
    else:
        PC += 1
        tick()

def BEQ(m):
    global PC, S, A, X, Y, P
    if (getZ()):
        addr = m()
        PC = addr
    else:
        PC += 1
        tick()

# Status register operations
def CLC():
    clrC()
    tick()

def CLI():
    clrI()
    tick()

def CLV():
    clrV()
    tick()

def CLD():
    clrD()
    tick()

def SEC():
    setC()
    tick()

def SEI():
    setI()
    tick()

def SED():
    setD()
    tick()

# System functions
def NOP():
    tick()

def BRK():
    global PC, S, A, X, Y, P
    PC += 1
    tick()
    push(PC >> 8)
    tick()
    push(PC & 0xFF)
    tick()
    push(P | 0x10)
    setI()
    tick()
    addrl = rd(0xFFFE)
    tick()
    addrh = rd(0xFFFF)
    PC = addrl | (addrh >> 8)
    tick()
    
## CPU Execution ##

def init():
    global PC, S, A, X, Y, P
    S = 0xFF

def exec_inst():
    global PC, S, A, X, Y, P
    op = rd(PC)
    PC += 1
    tick()
    if   (op == 0x00): BRK()
    elif (op == 0x01): ORA(xind())
    elif (op == 0x05): ORA(zp())
    elif (op == 0x06): ASL(zp())
    elif (op == 0x08): PHP()
    elif (op == 0x09): ORA(imm())
    elif (op == 0x0A): ASL_A()
    elif (op == 0x0D): ORA(abs())
    elif (op == 0x0E): ASL(abs())
    elif (op == 0x10): BPL(rel())
    elif (op == 0x11): ORA(indy())
    elif (op == 0x15): ORA(zpx())
    elif (op == 0x16): ASL(zpx())
    elif (op == 0x18): CLC()
    elif (op == 0x19): ORA(absy())
    elif (op == 0x1D): ORA(absx())
    elif (op == 0x1E): ASL(absx())
    elif (op == 0x20): JSR()
    elif (op == 0x21): AND(xind())
    elif (op == 0x24): BIT(zp())
    elif (op == 0x25): AND(zp())
    elif (op == 0x26): ROL(zp())
    elif (op == 0x28): PLP()
    elif (op == 0x29): AND(imm())
    elif (op == 0x2A): ROL_A()
    elif (op == 0x2C): BIT(abs())
    elif (op == 0x2D): AND(abs())
    elif (op == 0x2E): ROL(abs())
    elif (op == 0x30): BMI(rel())
    elif (op == 0x31): AND(indy())
    elif (op == 0x35): AND(zpx())
    elif (op == 0x36): ROL(zpx())
    elif (op == 0x38): SEC()
    elif (op == 0x39): AND(absy())
    elif (op == 0x3D): AND(absx())
    elif (op == 0x3E): ROL(absx())
    elif (op == 0x40): RTI()
    elif (op == 0x41): EOR(xind())
    elif (op == 0x45): EOR(zp())
    elif (op == 0x46): LSR(zp())
    elif (op == 0x48): PHA()
    elif (op == 0x49): EOR(imm())
    elif (op == 0x4A): LSR_A()
    elif (op == 0x4C): JMP(abs())
    elif (op == 0x4D): EOR(abs())
    elif (op == 0x4E): LSR(abs())
    elif (op == 0x50): BVC(rel())
    elif (op == 0x51): EOR(indy())
    elif (op == 0x55): EOR(zpx())
    elif (op == 0x56): LSR(zpx())
    elif (op == 0x58): CLI()
    elif (op == 0x59): EOR(absy())
    elif (op == 0x5D): EOR(absx())
    elif (op == 0x5E): LSR(absx())
    elif (op == 0x60): RTS()
    elif (op == 0x61): ADC(xind())
    elif (op == 0x65): ADC(zp())
    elif (op == 0x66): ROR(zp())
    elif (op == 0x68): PLA()
    elif (op == 0x69): ADC(imm())
    elif (op == 0x6A): ROR_A()
    elif (op == 0x6C): JMP(ind())
    elif (op == 0x6D): ADC(abs())
    elif (op == 0x6E): ROR(absx())
    elif (op == 0x70): BVS(rel())
    elif (op == 0x71): ADC(indy())
    elif (op == 0x75): ADC(zpx())
    elif (op == 0x76): ROR(zpx())
    elif (op == 0x78): SEI()
    elif (op == 0x79): ADC(absy())
    elif (op == 0x7D): ADC(absx())
    elif (op == 0x7E): ROR(absx())
    elif (op == 0x81): STA(xind())
    elif (op == 0x84): STY(zp())
    elif (op == 0x85): STA(zp())
    elif (op == 0x86): STX(zp())
    elif (op == 0x87): DEY()
    elif (op == 0x8A): TXA()
    elif (op == 0x8C): STY(abs())
    elif (op == 0x8D): STA(abs())
    elif (op == 0x8E): STX(abs())
    elif (op == 0x90): BCC(rel())
    elif (op == 0x91): STA(indy())
    elif (op == 0x94): STY(zpx())
    elif (op == 0x95): STA(zpx())
    elif (op == 0x96): STX(zpy())
    elif (op == 0x98): TYA()
    elif (op == 0x99): STA(absy())
    elif (op == 0x9A): TXS()
    elif (op == 0x9D): STA(absx())
    elif (op == 0xA0): LDY(imm())
    elif (op == 0xA1): LDA(xind())
    elif (op == 0xA2): LDX(imm())
    elif (op == 0xA4): LDY(zp())
    elif (op == 0xA5): LDA(zp())
    elif (op == 0xA6): LDX(zp())
    elif (op == 0xA8): TAY()
    elif (op == 0xA9): LDA(imm())
    elif (op == 0xAA): TAX()
    elif (op == 0xAC): LDY(abs())
    elif (op == 0xAD): LDA(abs())
    elif (op == 0xAE): LDX(abs())
    elif (op == 0xB0): BCS(rel())
    elif (op == 0xB1): LDA(indy())
    elif (op == 0xB4): LDY(zpx())
    elif (op == 0xB5): LDA(zpx())
    elif (op == 0xB6): LDX(zpy())
    elif (op == 0xB8): CLV()
    elif (op == 0xB9): LDA(absy())
    elif (op == 0xBA): TSX()
    elif (op == 0xBC): LDY(absx())
    elif (op == 0xBD): LDA(absx())
    elif (op == 0xBE): LDX(absy())
    elif (op == 0xC0): CPY(imm())
    elif (op == 0xC1): CMP(xind())
    elif (op == 0xC4): CPY(zp())
    elif (op == 0xC5): CMP(zp())
    elif (op == 0xC6): DEC(zp())
    elif (op == 0xC8): INY()
    elif (op == 0xC9): CMP(imm())
    elif (op == 0xCA): DEX()
    elif (op == 0xCC): CPY(abs())
    elif (op == 0xCD): CMP(abs())
    elif (op == 0xCE): DEC(abs())
    elif (op == 0xD0): BNE(rel())
    elif (op == 0xD1): CMP(indy())
    elif (op == 0xD5): CMP(zpx())
    elif (op == 0xD6): DEC(zpx())
    elif (op == 0xD8): CLD()
    elif (op == 0xD9): CMP(absy())
    elif (op == 0xDD): CMP(absx())
    elif (op == 0xDE): DEC(absx())
    elif (op == 0xE0): CPX(imm())
    elif (op == 0xE1): SBC(xind())
    elif (op == 0xE4): CPX(zp())
    elif (op == 0xE5): SBC(zp())
    elif (op == 0xE6): INC(zp())
    elif (op == 0xE8): INX()
    elif (op == 0xE9): SBC(imm())
    elif (op == 0xEA): NOP()
    elif (op == 0xEC): CPX(abs())
    elif (op == 0xED): SBC(abs())
    elif (op == 0xEE): INC(abs())
    elif (op == 0xF0): BEQ(rel())
    elif (op == 0xF1): SBC(indy())
    elif (op == 0xF5): SBC(zpx())
    elif (op == 0xF6): INC(zpx())
    elif (op == 0xF8): SED()
    elif (op == 0xF9): SBC(absy())
    elif (op == 0xFD): SBC(absx())
    elif (op == 0xFE): INC(absx())
    else:
        print("Invalid Instruction/n/r!") 
        NOP()

def run():
    