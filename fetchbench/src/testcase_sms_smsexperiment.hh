#pragma once

#include <cinttypes>
#include <ctime>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <algorithm>

#include "json11.hpp"

#include "utils.hh"
#include "aligned_maccess.hh"
#include "mapping.hh"

using json11::Json;
using std::vector;

typedef enum {SMS_NO_PREFETCH = 0, SMS_ABSOLUTE_PREFETCH = 0b01, SMS_RELATIVE_PREFETCH = 0b10 } sms_prefetch_state_t;

// Array of maccess used for creating entries in SMS prefetcher
static void (*maccess_array [20]) (void *p) = { maccess_1,
						maccess_2,
						maccess_3,
						maccess_4,
						maccess_5,
						maccess_6,
						maccess_7,
						maccess_8,
						maccess_9,
						maccess_10,
						maccess_11,
						maccess_12,
						maccess_13,
						maccess_14,
						maccess_15,
						maccess_16,
						maccess_17,
						maccess_18,
						maccess_19,
						maccess_20, };

class SMSExperiment {
public:
	// offsets to access to train a pattern, from the beginning of the
	// mapping, in bytes
	vector<size_t> const training_offsets;
	// offsets to access to trigger the pattern, from the beginning of the
	// mapping, in bytes
	vector<size_t> const trigger_offsets;
	// wait before probing or not
	bool const use_nanosleep;
	// Flush+Reload threshold
	size_t const fr_thresh;
	// Flush+Reload noise threshold
	size_t const noise_thresh;
	// structs for nanosleep
	struct timespec const t_req;
	struct timespec t_rem;

private:
	// relative distances of later training loads to the first training load
	vector<ssize_t> distances;

public:
	SMSExperiment(vector<size_t> training_offsets, vector<size_t> trigger_offsets, bool use_nanosleep, size_t fr_thresh, size_t noise_thresh);
	bool offset_accessed(size_t offset) const;
	sms_prefetch_state_t offset_potential_prefetch(size_t offset) const;
	bool cl_accessed(size_t cl_idx) const;
	sms_prefetch_state_t cl_potential_prefetch(size_t cl_idx) const;

private:
	inline void probe_single(vector<size_t>& cache_histogram, size_t idx, uint8_t* ptr) const __attribute__((always_inline)) {
		assert(idx < cache_histogram.size());
		size_t time = flush_reload_t(ptr);
		cache_histogram[idx] += (time < fr_thresh) ? 1 : 0;
	}

public:
	vector<size_t> collect_cache_histogram(Mapping const& mapping, size_t no_repetitions, void (*workload)(SMSExperiment const&, Mapping const&, void*), void* additional_info);
	vector<size_t> collect_cache_histogram(Mapping const& mapping1, Mapping const& mapping2, size_t no_repetitions, void (*workload)(SMSExperiment const&, Mapping const&, Mapping const&, void*), void* additional_info);
	vector<bool> evaluate_cache_histogram(vector<size_t> const& cache_histogram, size_t no_repetitions, double threshold_multiplier) const;
	vector<bool> evaluate_cache_histogram(vector<size_t> const& cache_histogram, size_t no_repetitions) const;
	void dump(vector<size_t> const& cache_histogram, vector<bool> prefetch_vector, string const& filepath) const;
	static pair<SMSExperiment, vector<size_t>> restore(string const& filepath);
};

// ===== WORKLOADS =====

/**
 * Trains the SMS prefetcher with a single load instruction in a single
 * memory region (mapping2).
 *
 * @param      experiment       The experiment
 * @param      mapping1         The mapping to use when accessing unrelated
 *                              regions is enabled, see additional_info
 *                              parameter
 * @param      mapping2         The mapping for the main experiment
 * @param      additional_info  The additional information: bool*: Should
 *                              the workload touch 16 unrelated regions
 *                              after training, but before the trigger
 *                              access?
 */
__attribute__((always_inline)) inline void workload_sms_same_pc_same_memory(SMSExperiment const& experiment, Mapping const& mapping1, Mapping const& mapping2, void* additional_info) {
	assert(additional_info != nullptr);
	bool access_regions = *((bool*)additional_info);

	// Training in mapping2
	for (size_t offset : experiment.training_offsets) {
		maccess_noinline(mapping2.base_addr + offset);
	}
	mfence();

	if (access_regions) {
		// Access some regions in between (with different PC) to move the
		// entry from the training table into the PHT
		for (size_t i = 0; i < 16; i++) {
			maccess(mapping1.base_addr + PAGE_SIZE * i);
		}
		mfence();
	}

	// flush mapping
	flush_mapping(mapping2);

	// Trigger in mapping2 (same PC)
	for (size_t offset : experiment.trigger_offsets) {
		maccess_noinline(mapping2.base_addr + offset);
	}
}

/**
 * Trains the SMS prefetcher with a single load instruction in mapping1.
 * Probes with the same load instruction in mapping2.
 *
 * @param      experiment       The experiment
 * @param      mapping1         The first mapping (training)
 * @param      mapping2         The second mapping (trigger)
 * @param      additional_info  The additional information (must be
 *                              nullptr)
 */
__attribute__((always_inline)) inline void workload_sms_same_pc_different_memory(SMSExperiment const& experiment, Mapping const& mapping1, Mapping const& mapping2, void* additional_info) {
	assert(additional_info == nullptr);

	// Training in mapping1
	for (size_t offset : experiment.training_offsets) {
		maccess_noinline(mapping1.base_addr + offset);
	}
	mfence();

	// Trigger in mapping2
	for (size_t offset : experiment.trigger_offsets) {
		maccess_noinline(mapping2.base_addr + offset);
	}
}

