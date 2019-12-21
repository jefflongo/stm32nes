#include "cpu.h"
#include "mapper.h"

#include <stdio.h>
#include <stdlib.h>

int main() {
    load_rom("smb.nes");
    cpu_init();
    while (1) {
        cpu_run();
    }

    return 0;
}