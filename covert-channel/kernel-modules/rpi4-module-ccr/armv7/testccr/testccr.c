#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>

static inline uint32_t readctr() {
	uint32_t pmccntr;
	// Read cycle counter (PMCCNTR)
	asm volatile("MRC p15, 0, %0, c9, c13, 0" : "=r" (pmccntr));
	return pmccntr;
}

int main() {
	// Enable cycle counter
	// Set bit  0 to 1 in PMCR
	asm volatile("MCR p15, 0, %0, c9, c12, 0" :: "r" (1));
	// Set bit 31 to 1 in PMCNTENSET
	asm volatile("MCR p15, 0, %0, c9, c12, 1" :: "r" (1<<31));
	
	uint32_t before = readctr();
	int i;
	for (i = 0; i < 1000; i++);
	uint32_t after = readctr();

	printf("The for loop execution took %" PRIu32 " ticks.\n", after - before);
	printf("Before: %" PRIu32 ", after: %" PRIu32 ".\n", before, after);
	return 0;
}
