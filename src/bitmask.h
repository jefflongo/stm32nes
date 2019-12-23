#ifndef BITMASK_H_
#define BITMASK_H_

#define NTH_BIT(reg, n) (((reg) >> (n)) & 1U)

#define SET_NTH_BIT(reg, n) reg |= (1U << (n))
#define CLEAR_NTH_BIT(reg, n) reg &= ~(1U << (n))
#define FLIP_NTH_BIT(reg, n) reg ^= ~(1U << (n))
#define ASSIGN_NTH_BIT(reg, n, x) reg = (reg & ~(1U << (n))) | (!!(x) << (n))

#endif // BITMASK_H_