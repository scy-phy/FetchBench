#pragma once

#include <random>
#include <cassert>
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sched.h>

#ifndef PAGE_SIZE
	#define PAGE_SIZE (4096)
#endif
#ifndef CACHE_LINE_WIDTH
	#define CACHE_LINE_WIDTH (64)
#endif

typedef struct {
	uint8_t* base_addr;
	size_t size;
} Mapping;

typedef struct Result {
	size_t index;
	uint64_t time;
} Result;

#if defined(__ARM_ARCH ) && __ARM_ARCH == 8
	#define ARCH_ARMV8

	static inline __attribute__((always_inline)) void timing_init() {
		uint32_t value = 0;

		/* Enable Performance Counter */
		asm volatile("MRS %0, PMCR_EL0" : "=r" (value));
		value |= (1 << 0); /* Enable */
		value |= (1 << 1); /* Cycle counter reset */
		value |= (1 << 2); /* Reset all counters */
		asm volatile("MSR PMCR_EL0, %0" : : "r" (value));

		/* Enable cycle counter register */
		asm volatile("MRS %0, PMCNTENSET_EL0" : "=r" (value));
		value |= (1 << 31);
		asm volatile("MSR PMCNTENSET_EL0, %0" : : "r" (value));
	}

	// the following inline functions are based on
	// https://github.com/IAIK/ZombieLoad/blob/master/attacker/variant1_linux/cacheutils.h
	//   and
	// https://github.com/IAIK/armageddon/blob/master/libflush/libflush/armv8/
	static inline __attribute__((always_inline)) void maccess(void *p) {
		volatile uint32_t value;
		asm volatile ("LDR %w0, [%1]\n\t"
			: "=r" (value)
			: "r" (p)
		);
	}
	static inline __attribute__((always_inline)) void flush(void* ptr) {
		asm volatile ("DC CIVAC, %0" :: "r"(ptr));
		asm volatile ("DSB ISH");
		asm volatile ("ISB");
	}
	static inline __attribute__((always_inline)) void mfence() {
		asm volatile ("DSB SY");
		asm volatile ("ISB");
	}
	static inline __attribute__((always_inline)) uint64_t rdtsc() {
		uint64_t result = 0;
		asm volatile("MRS %0, PMCCNTR_EL0" : "=r" (result));
		return result;
	}

#elif defined(__x86_64__)
	#define ARCH_INTEL
	
	static inline __attribute__((always_inline)) void timing_init() { }

	// the following inline functions are based on
	// https://github.com/IAIK/ZombieLoad/blob/master/attacker/variant1_linux/cacheutils.h
	static inline __attribute__((always_inline)) void maccess(void *p) {
		asm volatile("movq (%0), %%rax\n" : : "c"(p) : "rax");
	}
	static inline __attribute__((always_inline)) void flush(void* ptr) {
		asm volatile("clflush 0(%0)\n" : : "c"(ptr) : "rax");
	}
	static inline __attribute__((always_inline)) void mfence() {
		asm volatile("mfence");
	}
	static inline __attribute__((always_inline)) uint64_t rdtsc() {
		uint64_t a, d;
		  asm volatile("mfence");
		  asm volatile("rdtscp" : "=a"(a), "=d"(d) :: "rcx");
		  a = (d << 32) | a;
		  asm volatile("mfence");
		  return a;
	}
#endif

static inline __attribute__((always_inline)) int reload_t(void *ptr) {
	uint64_t start = 0, end = 0;

	mfence();
	start = rdtsc();
	mfence();
	maccess(ptr);
	mfence();
	end = rdtsc();
	mfence();

	return (int)(end - start);
}

static inline __attribute__((always_inline)) int reload_flush_t(void *ptr) {
	uint64_t start = 0, end = 0;

	mfence();
	start = rdtsc();
	mfence();
	maccess(ptr);
	mfence();
	end = rdtsc();
	mfence();
	flush(ptr);
	mfence();

	return (int)(end - start);
}

static inline __attribute__((always_inline)) int flush_t(void *ptr) {
	uint64_t start = 0, end = 0;

	mfence();
	start = rdtsc();
	mfence();
	flush(ptr);
	mfence();
	end = rdtsc();
	mfence();

	return (int)(end - start);
}


void timing_init();
cpu_set_t build_cpuset(int cpu);
int move_process_to_cpu(pid_t pid, int cpu);
int get_current_cpu_core();
Mapping allocate_mapping(size_t mem_size);
void flush_mapping(Mapping const& mapping);