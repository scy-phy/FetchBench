#include "cache_query.hh"

Mapping cache_pa;
uint64_t cache_state = 0;

struct timespec const t_req { .tv_sec = 0, .tv_nsec = 1000 /* 1µs */ };
struct timespec t_rem;

void cache_query_setup() {
	cache_pa = allocate_mapping(2 * REGION_SIZE);
	flush_mapping(cache_pa);
	// make sure we can use an uint64_t to store the cache state
	assert(cache_pa.size / CACHE_LINE_WIDTH <= 64);
}

void cache_query() {
	nanosleep(&t_req, &t_rem);
	
	cache_state = 0;
	
	// warmup
	for (size_t i = 0; i < 1000; i++) {
		flush(cache_pa.base_addr);
	}
	
	for (size_t clidx = 16-9; clidx < 16+12; clidx++) {
		// size_t permuted_clidx = permute(no_cls, clidx);
		size_t permuted_clidx = clidx;
		size_t time = flush_t(cache_pa.base_addr + permuted_clidx * CACHE_LINE_WIDTH);
		// size_t time = reload_flush_t(cache_pa.base_addr + permuted_clidx * CACHE_LINE_WIDTH);
		// if (time < FNR_THRESHOLD) {
		if (time > FNF_THRESHOLD) {
			cache_state |= (1ULL << permuted_clidx);
		}
	}

	flush_mapping(cache_pa);
}

void print_hits(uint64_t _cache_state) {
	size_t no_cls = cache_pa.size / CACHE_LINE_WIDTH;
	for (size_t clidx = 0; clidx < no_cls; clidx++) {
		if (_cache_state & (1ULL << clidx)) {
			printf("Hit at CL %zu\n", clidx);
		}
	}
}

void print_hits() {
	print_hits(cache_state);
}

uint64_t get_cache_state() {
	return cache_state;
}
