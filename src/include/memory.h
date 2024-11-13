#pragma once

#include "nes.h"

void memory_init(nes_t* state);
u8 memory_read(nes_t* state, u16 addr);
void memory_write(nes_t* state, u16 addr, u8 data);
