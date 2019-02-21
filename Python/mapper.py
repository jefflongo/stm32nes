# Represents 0x4020 - 0xFFFF
mem = [0] * 0xBFE0

# Offset of addr - 0x8000 + 0x10 in the ROM
def mapper_rd(addr):
    return mem[addr - 0x4020]

def mapper_wr(addr, data):
    mem[addr - 0x4020] = data

def load_rom(filename):
    rom = list(open(filename, "rb").read())
    if (rom):
        for i in range(0x4000):
            mem[0x8000 - 0x4020 + i] = rom[i + 0x10]
            mem[0x8000 - 0x4020 + 0x4000 + i] = rom[i + 0x10]
        return True
    else:
        return False
