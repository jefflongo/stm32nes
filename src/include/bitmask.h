#ifndef BITMASK_H_
#define BITMASK_H_

// Single bit operations
#define NTH_BIT(reg, n) (((reg) >> (n)) & 1U)

#define SET_NTH_BIT(reg, n) reg |= (1U << (n))
#define CLEAR_NTH_BIT(reg, n) reg &= ~(1U << (n))
#define FLIP_NTH_BIT(reg, n) reg ^= ~(1U << (n))
#define ASSIGN_NTH_BIT(reg, n, x) reg = (reg & ~(1U << (n))) | (!!(x) << (n))

// Multi bit operations
#define BITS(reg, mask, pos) (((reg) & (mask)) >> (pos))

#define SET_BITS(reg, mask) reg |= mask
#define CLEAR_BITS(reg, mask) reg &= ~(mask)
#define ASSIGN_BITS(reg, mask, pos, x)                                         \
    reg = (reg & ~(mask)) | (((x) << (pos)) & (mask))

#endif // BITMASK_H_