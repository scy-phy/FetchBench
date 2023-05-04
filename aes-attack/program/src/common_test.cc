#include "common_test.hh"


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

int get_current_cpu_core() {
	unsigned int cpu;
	int ret = getcpu(&cpu, NULL);
	if (ret != 0) {
		return -1;
	}
	return cpu;
}

Mapping allocate_mapping(size_t mem_size) {
	uint8_t* m = (uint8_t*) mmap(
		NULL, mem_size, PROT_READ | PROT_WRITE,
		MAP_POPULATE | MAP_PRIVATE | MAP_ANONYMOUS,
		-1, 0
	);
	if (m == MAP_FAILED) {
		printf("mmap failed\n");
		exit(1);
	}
	return Mapping {m, mem_size};
}

void flush_mapping(Mapping const& mapping) {
	for (uint8_t* ptr = mapping.base_addr; ptr < mapping.base_addr + mapping.size; ptr += CACHE_LINE_WIDTH) {
		flush(ptr);
	}
	mfence();
}