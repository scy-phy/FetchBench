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

#include "testcase_stream_streamexperiment.hh"

using json11::Json;
using std::vector;
using std::pair;
using std::map;

class TestCaseStream : public TestCaseBase {
private:
	size_t const fr_thresh;
	size_t const noise_thresh;
	bool use_nanosleep = false;

public:
	TestCaseStream(size_t fr_thresh, size_t noise_thresh, bool use_nanosleep)
	: fr_thresh {fr_thresh}
	, noise_thresh {noise_thresh}
	, use_nanosleep {use_nanosleep}
	{}

	virtual string id() override {
		return "stream";
	}

protected:
	virtual Json pre_test() override {
		architecture_t arch = get_arch();
		if (arch == ARCH_INTEL) {
			set_intel_prefetcher(-1, INTEL_L2_HW_PREFETCHER, true);
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
	 * Basic test for Stream prefetcher
	 *
	 * @param[in]  no_repetitions  Number of repetitions
	 *
	 * @return     JSON structure describing the result.
	 */
	Json test_base_test(size_t no_repetitions) {
		L::info("Test: %s\n", __FUNCTION__);
		Mapping mapping = allocate_mapping(2 * PAGE_SIZE);

		vector<string> dump_filenames;
		size_t train_entries = 20;
		size_t prefetch_count_pos = 0, prefetch_count_neg = 0;
		// Train with sequence of loads
		vector<size_t> training_offsets;
		vector<size_t> trigger_offsets; // unused. serves as hint for plotting
 	 	size_t offset = 10*CACHE_LINE_SIZE;
		size_t pos_size = 0, neg_size = 0;
		for (ssize_t sign : {-1, 1}) {
			size_t first_access = (sign == 1) ? offset : (PAGE_SIZE - offset);
			training_offsets.push_back(first_access);
			ssize_t boundary = (sign == 1) ? PAGE_SIZE : 0;
			for (size_t i = 1; i < train_entries; i++) {
				// pushback entries till the end of page
				// as stream prefetcher dont cross page boundary
				if ((sign * (ssize_t)(training_offsets[i-1] + sign * (i + 1) * CACHE_LINE_SIZE)) >= boundary)
					break;
				training_offsets.push_back(training_offsets[i-1] + sign * (i + 1) * CACHE_LINE_SIZE);

			}

			trigger_offsets.push_back(first_access);
			StreamExperiment experiment { training_offsets, trigger_offsets, use_nanosleep, fr_thresh, noise_thresh };

			random_activity(mapping);
			flush_mapping(mapping);
			// run experiments
			vector<size_t> cache_histogram = experiment.collect_cache_histogram(mapping, no_repetitions, workload_stream_basic, nullptr);
			vector<bool> prefetch_vector = experiment.evaluate_cache_histogram(cache_histogram, no_repetitions);

			// Dump cache histogram
			string test_sign = (sign == 1)? "pos" : "neg";
			string dump_filename = "trace-stream_"+test_sign+".json";
			experiment.dump(cache_histogram, prefetch_vector, dump_filename);
			dump_filenames.push_back(dump_filename);

			// Count consecutive prefetches that are more than 1
			for (size_t i = 0; i < training_offsets.size(); i++) {
				if ((bool)prefetch_vector[(int)(training_offsets[i]/CACHE_LINE_SIZE + sign)] == true && (bool)prefetch_vector[(int)(training_offsets[i]/CACHE_LINE_SIZE + 2 * sign)] == true) {
					if (sign == 1) {
						prefetch_count_pos++;
					} else {
						prefetch_count_neg++;
					}
				}
			}
			if (sign == 1) {
				pos_size = training_offsets.size();
			} else {
				neg_size = training_offsets.size();
			}
			training_offsets.clear();
			trigger_offsets.clear();
		}
		plot_stream(string{__FUNCTION__}, dump_filenames);
		unmap_mapping(mapping);
		printf("prefetch[+1,+2]/load  (+ve): %zu/%zu (-ve): %zu/%zu\n", prefetch_count_pos, pos_size, prefetch_count_neg, neg_size);

		// stream detected when both positive and negative streams have more than 50% two consecutive prefetches
		return Json::object {
			{"status", "completed"},
			{"stream_existence", (prefetch_count_pos > (pos_size/3)) && (prefetch_count_neg > (neg_size/3))},
			{"Prefetched", (int)(prefetch_count_pos)},
		};
	}

	virtual Json identify() override {
		// Low repetition as gem5 is slow and has less noise
		size_t no_repetitions = 40000 * (PAGE_SIZE / 4096);
		
		Json test_results = test_base_test(no_repetitions);
		bool identified = (test_results["stream_existence"].bool_value());

		return Json::object {
			{ "identified", identified },
			{ "test_base_test", test_results },
		};
	}

	virtual Json characterize() override {
		return {};
	}
};

