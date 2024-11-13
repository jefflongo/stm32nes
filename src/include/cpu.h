#pragma once

#include "nes.h"

void cpu_init(nes_t* state);
void cpu_step(nes_t* state);
void cpu_set_nmi(nes_t* state, bool enable);
void cpu_set_irq(nes_t* state, bool enable);
