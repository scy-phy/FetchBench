#pragma once
#include <assert.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdint.h>
#include <signal.h>

#include "counter_thread.hh"

#define ARM_CLOCK_MONOTONIC 0
#define ARM_CLOCK_CTRTHREAD 1
#define ARM_CLOCK_PMCCNTR 2
#define ARM_CLOCK_APPLE_MSR 3

#define INTEL_CLOCK_RDTSCP 0
#define INTEL_CLOCK_CTRTHREAD 1
#define INTEL_CLOCK_MONOTONIC 2

#if defined(__APPLE__) && defined(__aarch64__)
	#define CACHE_LINE_SIZE 128
	#define PAGE_SIZE 16384
	#warning "Using Apple size paramters"
#else
	#define CACHE_LINE_SIZE 64
	#define PAGE_SIZE 4096
#endif

/* ============================================================
 *                    User configuration
 * ============================================================ */

#define USE_RDTSCP              1

#if defined(COUNTER_THREAD)
	#define ARM_CLOCK_SOURCE	ARM_CLOCK_CTRTHREAD
	#define INTEL_CLOCK_SOURCE	INTEL_CLOCK_CTRTHREAD
	#include "counter_thread.hh"
	#warning "Using Counter Thread"
#elif defined(GETTIME)
	#define ARM_CLOCK_SOURCE	ARM_CLOCK_MONOTONIC
	#define INTEL_CLOCK_SOURCE	INTEL_CLOCK_MONOTONIC
	#include <time.h>
	#warning "Using clock_gettime"
#elif (defined(__i386__) || defined(__x86_64__)) && defined(RDTSC)
	#define INTEL_CLOCK_SOURCE	INTEL_CLOCK_RDTSCP
	#warning "Using RDTSC(P)"
#elif defined(__aarch64__)
	#ifdef ARM_MSR
		#define ARM_CLOCK_SOURCE	ARM_CLOCK_PMCCNTR
		#warning "Using ARM MSR"
	#elif APPLE_MSR
		#define ARM_CLOCK_SOURCE	ARM_CLOCK_APPLE_MSR
		#warning "Using APPLE MSR"
	#endif
#else
	#error "Please specify a clock source. Compile with -D<src>, where src" \
		" is one of: COUNTER_THREAD, GETTIME, RDTSC (only x86)," \
		" ARM_MSR (only ARM != M1), APPLE_MSR (only M1))"
#endif

/* ============================================================
 *                  User configuration End
 * ============================================================ */

/**
 * Initializes the timing source, if necessary.
 *
 * @param[in]  ctr_cpu  The CPU core to run any additional workload on,
 *                      e.g., a counter thread.
 */
void clock_init(int ctr_cpu);

/**
 * De-initializes the timing source, if necessary.
 */
void clock_teardown();

// Forward declaration of primitives for documentation

/**
 * Returns a current timestamp from the timing source. Will be inlined.
 *
 * @return     The timestamp
 */
__attribute__((always_inline)) static inline uint64_t rdtsc();

/**
 * Flushes the given address p from the cache. Will be inlined.
 *
 * @param      p     The address to flush
 */
__attribute__((always_inline)) static inline void flush(void *p);

/**
 * Performs a memory access to the given address p. As a side-effect, the
 * cache line containing that address is brought into the cache. This
 * function WILL be inlined, i.e., each call produces a new load
 * instruction with its own PC.
 *
 * @param      p     The address to load
 */
__attribute__((always_inline)) static inline void maccess(void *p);

/**
 * Issues a memory fence instruction. Will be inlined.
 */
__attribute__((always_inline)) static inline void mfence();

