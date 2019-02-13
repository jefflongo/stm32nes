## Registers ##

# uint_16, Program counter register
PC = None
# uint_8, Stack pointer register
S = None
# uint_8, Accumulator register
A = None
# uint_8, GP register, can modify stack pointer
X = None
# uint_8, GP register, cannot modify stack pointer
Y = None
# uint_8, Processor status flags:
# [0] C: Carry flag
# [1] Z: Zero flag
# [2] I: Interrupt disable
# [3] D: Decimal mode, can be set/cleared but not used
# [4] B: Break command
# [5] -: Not used
# [6] V: Overflow flag
# [7] N: Negative flag
P = None

## Memory ##

# uint_8, 2 kb internal ram
# $0000-$00FF (256 bytes)   - Zero Page
# $0100-$01FF (256 bytes)   - Stack memory
# $0200-$07FF (1536 bytes)  - RAM
ram = [None] * 0x800
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
    return ram[addr]

def wr(addr, data):
    ram[addr % 0x800] = data

## Flag adjustment ##

def setC():
    P |= 0x01

def clrC():
    P &= 0xFE

def setZ():
    P |= 0x02

def clrZ():
    P &= 0xFD

def setI():
    P |= 0x04

def clrI():
    P &= 0xFB

def setD():
    P |= 0x08

def clrD():
    P &= 0xF7

def setB():
    P |= 0x10

def clrB():
    P &= 0xEF

def setV():
    P |= 0x40

def clrV():
    P &= 0xBF

def setN():
    P |= 0x80

def clrN():
    P &= 0x7F

def updateC(d):
    if (d > 0xFF):
        setC()
    else:
        clrC()

def updateZ(d):
    if (d == 0):
        setZ()
    else:
        clrZ()

def updateV(d1, d2, r):
    if (~(d1^d2) & (d1^r) & 0x80):
        setV()
    else:
        clrV()

def updateN(d):
    if (d & 0x80):
        setN()
    else:
        clrN()

## Addressing modes ##

# Immediate:
# - Return current PC and increment PC (immediate stored here)
def imm():
    PC += 1
    return PC - 1
# ZP:
# - Read the immediate, increment PC
# - Return the immediate
def zp():
    addr = rd(PC)
    PC += 1
    tick()
    return addr
# ZP,X:
# - Read the immediate, increment PC
# - Calculate imm + X, include wraparound
# - Return the new address
def zpx():
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
    return addrl | (addrh << 8)
# X,Indirect:
# - Read imm (pointer), increment PC
# - Read address at imm + X on zero page
# - Read low byte from pointer
# - Read high byte from pointer and return the merged address
def xind():
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
    addr = m()
    d = rd(addr)
    updateZ(d)
    updateN(d)
    A = d
    tick()

def LDX(m):
    addr = m()
    d = rd(addr)
    updateZ(d)
    updateN(d)
    X = d
    tick()

def LDY(m):
    addr = m()
    d = rd(addr)
    updateZ(d)
    updateN(d)
    Y = d
    tick()

def STA(m):
    addr = m()
    wr(addr, A)
    tick()
    updateZ(Y)
    updateN(Y)
    A = Y
    tick()

def TXS():
    S = X
    tick()

def TAY():
    updateZ(A)
    updateN(A)
    Y = A
    tick()

def TAX():
    updateZ(A)
    updateN(A)
    X = A
    tick()

def TSX():
    updateZ(S)
    updateN(S)
    X = S
    tick()

# Stack operations
def PHP():
def PLP():
def PHA():
def PLA():

# Arithmetic/Logical operations
def ADC(m):
def SBC(m):

def AND(m):
    addr = m()
    d = rd(addr)
    A &= d
    updateZ(A)
    updateN(A)
    tick()

def EOR(m):
    addr = m()
    d = rd(addr)
    A ^= d
    updateZ(A)
    updateN(A)
    tick()

def ORA(m):
    addr = m()
    d = rd(addr)
    A |= d
    updateZ(A)
    updateN(A)
    tick()

def BIT(m):
    addr = m()
    d = rd(addr)
    updateZ(A & d)
    P = (P & 0x3F) | (d & 0xC0)
    tick()

# Compares
def CMP(m):
def CPX(m):
def CPY(m):

# Increments/Decrements
def INC(m):
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
    X += 1
    updateZ(X)
    updateN(X)
    tick()

def INY():
    Y += 1
    updateZ(Y)
    updateN(Y)
    tick()

def DEC(m):
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
    X -= 1
    updateZ(X)
    updateN(X)
    tick()

def DEY():
    Y -= 1
    updateZ(Y)
    updateN(Y)
    tick()

# Shifts
def ASL(m):
    addr = m()
    d = rd(addr)
    tick()
    if (d & 0x80):
        setC()
    else:
        clrC()
    d <<= 1
    updateZ(d)
    updateN(d)
    tick()
    wr(addr, d)
    tick()

def LSR(m):
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

def ROL(m):
    addr = m()
    d = rd(addr)
    tick()
    c = P & 0x01
    if (d & 0x80):
        setC()
    else:
        clrC()
    d = (d << 1) | c
    updateZ(d)
    updateN(d)
    tick()
    wr(addr, d)
    tick()

def ROR(m):
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

# Jumps/calls
def JMP(m):
def JSR(m):
def RTS():
def RTI():

# Branches
def BPL(m):
def BMI(m):
def BVC(m):
def BVS(m):
def BCC(m):
def BCS(m):
def BNE(m):
def BEQ(m):

# Status register operations
def CLC():
    clrC()
    tick()

def CLI():
    clrI()
    tick()

def CLV():
    clearV()
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

## CPU Execution ##

def init():

def exec():
    