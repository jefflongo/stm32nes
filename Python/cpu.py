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

## Read/Write ##
def rd(addr):
    return ram[addr]

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
def LDA():
    A = rd(addr)
    tick()

def LDX():
    X = rd(addr)
    tick()

def LDY():
    Y = rd(addr)
    tick()

def STA():
def STX():
def STY():
# Register transfer operations
def TXA():
def TYA():
def TXS():
def TAY():
def TAX():
def TSX():
# Stack operations
def PHP():
def PLP():
def PHA():
def PLA():
# Arithmetic/Logical operations
def ADC():
def SBC():
def AND():
def EOR():
def ORA():
def BIT():
# Compares
def CMP():
def CPX():
def CPY():
# Increments/Decrements
def INC():
def INX():
def INY():
def DEC():
def DEX():
def DEY():
# Shifts
def ASL():
def LSR():
def ROL():
def ROR():
# Jumps/calls
def JMP():
def JSR():
def RTS():
def RTI():
# Branches
def BPL():
def BMI():
def BVC():
def BVS():
def BCC():
def BCS():
def BNE():
def BEQ():
# Status register operations
def CLC():
def CLI():
def CLV():
def CLD():
def SEC():
def SEI():
def SED():
# System functions
def NOP():
def BRK():