#if defined(__i386__) || defined(__x86_64__)
	// ---------------------------------------------------------------------------
	__attribute__((always_inline)) static inline uint64_t rdtsc() {
		#if INTEL_CLOCK_SOURCE == INTEL_CLOCK_RDTSCP
			uint64_t a, d;
			asm volatile("mfence");
			#if USE_RDTSCP
				asm volatile("rdtscp" : "=a"(a), "=d"(d) :: "rcx");
			#else
				asm volatile("rdtsc" : "=a"(a), "=d"(d));
			#endif
			a = (d << 32) | a;
			asm volatile("mfence");
			return a;
		#elif INTEL_CLOCK_SOURCE == INTEL_CLOCK_MONOTONIC
			asm volatile("mfence");
			struct timespec t1;
			clock_gettime(CLOCK_MONOTONIC, &t1);
			uint64_t res = t1.tv_sec * 1000 * 1000 * 1000ULL + t1.tv_nsec;
			asm volatile("mfence");
			return res;
		#elif INTEL_CLOCK_SOURCE == INTEL_CLOCK_CTRTHREAD
			size_t value;
			asm volatile("mfence");
			value = ctr_thread_ctr;
			asm volatile("mfence");
			return value;
		#else
			#error "Unknown clock primitive"
		#endif
	}

	// ---------------------------------------------------------------------------
	__attribute__((always_inline)) static inline void flush(void *p) { asm volatile("clflush 0(%0)\n" : : "c"(p) : "rax"); }

	// ---------------------------------------------------------------------------
	__attribute__((always_inline)) static inline void maccess(void *p) { asm volatile("movq (%0), %%rax\n" : : "c"(p) : "rax"); }

	// ---------------------------------------------------------------------------
	__attribute__((always_inline)) static inline void mfence() { asm volatile("mfence"); }

#elif defined(__aarch64__)

	// ---------------------------------------------------------------------------
	__attribute__((always_inline)) static inline void flush(void *p) {
		asm volatile("DC CIVAC, %0" ::"r"(p));
		asm volatile("DSB ISH");
		asm volatile("ISB");
	}

	// ---------------------------------------------------------------------------
	__attribute__((always_inline)) static inline void maccess(void *p) {
		volatile uint32_t value;
		asm volatile("LDR %0, [%1]\n\t" : "=r"(value) : "r"(p));
		// asm volatile("DSB ISH");
		// asm volatile("ISB");
	}

	// ---------------------------------------------------------------------------
	__attribute__((always_inline)) static inline void mfence() { asm volatile("DSB ISH"); }

	// ---------------------------------------------------------------------------
	__attribute__((always_inline)) static inline uint64_t rdtsc() {
	#if ARM_CLOCK_SOURCE == ARM_CLOCK_MONOTONIC
		asm volatile("DSB SY");
		asm volatile("ISB");
		struct timespec t1;
		clock_gettime(CLOCK_MONOTONIC, &t1);
		uint64_t res = t1.tv_sec * 1000 * 1000 * 1000ULL + t1.tv_nsec;
		asm volatile("ISB");
		asm volatile("DSB SY");
		return res;
	#elif ARM_CLOCK_SOURCE == ARM_CLOCK_CTRTHREAD
		size_t value;
		asm volatile("ISB");
		asm volatile("DSB SY");
		value = ctr_thread_ctr;
		asm volatile("ISB");
		asm volatile("DSB SY");
		return value;
	#elif ARM_CLOCK_SOURCE == ARM_CLOCK_PMCCNTR
		uint64_t result = 0;
		asm volatile("ISB");
		asm volatile("DSB SY");
		asm volatile("MRS %0, PMCCNTR_EL0" : "=r" (result));
		asm volatile("ISB");
		asm volatile("DSB SY");
		return result;
	#elif ARM_CLOCK_SOURCE == ARM_CLOCK_APPLE_MSR
		uint64_t result = 0;
		asm volatile("ISB");
		asm volatile("DSB SY");
		asm volatile("MRS %0, s3_2_c15_c0_00" : "=r" (result));
		asm volatile("ISB");
		asm volatile("DSB SY");
		return result;
	#else
		#error Clock source not supported
	#endif
	}
#endif


/**
 * Combines maccess, mfence, and flush into a Flush+Reload primitive: Loads
 * the given address. Measures the execution time of that access. Flushes
 * the address again right afterwards. Returns the measured load time.
 *
 * @param      ptr   The pointer to load
 *
 * @return     Time required to load the given address. Unit: delta of two
 *             rdtsc()-timestamps.
 */
__attribute__((always_inline)) static inline int flush_reload_t(void *ptr) {
	uint64_t start = 0, end = 0;

	start = rdtsc();
	maccess(ptr);
	end = rdtsc();

	mfence();

	flush(ptr);

	return (int)(end - start);
}
__attribute__((noinline)) void maccess_noinline(void* addr);