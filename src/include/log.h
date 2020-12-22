#pragma once

#ifdef PRINTF_SUPPORTED
#include <stdio.h>
#define LOG(fmt, ...) printf((fmt), ##__VA_ARGS__)
#else
#define LOG(fmt, ...)
#endif // PRINTF_SUPPORTED
