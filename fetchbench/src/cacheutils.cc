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

__attribute__((noinline)) void maccess_noinline(void* addr) {
	maccess(addr);
}