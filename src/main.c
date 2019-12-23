#include "cpu.h"
#include "mapper.h"

#include <stdio.h>
#include <stdlib.h>

int main() {
    cartridge_init("smb.nes");
    cpu_init();
    while (1) {
        cpu_run();
    }

    return 0;
}