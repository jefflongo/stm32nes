from cpu import init, run, log
from mapper import load_rom

def main():
    nestest()

def nestest():
    file = open("nestest.txt").readlines()
    if (not load_rom("nestest.nes") or not file):
        return
    init()
    for line in file:
        test = log()
        exp = "PC:" + line[0:4] + \
        " A:" + line[50:52] + \
        " X:" + line[55:57] + \
        " Y:" + line[60:62] + \
        " P:" + line[65:67] + \
        " SP:" + line[71:73] + \
        " CYC:" + line[90:len(line) - 1]
        assert test == exp, "\nExpected " + exp + "\nGot      " + test
        print(test)
        run()

if __name__ == "__main__":
    main()