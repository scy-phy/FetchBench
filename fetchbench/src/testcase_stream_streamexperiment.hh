#pragma once

#include <cinttypes>
#include <ctime>
#include <fstream>
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

// Array of maccess used for creating entries in stream prefetcher
static void (*maccess_stream_array [20]) (void *p) = { maccess_1,
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

class StreamExperiment {
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
	StreamExperiment(vector<size_t> training_offsets, vector<size_t> trigger_offsets, bool use_nanosleep, size_t fr_thresh, size_t noise_thresh);
	bool offset_accessed(size_t offset) const;
	bool offset_potential_prefetch(size_t offset) const;
	bool cl_accessed(size_t cl_idx) const;
	bool cl_potential_prefetch(size_t cl_idx) const;

private:
	inline void probe_single(vector<size_t>& cache_histogram, size_t idx, uint8_t* ptr) const __attribute__((always_inline)) {
		assert(idx < cache_histogram.size());
		size_t time = flush_reload_t(ptr);
		cache_histogram[idx] += (time < fr_thresh) ? 1 : 0;
	}

public:
	vector<size_t> collect_cache_histogram(Mapping const& mapping, size_t no_repetitions, void (*workload)(StreamExperiment const&, Mapping const&, void*), void* additional_info);
	vector<size_t> collect_cache_histogram(Mapping const& mapping1, Mapping const& mapping2, size_t no_repetitions, void (*workload)(StreamExperiment const&, Mapping const&, Mapping const&, void*), void* additional_info);
	vector<bool> evaluate_cache_histogram(vector<size_t> const& cache_histogram, size_t no_repetitions, double threshold_multiplier) const;
	vector<bool> evaluate_cache_histogram(vector<size_t> const& cache_histogram, size_t no_repetitions) const;
	void dump(vector<size_t> const& cache_histogram, vector<bool> prefetch_vector, string const& filepath) const;
	static pair<StreamExperiment, vector<size_t>> restore(string const& filepath);
};

// ===== WORKLOADS =====

/**
 * Basic stream prefetcher workload. Uses different PCs to load from the
 * offsets specified in .training_offsets.
 *
 * @param      experiment       The experiment
 * @param      mapping1         The mapping to work in
 * @param      additional_info  The additional information (must be
 *                              nullptr)
 */
__attribute__((always_inline)) inline void workload_stream_basic(StreamExperiment const& experiment, Mapping const& mapping1, void* additional_info) {
	assert(additional_info == nullptr);
	int i = 0;
	int max = sizeof(maccess_stream_array)/sizeof(maccess_stream_array[0]);

	// every load is unique to ensure different PC
	for (size_t offset : experiment.training_offsets) {
		maccess_stream_array[i % max](mapping1.base_addr + offset);
		i++;
	}
	mfence();
}

