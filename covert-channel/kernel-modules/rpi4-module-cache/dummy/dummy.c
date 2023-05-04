#include <unistd.h>
#include <stdio.h>
#include <inttypes.h>

uint64_t id = 0x3333333300000000;

static inline __attribute__((always_inline)) void maccess(void *p) {
	volatile uint32_t value;
	asm volatile ("LDR %w0, [%1]\n\t"
		: "=r" (value)
		: "r" (p)
	);
}

int main() {
	uint64_t __attribute__((aligned(64))) data[1024];
	for (size_t i = 0; i < 1024; i += 8) {
		data[i] = id | i;
		printf("value: %.16lx\n", data[i]);
	}
	for (size_t i = 0; ; i += 8) {
		maccess(&(data[i % 1024]));
		usleep(1);
	}
}
