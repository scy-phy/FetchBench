#include "cacheutils.hh"

#ifdef COUNTER_THREAD
	void clock_init(int ctr_cpu) {
		ctr_thread_start(ctr_cpu);
	}
	void clock_teardown() {
		ctr_thread_stop();
	}
#else
	void clock_init(int ctr_cpu) {
		return;
	}
	void clock_teardown() {
		return;
	}
#endif

/**
 * Performs a memory access to the given address. As a side-effect, the
 * cache line containing that address is brought into the cache. This
 * function is NOT inlined, i.e., it can be used in experiments that
 * require the load instruction to be located at a constant PC.
 *
 * @param      addr  The address to load
 */
__attribute__((noinline)) void maccess_noinline(void* addr) {
	maccess(addr);
}