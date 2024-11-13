#pragma once

#ifdef PRINTF_SUPPORTED
#include <stdio.h>
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...)
#endif // PRINTF_SUPPORTED
