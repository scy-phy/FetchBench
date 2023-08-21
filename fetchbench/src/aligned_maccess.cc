#include "aligned_maccess.hh"

/**
 * Returns a pair of two function pointers to two non-inlining maccess
 * functions that are aligned at a 2^(colliding_bits) boundary.
 *
 * @param[in]  colliding_bits  The number of colliding bits
 *
 * @return     std::pair of function pointers to two maccess functions.
 */
pair<maccess_func_t,maccess_func_t> get_maccess_functions(size_t colliding_bits) {
	assert(colliding_bits >= 5 && colliding_bits <= 24);
	pair<maccess_func_t, maccess_func_t> funcs;
	switch(colliding_bits) {
		case 5:
			funcs = {maccess_5_1, maccess_5_2};
			break;
		case 6:
			funcs = {maccess_6_1, maccess_6_2};
			break;
		case 7:
			funcs = {maccess_7_1, maccess_7_2};
			break;
		case 8:
			funcs = {maccess_8_1, maccess_8_2};
			break;
		case 9:
			funcs = {maccess_9_1, maccess_9_2};
			break;
		case 10:
			funcs = {maccess_10_1, maccess_10_2};
			break;
		case 11:
			funcs = {maccess_11_1, maccess_11_2};
			break;
		case 12:
			funcs = {maccess_12_1, maccess_12_2};
			break;
		case 13:
			funcs = {maccess_13_1, maccess_13_2};
			break;
		case 14:
			funcs = {maccess_14_1, maccess_14_2};
			break;
		case 15:
			funcs = {maccess_15_1, maccess_15_2};
			break;
		case 16:
			funcs = {maccess_16_1, maccess_16_2};
			break;
		case 17:
			funcs = {maccess_17_1, maccess_17_2};
			break;
		case 18:
			funcs = {maccess_18_1, maccess_18_2};
			break;
		case 19:
			funcs = {maccess_19_1, maccess_19_2};
			break;
		case 20:
			funcs = {maccess_20_1, maccess_20_2};
			break;
		case 21:
			funcs = {maccess_21_1, maccess_21_2};
			break;
		case 22:
			funcs = {maccess_22_1, maccess_22_2};
			break;
		case 23:
			funcs = {maccess_23_1, maccess_23_2};
			break;
		case 24:
			funcs = {maccess_24_1, maccess_24_2};
			break;
		default:
			assert(false);
			break;
	}

	// make sure the compiler did what we intended

	// ensure that the lower (colliding_bits) bits of both functions are
	// actually equal
	assert(
		(((uintptr_t)funcs.first) & ((1ULL << colliding_bits)-1)) ==
		(((uintptr_t)funcs.second) & ((1ULL << colliding_bits)-1))
	);
	// ensure that the upper bits of both functions are actually different
	assert(
		(((uintptr_t)funcs.first) & ~((1ULL << colliding_bits)-1)) !=
		(((uintptr_t)funcs.second) & ~((1ULL << colliding_bits)-1))
	);
	// ensure that especially the (colliding_bits+1)'th bit is different
	assert(
		((((uintptr_t)funcs.first) >> colliding_bits) & 0b1) !=
		((((uintptr_t)funcs.second) >> colliding_bits) & 0b1)
	);

	return funcs;
}

