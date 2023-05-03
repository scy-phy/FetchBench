#pragma once

#include <cassert>
#include <limits>
#include <vector>
#include <unistd.h>
#include <utility>
#include <algorithm>
#include <map>

#include "json11.hpp"

#include "testcase.hh"
#include "cacheutils.hh"
#include "logger.hh"
#include "utils.hh"
#include "mapping.hh"

#include "testcase_dcreplay_dcexperiment.hh"

using json11::Json;
using std::vector;
using std::pair;
using std::map;

class TestCaseDCReplay : public TestCaseBase {
private:
	size_t const fr_thresh;
	size_t const noise_thresh;
	bool use_nanosleep = false;

public:
	TestCaseDCReplay(size_t fr_thresh, size_t noise_thresh, bool use_nanosleep)
	: fr_thresh {fr_thresh}
	, noise_thresh {noise_thresh}
	, use_nanosleep {use_nanosleep}
	{}

	virtual string id() override {
		return "dcreplay";
	}

protected:
	virtual Json pre_test() override {
		architecture_t arch = get_arch();
		if (arch == ARCH_INTEL) {
			set_intel_prefetcher(-1, INTEL_L2_HW_PREFETCHER, false);
			set_intel_prefetcher(-1, INTEL_L2_ADJACENT_CL_PREFETCHER, false);
			set_intel_prefetcher(-1, INTEL_DCU_PREFETCHER, false);
			set_intel_prefetcher(-1, INTEL_DCU_IP_PREFETCHER, false);
		} else if (arch == ARCH_ARM) {
		}
		return Json::object {
			{"architecture", arch},
		};
	}

	virtual Json post_test() override {
		if (get_arch() == ARCH_INTEL) {
			set_intel_prefetcher(-1, INTEL_L2_HW_PREFETCHER, true);
			set_intel_prefetcher(-1, INTEL_L2_ADJACENT_CL_PREFETCHER, true);
			set_intel_prefetcher(-1, INTEL_DCU_PREFETCHER, true);
			set_intel_prefetcher(-1, INTEL_DCU_IP_PREFETCHER, true);
		}
		return Json::object {};
	}

	// ========== TESTS ==============


	/**
	 * Basic test for Delta Correlating prediction tables prefetcher
	 *
	 * @param[in]  no_repetitions  Number of repetitions
	 *
	 * @return     JSON structure describing the result.
	 */
	Json test_base_test(size_t no_repetitions) {
		L::info("Test: %s\n", __FUNCTION__);
		Mapping mapping = allocate_mapping(4 * PAGE_SIZE);
		flush_mapping(mapping);

		Mapping mapping1 { mapping.base_addr, 2 * PAGE_SIZE };
		Mapping mapping2 { mapping.base_addr + 2 * PAGE_SIZE, 2 * PAGE_SIZE };

		// Train with sequence of 8 loads
		vector<size_t> training_offsets = {1 * CACHE_LINE_SIZE, 5 * CACHE_LINE_SIZE, 15 * CACHE_LINE_SIZE, 23 * CACHE_LINE_SIZE, 31 * CACHE_LINE_SIZE, 
				32 * CACHE_LINE_SIZE, 44* CACHE_LINE_SIZE, 50 * CACHE_LINE_SIZE};
		// Trigger with sequence of 5 loads
		vector<size_t> trigger_offsets { training_offsets[0], training_offsets[1], training_offsets[2], training_offsets[3], training_offsets[4]};
		DCReplayExperiment experiment { training_offsets, trigger_offsets, use_nanosleep, fr_thresh, noise_thresh };

		// run experiments
		vector<size_t> cache_histogram = experiment.collect_cache_histogram(mapping1, mapping2, no_repetitions, workload_dcreplay_same_pc_different_memory, nullptr);
		vector<bool> prefetch_vector = experiment.evaluate_cache_histogram(cache_histogram, no_repetitions);

		// Dump cache histogram
		string dump_filename = "trace-dcreplay_base.json";
		experiment.dump(cache_histogram, prefetch_vector, dump_filename);

		// Count prefetches
		size_t prefetch_count = std::count(prefetch_vector.begin(), prefetch_vector.end(), true);
		printf("loaded:%zu prefetched: %zu\n", trigger_offsets.size(), prefetch_count);
		unmap_mapping(mapping);

		return Json::object {
			{"status", "completed"},
			{"replay_existence", (prefetch_count > 0)},
			{"Prefetched", (int)(prefetch_count)},
		};
	}

	virtual Json identify() override {
		// Gem5 is slow and more deterministic -- reduce the number of
		// repetitions here to get faster results (e.g. to 400).
		size_t no_repetitions = 40000;
		
		Json test_results = test_base_test(no_repetitions);
		bool identified = (test_results["replay_existence"].bool_value());

		return Json::object {
			{ "identified", identified },
			{ "test_base_test", test_results },
		};
	}

	virtual Json characterize() override {
		return {};
	}
};

