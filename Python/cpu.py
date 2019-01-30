## Registers ##

# uint_16, Program counter register
PC = None
# uint_8, Stack pointer register
SP = None
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


## Addressing modes ##

# Implicit: Implied by instruction
# Accumulator: Operate directly on accumulator
# Immediate: Constant, stored at current PC
def imm():
    # return PC++
    PC += 1
    return PC - 1
# Zero Page: 8 bit address, stored at PC
def zp():
    return rd(imm())
# Zero Page X: 8 bit address stored at PC + value in X, wraps
def zpx():
    tick()
    return (zp() + X) % 0x100
# Zero Page Y: 8 bit address stored at PC + value in Y, wraps
def zpy():
    tick()
    return (zp() + Y) % 0x100

## Instructions ##

# Load/Store operations
def LDA():
def LDX():
def LDY():
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