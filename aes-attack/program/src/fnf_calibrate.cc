#include <stdio.h>
#include <vector>
#include <algorithm>

#include "common_test.hh"

using std::vector;

#define NO_ITERATIONS 100000

int main() {
	move_process_to_cpu(0, 3);
	timing_init();

	Mapping m = allocate_mapping(PAGE_SIZE);
	uint8_t* addr = m.base_addr + 1024;

	flush_mapping(m);

	mfence();

	vector<size_t> values_miss;
	values_miss.reserve(NO_ITERATIONS);
	for (size_t i = 0; i < NO_ITERATIONS; i++) {
		size_t measurement = flush_t(addr);
		values_miss.push_back(measurement);
	}

	vector<size_t> values_hit;
	values_hit.reserve(NO_ITERATIONS);
	for (size_t i = 0; i < NO_ITERATIONS; i++) {
		maccess(addr);
		mfence();
		size_t measurement = flush_t(addr);
		values_hit.push_back(measurement);
	}

	std::sort(values_hit.begin(), values_hit.end());
	std::sort(values_miss.begin(), values_miss.end());

	printf(
		"HIT:  0%%: %4zu, 25%%: %4zu, 50%%: %4zu, 75%%: %4zu, 100%%: %6zu\n",
		values_hit[0],
		values_hit[NO_ITERATIONS * 0.25],
		values_hit[NO_ITERATIONS * 0.5],
		values_hit[NO_ITERATIONS * 0.75],
		values_hit[NO_ITERATIONS - 1]
	);

	printf(
		"MISS: 0%%: %4zu, 25%%: %4zu, 50%%: %4zu, 75%%: %4zu, 100%%: %6zu\n",
		values_miss[0],
		values_miss[NO_ITERATIONS * 0.25],
		values_miss[NO_ITERATIONS * 0.5],
		values_miss[NO_ITERATIONS * 0.75],
		values_miss[NO_ITERATIONS - 1]
	);


	return 0;
}