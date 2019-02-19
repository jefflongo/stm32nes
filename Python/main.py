from cpu import init, run
from mapper import load_rom

def main():
    if (load_rom("nestest.nes")):
        init()
    for i in range(100):
        run()

if __name__ == "__main__":
    main()