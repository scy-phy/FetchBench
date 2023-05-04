#pragma once

#include <random>
#include <cassert>
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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

	void timing_init() {
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
	
	void timing_init() { }

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

enum { NS_PER_SECOND = 1000000000 };
void time_diff(struct timespec t1, struct timespec t2, struct timespec *td)
{
    td->tv_nsec = t2.tv_nsec - t1.tv_nsec;
    td->tv_sec  = t2.tv_sec - t1.tv_sec;
    if (td->tv_sec > 0 && td->tv_nsec < 0)
    {
        td->tv_nsec += NS_PER_SECOND;
        td->tv_sec--;
    }
    else if (td->tv_sec < 0 && td->tv_nsec > 0)
    {
        td->tv_nsec -= NS_PER_SECOND;
        td->tv_sec++;
    }
}

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

// Helper function that returns a cpu_set_t with a cpu affinity mask
// that limits execution to the single (logical) CPU core cpu.
cpu_set_t build_cpuset(int cpu) {
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(cpu, &cpuset);
	return cpuset;
}

// Set affinity mask of the given process so that the process is executed
// on a specific (logical) core.
int move_process_to_cpu(pid_t pid, int cpu) {
	cpu_set_t cpuset = build_cpuset(cpu);
	return sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset);
}

std::mt19937* get_rng() {
	static std::mt19937* rng = nullptr;
	if (!rng) {
		// DEBUG: set seed to constant value to make execution runs reproducible.
		size_t seed = 0;
		rng = new std::mt19937(seed);
	}
	return rng;
}

std::mt19937::result_type random_uint32(std::mt19937::result_type lower, std::mt19937::result_type upper) {
	std::uniform_int_distribution<std::mt19937::result_type> dist {lower, upper};
	return dist(*get_rng());
}

// https://github.com/IAIK/rowhammerjs/blob/master/native/rowhammer.cc
// Extract the physical page number from a Linux /proc/PID/pagemap entry.
uint64_t frame_number_from_pagemap(uint64_t value) {
	return value & ((1ULL << 54) - 1);
}

// https://github.com/IAIK/rowhammerjs/blob/master/native/rowhammer.cc
// find the physical address for a virtual/logical address
uint64_t get_physical_addr(int pagemap, uint64_t virtual_addr) {
	uint64_t value;
	off_t offset = (virtual_addr / 4096) * sizeof(value);
	int got = pread(pagemap, &value, sizeof(value), offset);
	assert(got == 8);

	// Check the "page present" flag.
	assert(value & (1ULL << 63));

	uint64_t frame_num = frame_number_from_pagemap(value);
	return (frame_num * 4096) | (virtual_addr & (4095));
}

// https://github.com/IAIK/rowhammerjs/blob/master/native/rowhammer.cc
// create a file descriptor for the pagemap of the process
int open_pagemap() {
	int pagemap = open("/proc/self/pagemap", O_RDONLY);
	assert(pagemap >= 0);
	return pagemap;
}

// query the number of free pages in the system
static inline __attribute__((always_inline)) long no_free_pages() {
	return sysconf(_SC_AVPHYS_PAGES);
}