__attribute__((aligned(1 << 5))) __attribute__ ((noinline)) void maccess_5_1(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 5))) __attribute__ ((noinline)) void maccess_5_2(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 6))) __attribute__ ((noinline)) void maccess_6_1(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 6))) __attribute__ ((noinline)) void maccess_6_2(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 7))) __attribute__ ((noinline)) void maccess_7_1(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 7))) __attribute__ ((noinline)) void maccess_7_2(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 8))) __attribute__ ((noinline)) void maccess_8_1(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 8))) __attribute__ ((noinline)) void maccess_8_2(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 9))) __attribute__ ((noinline)) void maccess_9_1(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 9))) __attribute__ ((noinline)) void maccess_9_2(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 10))) __attribute__ ((noinline)) void maccess_10_1(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 10))) __attribute__ ((noinline)) void maccess_10_2(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 11))) __attribute__ ((noinline)) void maccess_11_1(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 11))) __attribute__ ((noinline)) void maccess_11_2(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 12))) __attribute__ ((noinline)) void maccess_12_1(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 12))) __attribute__ ((noinline)) void maccess_12_2(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 13))) __attribute__ ((noinline)) void maccess_13_1(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 13))) __attribute__ ((noinline)) void maccess_13_2(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 14))) __attribute__ ((noinline)) void maccess_14_1(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 14))) __attribute__ ((noinline)) void maccess_14_2(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 15))) __attribute__ ((noinline)) void maccess_15_1(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 15))) __attribute__ ((noinline)) void maccess_15_2(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 16))) __attribute__ ((noinline)) void maccess_16_1(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 16))) __attribute__ ((noinline)) void maccess_16_2(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 17))) __attribute__ ((noinline)) void maccess_17_1(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 17))) __attribute__ ((noinline)) void maccess_17_2(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 18))) __attribute__ ((noinline)) void maccess_18_1(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 18))) __attribute__ ((noinline)) void maccess_18_2(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 19))) __attribute__ ((noinline)) void maccess_19_1(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 19))) __attribute__ ((noinline)) void maccess_19_2(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 20))) __attribute__ ((noinline)) void maccess_20_1(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 20))) __attribute__ ((noinline)) void maccess_20_2(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 21))) __attribute__ ((noinline)) void maccess_21_1(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 21))) __attribute__ ((noinline)) void maccess_21_2(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 22))) __attribute__ ((noinline)) void maccess_22_1(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 22))) __attribute__ ((noinline)) void maccess_22_2(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 23))) __attribute__ ((noinline)) void maccess_23_1(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 23))) __attribute__ ((noinline)) void maccess_23_2(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 24))) __attribute__ ((noinline)) void maccess_24_1(void* p) {
	maccess(p);
}

__attribute__((aligned(1 << 24))) __attribute__ ((noinline)) void maccess_24_2(void* p) {
	maccess(p);
}

// non inline maccess for creating multiple entries in SMS prefetcher
__attribute__ ((noinline)) void maccess_1(void* p) {
	maccess(p);
}
__attribute__ ((noinline)) void maccess_2(void* p) {
	maccess(p);
}
__attribute__ ((noinline)) void maccess_3(void* p) {
	maccess(p);
}
__attribute__ ((noinline)) void maccess_4(void* p) {
	maccess(p);
}
__attribute__ ((noinline)) void maccess_5(void* p) {
	maccess(p);
}
__attribute__ ((noinline)) void maccess_6(void* p) {
	maccess(p);
}
__attribute__ ((noinline)) void maccess_7(void* p) {
	maccess(p);
}
__attribute__ ((noinline)) void maccess_8(void* p) {
	maccess(p);
}
__attribute__ ((noinline)) void maccess_9(void* p) {
	maccess(p);
}
__attribute__ ((noinline)) void maccess_10(void* p) {
	maccess(p);
}
__attribute__ ((noinline)) void maccess_11(void* p) {
	maccess(p);
}
__attribute__ ((noinline)) void maccess_12(void* p) {
	maccess(p);
}
__attribute__ ((noinline)) void maccess_13(void* p) {
	maccess(p);
}
__attribute__ ((noinline)) void maccess_14(void* p) {
	maccess(p);
}
__attribute__ ((noinline)) void maccess_15(void* p) {
	maccess(p);
}
__attribute__ ((noinline)) void maccess_16(void* p) {
	maccess(p);
}
__attribute__ ((noinline)) void maccess_17(void* p) {
	maccess(p);
}
__attribute__ ((noinline)) void maccess_18(void* p) {
	maccess(p);
}
__attribute__ ((noinline)) void maccess_19(void* p) {
	maccess(p);
}
__attribute__ ((noinline)) void maccess_20(void* p) {
	maccess(p);
}

