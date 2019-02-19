ram = [0] * 0x10000

def read(addr):
    return ram[addr]

def write(addr, data): 
    ram[addr] = data