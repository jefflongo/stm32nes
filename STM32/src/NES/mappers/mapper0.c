#include "NES/mappers/mapper0.h"

static u8 *prgBank, *chrBank;

void mapper0_init(u8* pBank, u8* cBank) {
	prgBank = pBank;
	chrBank = cBank;

	*prgBank = 0;
	*chrBank = 0;
}
