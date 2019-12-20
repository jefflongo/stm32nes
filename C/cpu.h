#ifndef CPU_H_
#define CPU_H_

#include "types.h"

typedef struct {
    u16 PC;
    u8 S;
    u8 A;
    u8 X;
    u8 Y;
    u8 P;
    u64 cycle;
} cpu_state_t;

void cpu_init(void);
void cpu_run(void);
void cpu_set_NMI(bool enable);
void cpu_set_IRQ(bool enable);
void cpu_get_state(cpu_state_t* state);

#endif // CPU_H_