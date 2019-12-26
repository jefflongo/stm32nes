#ifndef CPU_H_
#define CPU_H_

#include "types.h"

typedef struct {
    u16 pc;
    u8 s;
    u8 a;
    u8 x;
    u8 y;
    u8 p;
} cpu_registers_t;

typedef struct {
    bool nmi;
    bool irq;
} cpu_interrupt_t;

typedef struct {
    cpu_registers_t registers;
    cpu_interrupt_t interrupt;
    u64 cycle;
} cpu_state_t;

void cpu_init(void);
void cpu_reset(void);
void cpu_run(void);
void cpu_set_NMI(bool enable);
void cpu_set_IRQ(bool enable);
void cpu_get_state(cpu_state_t* state);

#endif // CPU_H_