/**
 * Trains the SMS prefetcher in a memory region (mapping2) and attempts to
 * trigger it in the same region, but with a different load instruction.
 *
 * @param      experiment       The experiment
 * @param      mapping1         The mapping to use when accessing
 *                              unrelated regions is enabled, see
 *                              additional_info parameter
 * @param      mapping2         The mapping for the main experiment
 * @param      additional_info  The additional information: bool*: Should
 *                              the workload touch 16 unrelated regions
 *                              after training, but before the trigger
 *                              access?
 */
__attribute__((always_inline)) inline void workload_sms_different_pc_same_memory(SMSExperiment const& experiment, Mapping const& mapping1, Mapping const& mapping2, void* additional_info) {
	assert(additional_info != nullptr);
	bool access_regions = *((bool*)additional_info);

	// Training in mapping2
	for (size_t offset : experiment.training_offsets) {
		maccess(mapping2.base_addr + offset);
	}
	mfence();

	if (access_regions) {
		// Access some regions in between (with different PC) to move the
		// entry from the training table into the PHT
		for (size_t i = 0; i < 16; i++) {
			maccess(mapping1.base_addr + PAGE_SIZE * i);
		}
		mfence();
	}

	// flush mapping
	flush_mapping(mapping2);

	// Trigger in mapping2 (different PC)
	for (size_t offset : experiment.trigger_offsets) {
		maccess(mapping2.base_addr + offset);
	}
}

/**
 * Trains the prefetcher using instruction A in mapping1, attempts to trigger using instruction B in mapping2.
 *
 * @param      experiment       The experiment
 * @param      mapping1         The first mapping
 * @param      mapping2         The second mapping
 * @param      additional_info  The additional information (must be nullptr)
 */
__attribute__((always_inline)) inline void workload_sms_different_pc_different_memory(SMSExperiment const& experiment, Mapping const& mapping1, Mapping const& mapping2, void* additional_info) {
	assert(additional_info == nullptr);

	// Training in mapping1
	for (size_t offset : experiment.training_offsets) {
		maccess(mapping1.base_addr + offset);
	}
	mfence();

	// Trigger in mapping2
	for (size_t offset : experiment.trigger_offsets) {
		maccess(mapping2.base_addr + offset);
	}
}


/**
 * Try to estimate the number of entries in a PC-correlating SMS
 * prefetcher. Train using instruction A in mapping1. Then touch a number
 * (-> additional_info) of unrelated regions. Finally, try to trigger the
 * prefetcher with the first pattern again. Note that this can only be an
 * estimation, since we don't know the replacement policy.
 *
 * @param      experiment       The experiment
 * @param      mapping1         The mapping for all training activities,
 *                              for the first and all additional regions
 * @param      mapping2         The mapping for the trigger
 * @param      additional_info  The additional information: size_t*: number
 *                              of additional regions to touch between
 *                              training the first region and attempting to
 *                              re-trigger it
 */
__attribute__((always_inline)) inline void workload_sms_training_entries(SMSExperiment const& experiment, Mapping const& mapping1, Mapping const& mapping2, void* additional_info) {
	assert(additional_info != nullptr);
	size_t *entries = (size_t *)additional_info;
	vector<size_t> random_offsets {
		5 * CACHE_LINE_SIZE, 1 * CACHE_LINE_SIZE, 3 * CACHE_LINE_SIZE, 9 * CACHE_LINE_SIZE
	};

	// Training in mapping1
	for (auto offset = experiment.training_offsets.begin(); offset != std::prev(experiment.training_offsets.end()); ++offset) {
		maccess_noinline(mapping1.base_addr + *offset);
	}
	mfence();

	// Training more entries
	for (size_t i = 0; i < *entries; i++) {
		for (size_t offset : random_offsets) {
			maccess_array[i](mapping1.base_addr + i * PAGE_SIZE + offset);
		}
		mfence();
	}
	// append last element to the training set of mapping1
	maccess_noinline(mapping1.base_addr + experiment.training_offsets.back());
	mfence();


	// Trigger in mapping2
	for (size_t offset : experiment.trigger_offsets) {
		maccess_noinline(mapping2.base_addr + offset);
	}
}

/**
 * Performs training accesses with PC1 in memory area mapping1 and trigger
 * accesses with PC2 in memory area mapping2. PC1 and PC2 have colliding
 * LSBs. The number of colliding bits is specified in additional_info.
 *
 * @param      experiment       The experiment
 * @param      mapping1         The mapping 1
 * @param      mapping2         The mapping 2
 * @param      additional_info  The additional information (expects pointer
 *                              to size_t, containing the number of
 *                              colliding bits)
 */
__attribute__((always_inline)) inline void workload_sms_pc_collision(SMSExperiment const& experiment, Mapping const& mapping1, Mapping const& mapping2, void* additional_info)  {
	assert(additional_info != nullptr);
	
	size_t colliding_bits = *((size_t*)additional_info);
	
	// get pointers to co-aligned maccess functions
	pair<maccess_func_t, maccess_func_t> maccess_funcs = get_maccess_functions(colliding_bits);
	maccess_func_t const& maccess_train = maccess_funcs.first;
	maccess_func_t const& maccess_probe = maccess_funcs.second;
	
	// Training in mapping1
	for (size_t offset : experiment.training_offsets) {
		maccess_train(mapping1.base_addr + offset);
	}
	mfence();

	// Trigger in mapping2
	for (size_t offset : experiment.trigger_offsets) {
		maccess_probe(mapping2.base_addr + offset);
	}
}
