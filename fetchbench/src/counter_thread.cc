#include "counter_thread.hh"
#include "logger.hh"

size_t volatile ctr_thread_ctr;
bool volatile ctr_thread_running = false;
std::thread ctr_thread;

void ctr_thread_worker(int cpu) {
	pin_process_to_cpu(0, cpu);

	#if defined(__aarch64__)
		// For AARCH64, use an assembly version of the loop that does not
		// re-load the value after each store.
		size_t asm_ctr_value = 0;
		bool asm_runflag_value = ctr_thread_running;
		size_t volatile* asm_ctr_addr = &ctr_thread_ctr;
		bool volatile* asm_runflag_addr = &ctr_thread_running;
		asm volatile(
			// load current counter value (should be 0 initially)
			"	ldr     %[ctr_value], [%[ctr_addr]]\n"
			"ctr_thread_loop:\n"
			// increment & store the counter
			"	add     %[ctr_value], %[ctr_value], #1\n"
			"	str     %[ctr_value], [%[ctr_addr]]\n"
			// load runflag & compare to 0. continue if unequal.
			"	ldr     %[runflag_value], [%[runflag_addr]]\n"
			"	cmp		%[runflag_value], #0\n"
			"	b.ne ctr_thread_loop\n"
			: [ctr_value] "+r" (asm_ctr_value), [runflag_value] "+r" (asm_runflag_value)
			: [ctr_addr] "r" (asm_ctr_addr), [runflag_addr] "r" (asm_runflag_addr)
		);
	#else
		while(ctr_thread_running) {
			++ctr_thread_ctr;
		}
	#endif
}

void ctr_thread_start(int cpu) {
	if ( ! ctr_thread_running) {
		ctr_thread_running = true;
		L::debug("Starting counter thread on CPU %d\n", cpu);
		
		// start the thread
		ctr_thread = std::thread(&ctr_thread_worker, cpu);

		// wait for the thread to start incrementing
		while(ctr_thread_ctr == 0) {
			usleep(100);
		}
	}
}

void ctr_thread_stop() {
	if (ctr_thread_running) {
		ctr_thread_running = false;
		ctr_thread.join();
	}
}