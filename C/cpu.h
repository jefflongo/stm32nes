#ifndef CPU_H_
#define CPU_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "mapper.h"

typedef uint16_t u16;
typedef uint8_t u8;
typedef u16 (*mode)();

void init();
void run();
void setNMI(u8 value);
void setIRQ(u8 value);
void log(char* s, int length);

#endif // CPU_H_