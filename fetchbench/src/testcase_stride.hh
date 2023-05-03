#pragma once

#include <cassert>
#include <limits>
#include <vector>
#include <unistd.h>
#include <utility>
#include <algorithm>
#include <map>
#include <sys/mman.h>

#include "json11.hpp"

#include "testcase.hh"
#include "cacheutils.hh"
#include "logger.hh"
#include "utils.hh"
#include "mapping.hh"

#include "testcase_stride_strideexperiment.hh"

using json11::Json;
using std::vector;
using std::pair;
using std::map;

class TestCaseStride : public TestCaseBase {
private:
	size_t const fr_thresh;
	size_t const noise_thresh;
	bool use_nanosleep = false;

public:
	TestCaseStride(size_t fr_thresh, size_t noise_thresh, bool use_nanosleep)
	: fr_thresh {fr_thresh}
	, noise_thresh {noise_thresh}
	, use_nanosleep {use_nanosleep}
	{}

	virtual string id() override {
		return "stride";
	}

protected:
	virtual Json pre_test() override {
		architecture_t arch = get_arch();
		if (arch == ARCH_INTEL) {
			set_intel_prefetcher(-1, INTEL_L2_HW_PREFETCHER, false);
			set_intel_prefetcher(-1, INTEL_L2_ADJACENT_CL_PREFETCHER, false);
			set_intel_prefetcher(-1, INTEL_DCU_PREFETCHER, false);
			set_intel_prefetcher(-1, INTEL_DCU_IP_PREFETCHER, true);
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

	vector<double> compute_diff_factors(vector<bool> const& prefetch_vector_a, vector<bool> const& prefetch_vector_b, StrideExperiment const& experiment_b) {
		// ensure both prefetch vectors have same length
		assert(prefetch_vector_a.size() == prefetch_vector_b.size());

		vector<double> diff_factors;
		for (size_t cl_idx = 0; cl_idx < prefetch_vector_a.size(); cl_idx++) {
			if (prefetch_vector_a[cl_idx] == false && prefetch_vector_b[cl_idx] == true) {
				double delta_multiple_of_stride = ((ssize_t)(cl_idx * CACHE_LINE_SIZE - experiment_b.offset_last_access())) / experiment_b.stride;
				diff_factors.push_back(delta_multiple_of_stride);
			}
		}
		return diff_factors;
	}

	// ########################## actual tests #######################################################

	/**
	 * Tests whether the prefetcher maps two differrent memory areas to the
	 * same prefetch data structure entry or not.
	 *
	 * @param[in]  no_repetitions  Number of repetitions
	 *
	 * @return     JSON structure describing the result.
	 */
	Json test_trigger_same_pc_different_memory(size_t no_repetitions) {
		L::info("Test: %s\n", __FUNCTION__);
		Mapping mapping = allocate_mapping((2 + 256 + 2) * PAGE_SIZE);
		Mapping mapping1 { mapping.base_addr, 2 * PAGE_SIZE };
		Mapping mapping2 { mapping.base_addr + (2 + 256) * PAGE_SIZE, 2 * PAGE_SIZE };
		random_activity(mapping1);
		random_activity(mapping2);
		flush_mapping(mapping1);
		flush_mapping(mapping2);

		// base experiment
		ssize_t stride = 3 * CACHE_LINE_SIZE;
		size_t step = 12;
		StrideExperiment experiment_diffmem { stride, step, 0, use_nanosleep, fr_thresh, noise_thresh };
		StrideExperiment experiment_baseline_1acc { stride, 1, (step-1)*stride, use_nanosleep, fr_thresh, noise_thresh };
		StrideExperiment experiment_baseline_2acc { stride, 2, (step-2)*stride, use_nanosleep, fr_thresh, noise_thresh };

		// run experiments
		size_t no_accesses_on_mapping2 = 1;
		// (step-1) loads in mapping1, 1 load in mapping2
		vector<size_t> cache_histogram_diffmem_1acc = experiment_diffmem.collect_cache_histogram(mapping1, mapping2, no_repetitions, workload_stride_same_pc_different_memory, &no_accesses_on_mapping2);
		random_activity(mapping1);
		random_activity(mapping2);
		flush_mapping(mapping1);
		flush_mapping(mapping2);
		no_accesses_on_mapping2 = 2;
		// (step-2) loads in mapping1, 2 loads in mapping2
		vector<size_t> cache_histogram_diffmem_2acc = experiment_diffmem.collect_cache_histogram(mapping1, mapping2, no_repetitions, workload_stride_same_pc_different_memory, &no_accesses_on_mapping2);
		random_activity(mapping1);
		random_activity(mapping2);
		flush_mapping(mapping1);
		flush_mapping(mapping2);
		// baseline: 1 load in mapping1
		vector<size_t> cache_histogram_baseline_1acc = experiment_baseline_1acc.collect_cache_histogram(mapping1, no_repetitions, workload_stride_loop, nullptr);
		random_activity(mapping1);
		random_activity(mapping2);
		flush_mapping(mapping1);
		flush_mapping(mapping2);
		// baseline: 2 loads in mapping1
		vector<size_t> cache_histogram_baseline_2acc = experiment_baseline_2acc.collect_cache_histogram(mapping1, no_repetitions, workload_stride_loop, nullptr);

		// evaluate the recorded trace: count the number of prefetches
		L::debug("- Baseline: 1 access\n");
		vector<bool> prefetch_vector_baseline_1acc = experiment_baseline_1acc.evaluate_cache_histogram(cache_histogram_baseline_1acc, no_repetitions);
		L::debug("- Baseline: 2 accesses\n");
		vector<bool> prefetch_vector_baseline_2acc = experiment_baseline_2acc.evaluate_cache_histogram(cache_histogram_baseline_2acc, no_repetitions);
		L::debug("- Different memory areas: 1 access\n");
		vector<bool> prefetch_vector_diffmem_1acc = experiment_diffmem.evaluate_cache_histogram(cache_histogram_diffmem_1acc, no_repetitions);
		L::debug("- Different memory areas: 2 accesses\n");
		vector<bool> prefetch_vector_diffmem_2acc = experiment_diffmem.evaluate_cache_histogram(cache_histogram_diffmem_2acc, no_repetitions);

		experiment_baseline_1acc.dump(cache_histogram_baseline_1acc, prefetch_vector_baseline_1acc, "trace-stride-test_trigger_same_pc_different_memory-baseline1.json");
		experiment_baseline_2acc.dump(cache_histogram_baseline_2acc, prefetch_vector_baseline_2acc, "trace-stride-test_trigger_same_pc_different_memory-baseline2.json");
		experiment_diffmem.dump(cache_histogram_diffmem_1acc, prefetch_vector_diffmem_1acc, "trace-stride-test_trigger_same_pc_different_memory-diffmem1.json");
		experiment_diffmem.dump(cache_histogram_diffmem_2acc, prefetch_vector_diffmem_2acc, "trace-stride-test_trigger_same_pc_different_memory-diffmem2.json");
		plot_stride(string{__FUNCTION__}, {
			"trace-stride-test_trigger_same_pc_different_memory-baseline1.json",
			"trace-stride-test_trigger_same_pc_different_memory-diffmem1.json",
			"trace-stride-test_trigger_same_pc_different_memory-baseline2.json",
			"trace-stride-test_trigger_same_pc_different_memory-diffmem2.json",
 		});

		size_t prefetch_count_baseline_1acc = std::count(prefetch_vector_baseline_1acc.begin(), prefetch_vector_baseline_1acc.end(), true);
		size_t prefetch_count_baseline_2acc = std::count(prefetch_vector_baseline_2acc.begin(), prefetch_vector_baseline_2acc.end(), true);
		size_t prefetch_count_diffmem_1acc = std::count(prefetch_vector_diffmem_1acc.begin(), prefetch_vector_diffmem_1acc.end(), true);
		size_t prefetch_count_diffmem_2acc = std::count(prefetch_vector_diffmem_2acc.begin(), prefetch_vector_diffmem_2acc.end(), true);

		L::info("Prefetch count baseline 1 access:   %zu\n", prefetch_count_baseline_1acc);
		L::info("Prefetch count baseline 2 accesses: %zu\n", prefetch_count_baseline_2acc);
		L::info("Prefetch count diff. memory areas (1 access):   %zu\n", prefetch_count_diffmem_1acc);
		L::info("Prefetch count diff. memory areas (2 accesses): %zu\n", prefetch_count_diffmem_2acc);

		unmap_mapping(mapping);

		// If we see more prefetching for the "diffmem" case compared to
		// the "baseline" case, the memory address (alone) is not relevant
		// to identify an entry in the prefetcher's data structure.
		return Json::object {
			{"status", "completed"},
			{"triggers_prefetch_after_1_access", (prefetch_count_diffmem_1acc > prefetch_count_baseline_1acc)},
			{"triggers_prefetch_after_2_accesses", (prefetch_count_diffmem_2acc > prefetch_count_baseline_2acc)},
		};
	}

	/**
	 * Tests whether the prefetcher maps different load instructions
	 * (instructions with different PC value) to the same prefetch data
	 * structure entry or not.
	 *
	 * @param[in]  no_repetitions  Number of repetitions
	 *
	 * @return     JSON structure describing the result.
	 */
	Json test_trigger_different_pc_same_memory(size_t no_repetitions) {
		L::info("Test: %s\n", __FUNCTION__);
		Mapping mapping = allocate_mapping(2 * PAGE_SIZE);
		random_activity(mapping);
		flush_mapping(mapping);

		// base experiment
		ssize_t stride = 3 * CACHE_LINE_SIZE;
		size_t step = 12;
		StrideExperiment experiment_base { stride, step, 0, use_nanosleep, fr_thresh, noise_thresh };
		
		// run experiment; perform (step) loads with (step) different PCs
		vector<size_t> cache_histogram_diffpc = experiment_base.collect_cache_histogram(mapping, no_repetitions, workload_stride_different_pc_same_memory, nullptr);
		random_activity(mapping);
		flush_mapping(mapping);
		// run baseline experiment: perform (step) loads with same PC
		vector<size_t> cache_histogram_baseline_base = experiment_base.collect_cache_histogram(mapping, no_repetitions, workload_stride_loop, nullptr);
		
		// Compare traces.
		// If PC is irrelevant, expect baseline_stm1 < baseline_base && diffpc == baseline_base.
		L::debug("- Baseline %zu (same PC)\n", experiment_base.step);
		vector<bool> prefetch_vector_baseline_base = experiment_base.evaluate_cache_histogram(cache_histogram_baseline_base, no_repetitions);
		L::debug("- Different PCs\n");
		vector<bool> prefetch_vector_diffpc = experiment_base.evaluate_cache_histogram(cache_histogram_diffpc, no_repetitions);

		experiment_base.dump(cache_histogram_baseline_base, prefetch_vector_baseline_base, "trace-stride-test_trigger_different_pc_same_memory-baseline-base.json");
		experiment_base.dump(cache_histogram_diffpc, prefetch_vector_diffpc, "trace-stride-test_trigger_different_pc_same_memory-diffpc.json");
		plot_stride(string{__FUNCTION__}, {
			"trace-stride-test_trigger_different_pc_same_memory-baseline-base.json",
 			"trace-stride-test_trigger_different_pc_same_memory-diffpc.json",
 		});

		// Count prefetches in the experiment with different PCs
		size_t prefetch_count_diffpc = std::count(prefetch_vector_diffpc.begin(), prefetch_vector_diffpc.end(), true);

		unmap_mapping(mapping);

		// If a matching PC is relevant, we would expect no prefetching at
		// all in the "diffpc" case.
		return Json::object {
			{"status", "completed"},
			{"triggers_prefetch", (prefetch_count_diffpc != 0)},
		};
	}

	/**
	 * Tests whether the prefetcher maps accesses in two differrent memory
	 * areas, performed from two different load instructions, to the same
	 * prefetch data structure entry or not.
	 *
	 * @param[in]  no_repetitions  Number of repetitions
	 *
	 * @return     JSON structure describing the result.
	 */
	Json test_trigger_different_pc_different_memory(size_t no_repetitions) {
		L::info("Test: %s\n", __FUNCTION__);
		Mapping mapping = allocate_mapping((2 + 256 + 2) * PAGE_SIZE);
		Mapping mapping1 { mapping.base_addr, 2 * PAGE_SIZE };
		Mapping mapping2 { mapping.base_addr + (2 + 256) * PAGE_SIZE, 2 * PAGE_SIZE };
		random_activity(mapping1);
		random_activity(mapping2);
		flush_mapping(mapping1);
		flush_mapping(mapping2);

		// base experiment
		ssize_t stride = 3 * CACHE_LINE_SIZE;
		size_t step = 12;
		StrideExperiment experiment_diff { stride, step, 0, use_nanosleep, fr_thresh, noise_thresh };
		StrideExperiment experiment_baseline_1acc { stride, 1, (step-1)*stride, use_nanosleep, fr_thresh, noise_thresh };
		StrideExperiment experiment_baseline_2acc { stride, 2, (step-2)*stride, use_nanosleep, fr_thresh, noise_thresh };

		// run experiments
		size_t no_accesses_on_mapping2 = 1;
		// (step-1) loads in mapping1, 1 load in mapping2
		vector<size_t> cache_histogram_diff_1acc = experiment_diff.collect_cache_histogram(mapping1, mapping2, no_repetitions, workload_stride_different_pc_different_memory, &no_accesses_on_mapping2);
		random_activity(mapping1);
		random_activity(mapping2);
		flush_mapping(mapping1);
		flush_mapping(mapping2);
		no_accesses_on_mapping2 = 2;
		// (step-2) loads in mapping1, 2 loads in mapping2
		vector<size_t> cache_histogram_diff_2acc = experiment_diff.collect_cache_histogram(mapping1, mapping2, no_repetitions, workload_stride_different_pc_different_memory, &no_accesses_on_mapping2);
		random_activity(mapping1);
		random_activity(mapping2);
		flush_mapping(mapping1);
		flush_mapping(mapping2);
		// baseline: 1 load in mapping1
		vector<size_t> cache_histogram_baseline_1acc = experiment_baseline_1acc.collect_cache_histogram(mapping1, no_repetitions, workload_stride_loop, nullptr);
		random_activity(mapping1);
		random_activity(mapping2);
		flush_mapping(mapping1);
		flush_mapping(mapping2);
		// baseline: 2 loads in mapping1
		vector<size_t> cache_histogram_baseline_2acc = experiment_baseline_2acc.collect_cache_histogram(mapping1, no_repetitions, workload_stride_loop, nullptr);

		// evaluate the recorded trace: count the number of prefetches
		L::debug("- Baseline: 1 access\n");
		vector<bool> prefetch_vector_baseline_1acc = experiment_baseline_1acc.evaluate_cache_histogram(cache_histogram_baseline_1acc, no_repetitions);
		L::debug("- Baseline: 2 accesses\n");
		vector<bool> prefetch_vector_baseline_2acc = experiment_baseline_2acc.evaluate_cache_histogram(cache_histogram_baseline_2acc, no_repetitions);
		L::debug("- Different memory areas: 1 access\n");
		vector<bool> prefetch_vector_diff_1acc = experiment_diff.evaluate_cache_histogram(cache_histogram_diff_1acc, no_repetitions);
		L::debug("- Different memory areas: 2 accesses\n");
		vector<bool> prefetch_vector_diff_2acc = experiment_diff.evaluate_cache_histogram(cache_histogram_diff_2acc, no_repetitions);

		experiment_baseline_1acc.dump(cache_histogram_baseline_1acc, prefetch_vector_baseline_1acc, "trace-stride-test_trigger_different_pc_different_memory-baseline1.json");
		experiment_baseline_2acc.dump(cache_histogram_baseline_2acc, prefetch_vector_baseline_2acc, "trace-stride-test_trigger_different_pc_different_memory-baseline2.json");
		experiment_diff.dump(cache_histogram_diff_1acc, prefetch_vector_diff_1acc, "trace-stride-test_trigger_different_pc_different_memory-diffmem1.json");
		experiment_diff.dump(cache_histogram_diff_2acc, prefetch_vector_diff_2acc, "trace-stride-test_trigger_different_pc_different_memory-diffmem2.json");
		plot_stride(string{__FUNCTION__}, {
			"trace-stride-test_trigger_different_pc_different_memory-baseline1.json",
			"trace-stride-test_trigger_different_pc_different_memory-diffmem1.json",
			"trace-stride-test_trigger_different_pc_different_memory-baseline2.json",
			"trace-stride-test_trigger_different_pc_different_memory-diffmem2.json",
 		});

		size_t prefetch_count_baseline_1acc = std::count(prefetch_vector_baseline_1acc.begin(), prefetch_vector_baseline_1acc.end(), true);
		size_t prefetch_count_baseline_2acc = std::count(prefetch_vector_baseline_2acc.begin(), prefetch_vector_baseline_2acc.end(), true);
		size_t prefetch_count_diff_1acc = std::count(prefetch_vector_diff_1acc.begin(), prefetch_vector_diff_1acc.end(), true);
		size_t prefetch_count_diff_2acc = std::count(prefetch_vector_diff_2acc.begin(), prefetch_vector_diff_2acc.end(), true);

		L::info("Prefetch count baseline 1 access:   %zu\n", prefetch_count_baseline_1acc);
		L::info("Prefetch count baseline 2 accesses: %zu\n", prefetch_count_baseline_2acc);
		L::info("Prefetch count diff. memory areas (1 access):   %zu\n", prefetch_count_diff_1acc);
		L::info("Prefetch count diff. memory areas (2 accesses): %zu\n", prefetch_count_diff_2acc);

		unmap_mapping(mapping);

		// If we see more prefetching for the "diff" case compared to
		// the "single access" case, the memory address and PC are not
		// relevant to identify an entry in the prefetcher's data
		// structure.
		return Json::object {
			{"status", "completed"},
			{"triggers_prefetch_after_1_access", (prefetch_count_diff_1acc > prefetch_count_baseline_1acc)},
			{"triggers_prefetch_after_2_accesses", (prefetch_count_diff_2acc > prefetch_count_baseline_2acc)},
		};
	}

	/**
	 * "Test" to generate data that can be plotted in a nice overview
	 * heatmap.
	 *
	 * @param[in]  no_repetitions  No repetitions
	 *
	 * @return     { description_of_the_return_value }
	 */
	Json test_overview(size_t no_repetitions) {
		L::info("Test: %s\n", __FUNCTION__);
		Mapping mapping = allocate_mapping(8 * PAGE_SIZE);
		random_activity(mapping);
		flush_mapping(mapping);

		// map<diff_factor, count>
		map<double, size_t> diff_factor_hist[2];

		vector<string> dump_filenames;

		// try a few different strides (positive and negative) and a few
		// different step sizes to test for prefetching in both directions
		for (ssize_t const sign : {-1, 1}) { // for positive and for negative direction (stride)
			for (ssize_t stride = sign * CACHE_LINE_SIZE; std::abs(stride) <= (PAGE_SIZE / 2); stride *= 2) {
				vector<bool> previous_prefetch_vector(mapping.size / CACHE_LINE_SIZE, false);
				for (size_t step = 1; step <= 14; step++) {
					L::info("stride = %zd, step = %zu\n", stride, step);
					
					// for negative strides, start at the end of the memory area
					size_t first_access_offset = (sign == 1) ? 0 : (mapping.size - CACHE_LINE_SIZE);
					// run the experiment
					StrideExperiment experiment { stride, step, first_access_offset, use_nanosleep, fr_thresh, noise_thresh };
					vector<size_t> cache_histogram = experiment.collect_cache_histogram(mapping, no_repetitions, workload_stride_loop, nullptr);
					// evaluate the trace
					vector<bool> prefetch_vector = experiment.evaluate_cache_histogram(cache_histogram, no_repetitions);
					
					random_activity(mapping);
					flush_mapping(mapping);
				
					// compare the recorded trace to the trace of the previous
					// experiment (= same stride, step-1). Identify the newly
					// prefetched locations (as multiples of the stride relative
					// from the last architectural access) and store a histogram
					// of the most common distances.
					L::info("diff from %zu to %zu: ", step-1, step);
					vector<double> diff_factors = compute_diff_factors(previous_prefetch_vector, prefetch_vector, experiment);
					for (double const diff_factor : diff_factors) {
						L::info("%lf ", diff_factor);
						set_or_increment<double,size_t>((sign == 1) ? diff_factor_hist[1] : diff_factor_hist[0], diff_factor, 1);
					}
					L::info("\n");
					previous_prefetch_vector = prefetch_vector;
					string dump_filename = "trace-stride-test_overview-stride_" + zero_pad(stride, 5) + "-step_" + zero_pad(step, 2) + ".json";
					experiment.dump(cache_histogram, prefetch_vector, dump_filename);
					dump_filenames.push_back(dump_filename);
				}
			}
		}
		for (size_t i = 0; i < 2; i++) {
			L::info("Diff factors (%s): ", (i == 0) ? "negative" : "positive");
			for (pair<double, size_t> diff_factor_pair : diff_factor_hist[i]) {
				double const& diff_factor = diff_factor_pair.first;
				size_t const& count = diff_factor_pair.second;
				L::info("%lf (%zu) ", diff_factor, count);
			}
			puts("");
		}
		plot_stride(string{__FUNCTION__}, dump_filenames);
		unmap_mapping(mapping);

		return Json::object {
			{"status", "completed"},
		};
	}

	/**
	 * Tests whether prefetching in positive and/or negative direction
	 * occurs.
	 *
	 * @param[in]  no_repetitions  Number of repetitions
	 *
	 * @return     JSON structure describing the result.
	 */
	Json test_direction(size_t no_repetitions) {
		L::info("Test: %s\n", __FUNCTION__);
		Mapping mapping = allocate_mapping(2 * PAGE_SIZE);
		random_activity(mapping);
		flush_mapping(mapping);

		// base experiment
		ssize_t stride = 3 * CACHE_LINE_SIZE;
		size_t step = 12;
		StrideExperiment experiment_pos { stride, step, 0, use_nanosleep, fr_thresh, noise_thresh };
		StrideExperiment experiment_neg { -stride, step, mapping.size - CACHE_LINE_SIZE, use_nanosleep, fr_thresh, noise_thresh };

		// run experiments
		vector<size_t> cache_histogram_pos = experiment_pos.collect_cache_histogram(mapping, no_repetitions, workload_stride_loop, nullptr);
		random_activity(mapping);
		flush_mapping(mapping);
		vector<size_t> cache_histogram_neg = experiment_neg.collect_cache_histogram(mapping, no_repetitions, workload_stride_loop, nullptr);

		// evaluate the recorded trace: count the number of prefetches
		L::debug("- Direction: positive\n");
		vector<bool> prefetch_vector_pos = experiment_pos.evaluate_cache_histogram(cache_histogram_pos, no_repetitions);
		L::debug("- Direction: negative\n");
		vector<bool> prefetch_vector_neg = experiment_neg.evaluate_cache_histogram(cache_histogram_neg, no_repetitions);

		experiment_pos.dump(cache_histogram_pos, prefetch_vector_pos, "trace-stride-test_direction-pos.json");
		experiment_neg.dump(cache_histogram_neg, prefetch_vector_neg, "trace-stride-test_direction-neg.json");
		plot_stride(string{__FUNCTION__}, {
			"trace-stride-test_direction-pos.json",
			"trace-stride-test_direction-neg.json",
 		});

		size_t prefetch_count_pos = std::count(prefetch_vector_pos.begin(), prefetch_vector_pos.end(), true);
		size_t prefetch_count_neg = std::count(prefetch_vector_neg.begin(), prefetch_vector_neg.end(), true);

		L::info("Prefetch count positive direction: %zu\n", prefetch_count_pos);
		L::info("Prefetch count negative direction: %zu\n", prefetch_count_neg);

		unmap_mapping(mapping);

		// If we see more prefetching for the "diff" case compared to
		// the "single access" case, the memory address and PC are not
		// relevant to identify an entry in the prefetcher's data
		// structure.
		return Json::object {
			{"status", "completed"},
			{"positive_direction", (prefetch_count_pos > 0)},
			{"negative_direction", (prefetch_count_neg > 0)},
		};
	}

	/**
	 * Test case to identify correlation between the number of loads and
	 * the amount of prefetched data.
	 *
	 * @param      no_repetitions  Number of repetitions
	 *
	 * @return     JSON structure describing the results
	 */
	Json test_load_pref_corr(size_t no_repetitions) {
		L::info("Test: %s\n", __FUNCTION__);
		Mapping mapping = allocate_mapping(2*PAGE_SIZE);
		random_activity(mapping);
		flush_mapping(mapping);

		vector<pair<StrideExperiment, vector<bool>>> results;
		vector<string> dump_filenames;
		ssize_t stride = 3 * CACHE_LINE_SIZE;
		for (size_t step = 1; step <= 20; step++) {
			// insert a progressive pattern 1, 2, 3 ... steps
			StrideExperiment experiment { stride, step, 0, use_nanosleep, fr_thresh, noise_thresh };
			vector<size_t> cache_histogram = experiment.collect_cache_histogram(mapping, no_repetitions, workload_stride_loop, nullptr);
			// probe number of prefetches
			vector<bool> prefetch_vector = experiment.evaluate_cache_histogram(cache_histogram, no_repetitions);
			results.push_back({experiment, prefetch_vector});

			string dump_filename = "trace-stride-test_load_pref_corr-step_" + zero_pad(step, 2) + ".json";
			experiment.dump(cache_histogram, prefetch_vector, dump_filename);
			dump_filenames.push_back(dump_filename);
			
			random_activity(mapping);
			flush_mapping(mapping);
		}
		plot_stride(string{__FUNCTION__}, dump_filenames);

		size_t diff_count_max = std::numeric_limits<size_t>::min();
		size_t diff_count_min = std::numeric_limits<size_t>::max();

		Json::array results_json;
		for (size_t i = 1; i < results.size(); i++) {
			StrideExperiment const& experiment = results[i].first;
			vector<bool> const& prefetch_vector = results[i].second;
			vector<bool> const& previous_prefetch_vector = results[i-1].second;

			vector<double> diffs = compute_diff_factors(previous_prefetch_vector, prefetch_vector, experiment);
			L::debug("step = %zu; diffs: ", experiment.step);
			Json::array diffs_json;
			for (double const& diff : diffs) {
				L::debug("%lf ", diff);
				diffs_json.push_back(diff);
			}
			results_json.push_back(Json::object {
				{ "step", (int)experiment.step },
				{ "diffs", diffs_json },
			});
			L::debug("\n");
			if (diffs.size() > diff_count_max) {
				diff_count_max = diffs.size();
			}
			if (diffs.size() < diff_count_min) {
				diff_count_min = diffs.size();
			}
		}

		unmap_mapping(mapping);
		return Json::object {
			{ "status", "completed" },
			{ "results", results_json },
			{ "count_min", (int)diff_count_min },
			{ "count_max", (int)diff_count_max },
		};
	}
	
	
	/**
	 * Test to determine the minimum/maximum number of prefetches.
	 *
	 * @param[in]  no_repetitions  Number of repetitions
	 *
	 * @return     JSON structure describing the results
	 */
	Json test_no_prefetches(size_t no_repetitions) {
		L::info("Test: %s\n", __FUNCTION__);
		Mapping mapping = allocate_mapping(5 * PAGE_SIZE);
		random_activity(mapping);
		flush_mapping(mapping);

		ssize_t stride = 3 * CACHE_LINE_SIZE;
		
		// map<no_prefetches, count>
		map<size_t, size_t> no_prefetch_hist;
		vector<string> dump_filenames;
		for (size_t step = 2; step <= 48; step++) {
			StrideExperiment experiment { stride, step, 0, use_nanosleep, fr_thresh, noise_thresh };
			vector<size_t> cache_histogram = experiment.collect_cache_histogram(mapping, no_repetitions, workload_stride_loop, nullptr);
			vector<bool> prefetch_vector = experiment.evaluate_cache_histogram(cache_histogram, no_repetitions);
			size_t count = std::count(prefetch_vector.begin(), prefetch_vector.end(), true);
			set_or_increment<size_t,size_t>(no_prefetch_hist, count, 1);
		
			string dump_filename = "trace-stride-test_no_prefetches-step_" + zero_pad(step, 2) + ".json";
			experiment.dump(cache_histogram, prefetch_vector, dump_filename);
			dump_filenames.push_back(dump_filename);			

			random_activity(mapping);
			flush_mapping(mapping);
		}
		plot_stride(string{__FUNCTION__}, dump_filenames);

		// print histogram
		for (pair<size_t const, size_t> const& no_prefetch_hist_pair : no_prefetch_hist) {
			size_t const& no_prefetches = no_prefetch_hist_pair.first;
			size_t const& count = no_prefetch_hist_pair.second;
			L::debug("%zu: %zu times\n", no_prefetches, count);
		}
		
		// find minimum and maximum number of prefetches (note that the
		// map<> is ordered by the key, so we can just take the first/last
		// element)
		// 1. minimum, incl. 0
		pair<size_t const,size_t> const& min_no_prefetches = *no_prefetch_hist.begin();
		// 2. maximum
		pair<size_t const,size_t> const& max_no_prefetches = *no_prefetch_hist.rbegin();
		// 3. minimum non-zero (if any)
		map<size_t, size_t>::const_iterator it_min_nonzero = std::find_if(
			no_prefetch_hist.begin(), no_prefetch_hist.end(),
			[](pair<size_t const, size_t> e) {
				return e.first != 0 && e.second != 0;
			}
		);
		L::debug("Minimum: %zu prefetches (%zu times)\n", min_no_prefetches.first, min_no_prefetches.second);
		L::debug("Maximum: %zu prefetches (%zu times)\n", max_no_prefetches.first, max_no_prefetches.second);
		pair<size_t, size_t> min_nonzero_no_prefetches = {0, 0};
		if (it_min_nonzero != no_prefetch_hist.end()) {
			L::debug("Minimum nonzero: %zu prefetches (%zu times)\n", it_min_nonzero->first, it_min_nonzero->second);
			min_nonzero_no_prefetches = *it_min_nonzero;
		}
		
		unmap_mapping(mapping);

		return Json::object {
			{ "status", "completed" },
			{ "min_no_prefetches", (int)min_no_prefetches.first },
			{ "min_nonzero_no_prefetches", (int)min_nonzero_no_prefetches.first },
			{ "max_no_prefetches", (int)max_no_prefetches.first },
			{ "min_no_prefetches_count", (int)min_no_prefetches.second },
			{ "min_nonzero_no_prefetches_count", (int)min_nonzero_no_prefetches.second },
			{ "max_no_prefetches_count", (int)max_no_prefetches.second },
		};
	}

	
	/**
	 * Find the minimum/maximum stride in range [CACHE_LINE_SIZE,
	 * 4*PAGE_SIZE].
	 *
	 * @param[in]  no_repetitions  Number of repetitions
	 *
	 * @return     JSON structure describing the results.
	 */
	Json test_min_max_stride(size_t no_repetitions) {
		L::info("Test: %s\n", __FUNCTION__);
		Mapping mapping = allocate_mapping(129 * PAGE_SIZE);
		flush_mapping(mapping);

		// start with 1 cache line and hop by multiple of twos power
		// map<stride, max. prefetch count across all "steps" iterations>
		map<ssize_t, size_t> stride_hist;
		vector<string> dump_filenames;
		for (ssize_t sign : {-1, 1}) {
			for (ssize_t stride = sign * CACHE_LINE_SIZE; std::abs(stride) <= 4 * PAGE_SIZE; stride *= 2){
				for (size_t step = 5; step <= 12; step++) {
					L::debug("Testing stride: %zd, steps: %zu\n", stride, step);
					
					// work in a sub_mapping, since probing a large mapping takes time
					Mapping sub_mapping { .base_addr = mapping.base_addr, .size = ((std::abs(stride) * (step + 20)) + CACHE_LINE_SIZE) };
					assert(sub_mapping.size <= mapping.size);

					// run the experiment
					// for negative strides, start at the end of the memory area
					size_t first_access_offset = (sign == 1) ? 0 : (sub_mapping.size - CACHE_LINE_SIZE);
					StrideExperiment experiment { stride, step, first_access_offset, use_nanosleep, fr_thresh, noise_thresh };
					vector<size_t> cache_histogram = experiment.collect_cache_histogram_lazy(sub_mapping, no_repetitions, workload_stride_loop, nullptr);

					// evaluate
					vector<bool> prefetch_vector = experiment.evaluate_cache_histogram(cache_histogram, no_repetitions);
					size_t count = std::count(prefetch_vector.begin(), prefetch_vector.end(), true);
					L::debug("step: %zu, count: %zu\n", step, count);

					string dump_filename = "trace-stride-test_min_max_stride-stride_" + zero_pad(stride, 5) + "-step_" + zero_pad(step, 2) + ".json";
					experiment.dump(cache_histogram, prefetch_vector, dump_filename);
					dump_filenames.push_back(dump_filename);
					if (stride_hist[stride] < count) {
						stride_hist[stride] = count;
					}
					// random_activity(mapping); // takes a long time in our huge memory area
					flush_mapping(mapping);
				}
			}
		}
		plot_stride_minmax(string {__FUNCTION__}, dump_filenames);

		ssize_t min_stride_neg = std::numeric_limits<ssize_t>::min();
		ssize_t min_stride_pos = std::numeric_limits<ssize_t>::max();
		ssize_t max_stride_neg = std::numeric_limits<ssize_t>::max();
		ssize_t max_stride_pos = std::numeric_limits<ssize_t>::min();
		for (pair<ssize_t const, size_t> const& stride_hist_pair : stride_hist) {
			ssize_t const& stride = stride_hist_pair.first;
			size_t const& max_count = stride_hist_pair.second;
			if (stride > 0 && stride < min_stride_pos && max_count > 0) { min_stride_pos = stride; }
			if (stride < 0 && stride > min_stride_neg && max_count > 0) { min_stride_neg = stride; }
			if (max_stride_pos < stride && max_count > 0) { max_stride_pos = stride; }
			if (max_stride_neg > stride && max_count > 0) { max_stride_neg = stride; }
			L::debug("Stride: %zd, max. count: %zu\n", stride, max_count);
		}
		unmap_mapping(mapping);

		return Json::object {
			{"status", "completed"},
			{"min_stride_negative", (min_stride_neg != std::numeric_limits<ssize_t>::min()) ? (int)min_stride_neg : 0},
			{"min_stride_positive", (min_stride_pos != std::numeric_limits<ssize_t>::max()) ? (int)min_stride_pos : 0},
			{"max_stride_negative", (max_stride_neg < 0) ? (int)max_stride_neg : 0},
			{"max_stride_positive", (max_stride_pos > 0) ? (int)max_stride_pos : 0},
		};
	}

	/**
	 * Extended "different PC, different memory" test. Test whether
	 * prefetching works if N low-order bits of the PC collide.
	 *
	 * @param[in]  no_repetitions  Number of repetitions
	 *
	 * @return     JSON structure describing the results.
	 */
	Json test_pc_collision(size_t no_repetitions, size_t no_accesses_on_mapping2) {
		L::info("Test: %s\n", __FUNCTION__);
		Mapping mapping = allocate_mapping(99*PAGE_SIZE);
		Mapping mapping1 {.base_addr = mapping.base_addr, .size = PAGE_SIZE};
		Mapping mapping2 {.base_addr = mapping.base_addr + 98 * PAGE_SIZE, .size = PAGE_SIZE};
		random_activity(mapping1);
		random_activity(mapping2);
		flush_mapping(mapping1);
		flush_mapping(mapping2);

		vector<string> json_dumps_file_paths;
		ssize_t min_colliding_bits = -1;
		for (size_t colliding_bits = 5; colliding_bits <= 24; colliding_bits++) {
			L::debug("\n\ncolliding_bits = %zu\n", colliding_bits);
			
			// run the experiment
			ssize_t stride = 3 * CACHE_LINE_SIZE;
			size_t step = 12;
			StrideExperiment experiment { stride, step, 0, use_nanosleep, fr_thresh, noise_thresh };
			pair<size_t, size_t> ai_collidingbits_noaccesses { colliding_bits, no_accesses_on_mapping2 };
			vector<size_t> cache_histogram = experiment.collect_cache_histogram(mapping1, mapping2, no_repetitions, workload_stride_pc_collision, &ai_collidingbits_noaccesses);
			vector<bool> prefetch_vector = experiment.evaluate_cache_histogram(cache_histogram, no_repetitions, 0.7);
			size_t count = std::count(prefetch_vector.begin(), prefetch_vector.end(), true);

			if (count > 0 && min_colliding_bits == -1) {
				min_colliding_bits = colliding_bits;
			}

			string dump_filename = "trace-stride-test_pc_collision-acc_" + zero_pad(no_accesses_on_mapping2, 2) + "-coll_" + zero_pad(colliding_bits, 2) + ".json";
			experiment.dump(cache_histogram, prefetch_vector, dump_filename);
			json_dumps_file_paths.push_back(dump_filename);

			// print results
			for (size_t i = 0; i < cache_histogram.size(); i++) {
				L::debug("CL %2zu: %zu%s\n", i, cache_histogram[i], prefetch_vector[i] ? " (*)" : "");
			}
			L::debug("Prefetch Count: %zu\n", count);

			random_activity(mapping1);
			random_activity(mapping2);
			flush_mapping(mapping1);
			flush_mapping(mapping2);
		}

		plot_stride(string{__FUNCTION__} + "_" + zero_pad(no_accesses_on_mapping2, 2) + "acc", json_dumps_file_paths);

		unmap_mapping(mapping);
		
		return Json::object {
			{"status", "completed"},
			{"collision_found", (min_colliding_bits >= 0)},
			{"min_colliding_bits", (int)min_colliding_bits},
		};
	}

	/**
	 * Test whether the stride prefetcher supports strides less than cache
	 * line size.
	 *
	 * @param[in]  no_repetitions  Number of repetitions
	 *
	 * @return     JSON structure describing the results
	 */
	Json test_stride_less_than_cl_size(size_t no_repetitions) {
		L::info("Test: %s\n", __FUNCTION__);
		Mapping mapping = allocate_mapping(PAGE_SIZE);
		random_activity(mapping);
		flush_mapping(mapping);

		// Run experiment with stride = CACHE_LINE_SIZE / 4 and 4 steps,
		// i.e., performing 4 accesses within the same cache line
		StrideExperiment experiment_sub_cl { CACHE_LINE_SIZE/4, 4, 0, use_nanosleep, fr_thresh, noise_thresh };
		vector<size_t> cache_histogram_sub_cl = experiment_sub_cl.collect_cache_histogram(mapping, no_repetitions, workload_stride_loop, nullptr);
		vector<bool> prefetch_vector_sub_cl = experiment_sub_cl.evaluate_cache_histogram(cache_histogram_sub_cl, no_repetitions);
		experiment_sub_cl.dump(cache_histogram_sub_cl, prefetch_vector_sub_cl, "trace-stride-test_stride_less_than_cl_size-sub_cl.json");
		size_t count_sub_cl = std::count(prefetch_vector_sub_cl.begin(), prefetch_vector_sub_cl.end(), true);

		random_activity(mapping);
		flush_mapping(mapping);

		// Run baseline experiment: stride = CACHE_LINE_SIZE, only one
		// step. If the prefetcher ignores the sub-cache-line stride, this
		// experiment should produce the exact same result as the previous
		// one. Otherwise, if the prefetcher detects the small stride, we
		// expect more prefetching on the previous experiment than here.
		StrideExperiment experiment_cl { CACHE_LINE_SIZE, 1, 0, use_nanosleep, fr_thresh, noise_thresh };
		vector<size_t> cache_histogram_cl = experiment_cl.collect_cache_histogram(mapping, no_repetitions, workload_stride_loop, nullptr);
		vector<bool> prefetch_vector_cl = experiment_cl.evaluate_cache_histogram(cache_histogram_cl, no_repetitions);
		experiment_cl.dump(cache_histogram_cl, prefetch_vector_cl, "trace-stride-test_stride_less_than_cl_size-cl.json");
		size_t count_cl = std::count(prefetch_vector_cl.begin(), prefetch_vector_cl.end(), true);
		
		plot_stride(string{__FUNCTION__}, {
			"trace-stride-test_stride_less_than_cl_size-cl.json",
			"trace-stride-test_stride_less_than_cl_size-sub_cl.json",
		});

		unmap_mapping(mapping);

		if (count_sub_cl == 0 && count_cl == 0) {
			return Json::object {
				{"status", "completed"},
				{"stride_less_than_cl_size", "no effect"}
			};
		} else if (count_sub_cl > 0 && count_cl == 0) {
			return Json::object {
				{"status", "completed"},
				{"stride_less_than_cl_size", "triggers prefetch"}
			};
		} else {
			return Json::object {
				{"status", "completed"},
				{"stride_less_than_cl_size", "inconclusive result"}
			};
		}
	}

	Json test_random_offset_within_cl(size_t no_repetitions) {
		L::info("Test: %s\n", __FUNCTION__);
		Mapping mapping = allocate_mapping(PAGE_SIZE);
		random_activity(mapping);
		flush_mapping(mapping);

		vector<string> dump_filenames;
		// map<stride, baseline > 0 && random > 0>
		map<ssize_t, bool> results;
		for (ssize_t sign : {-1, 1}) {
			for (ssize_t stride : {sign * CACHE_LINE_SIZE, sign * 3 * CACHE_LINE_SIZE}) {
				size_t first_access_offset = (sign == 1) ? 0 : (mapping.size - CACHE_LINE_SIZE);
				StrideExperiment experiment { stride, 6, first_access_offset, use_nanosleep, fr_thresh, noise_thresh };
				
				// Run experiment. The workload will make sure to access random
				// locations within the cache lines.
				vector<size_t> cache_histogram_random = experiment.collect_cache_histogram(mapping, no_repetitions, workload_stride_random_offset_within_cl, nullptr);
				vector<bool> prefetch_vector_random = experiment.evaluate_cache_histogram(cache_histogram_random, no_repetitions);
				string dump_filename_random = "trace-stride-test_random_offset_within_cl-stride_" + zero_pad(stride, 5) + "-random.json";
				experiment.dump(cache_histogram_random, prefetch_vector_random, dump_filename_random);
				dump_filenames.push_back(dump_filename_random);

				random_activity(mapping);
				flush_mapping(mapping);

				// Run baseline experiment (accessing offset 0 within all the
				// cache lines)
				vector<size_t> cache_histogram_baseline = experiment.collect_cache_histogram(mapping, no_repetitions, workload_stride_loop, nullptr);
				vector<bool> prefetch_vector_baseline = experiment.evaluate_cache_histogram(cache_histogram_baseline, no_repetitions);
				string dump_filename_baseline = "trace-stride-test_random_offset_within_cl-stride_" + zero_pad(stride, 5) + "-baseline.json";
				experiment.dump(cache_histogram_baseline, prefetch_vector_baseline, dump_filename_baseline);
				dump_filenames.push_back(dump_filename_baseline);
				
				random_activity(mapping);
				flush_mapping(mapping);

				size_t prefetch_count_random = std::count(prefetch_vector_random.begin(), prefetch_vector_random.end(), true);
				size_t prefetch_count_baseline = std::count(prefetch_vector_random.begin(), prefetch_vector_random.end(), true);

				results[stride] = (prefetch_count_baseline > 0 && prefetch_count_random > 0);
			}
		}

		plot_stride(string{__FUNCTION__}, dump_filenames);
		Json::object results_json;
		for (pair<ssize_t const, bool> const& results_pair : results) {
			ssize_t const& stride = results_pair.first;
			bool const& condition_fulfilled = results_pair.second;
			
			results_json[std::to_string(stride)] = Json {(bool) condition_fulfilled};
		}

		unmap_mapping(mapping);

		return Json::object {
			{"status", "completed"},
			{"offset_within_cl_irrelevant", results_json},
		};
	}

	/**
	 * Test whether the prefetcher crosses the page boundary.
	 *
	 * @param[in]  no_repetitions  Number of repetitions
	 *
	 * @return     JSON structure describing the results
	 */
	Json test_cross_page_boundary(size_t no_repetitions) {
		L::info("Test: %s\n", __FUNCTION__);
		Mapping mapping = allocate_mapping(2 * PAGE_SIZE);
		random_activity(mapping);
		flush_mapping(mapping);

		// align the accesses towards the end of the page, such that the
		// first prefetch would hit the first cache line of the next page
		ssize_t stride = 3 * CACHE_LINE_SIZE;
		size_t step = 12;
		assert(step * stride < PAGE_SIZE);
		size_t first_access_offset = PAGE_SIZE - step * stride;

		StrideExperiment experiment { stride, step, first_access_offset, use_nanosleep, fr_thresh, noise_thresh };
		vector<size_t> cache_histogram = experiment.collect_cache_histogram(mapping, no_repetitions, workload_stride_loop, nullptr);
		vector<bool> prefetch_vector = experiment.evaluate_cache_histogram(cache_histogram, no_repetitions);
		experiment.dump(cache_histogram, prefetch_vector, "trace-stride-test_cross_page_boundary.json");
		
		// count prefetches. if any prefetches are observed, the prefetcher
		// crossed the page boundary.
		size_t count = std::count(prefetch_vector.begin(), prefetch_vector.end(), true);
		
		plot_stride(string{__FUNCTION__}, {"trace-stride-test_cross_page_boundary.json"});

		unmap_mapping(mapping);

		return Json::object {
			{"status", "completed"},
			{"cross_page_boundary", (count > 0)},
		};
	}

	virtual Json identify() override {
		size_t no_repetitions = 40000 * (PAGE_SIZE / 4096);

		Json test_results = test_direction(no_repetitions);
		bool identified = (
			test_results["positive_direction"].bool_value() == true
			|| test_results["negative_direction"].bool_value() == true
		);
		return Json::object {
			{ "identified", identified },
			{ "test_direction", test_results },
		};
	}

	virtual Json characterize() override {
		size_t no_repetitions = 40000 * (PAGE_SIZE / 4096);

		return Json::object {
			{ "test_trigger_same_pc_different_memory", test_trigger_same_pc_different_memory(no_repetitions) },
			{ "test_trigger_different_pc_same_memory", test_trigger_different_pc_same_memory(no_repetitions) },
			{ "test_trigger_different_pc_different_memory", test_trigger_different_pc_different_memory(no_repetitions) },
			{ "test_overview", test_overview(no_repetitions) },
			{ "test_load_pref_corr", test_load_pref_corr(no_repetitions) },
			{ "test_no_prefetches", test_no_prefetches(no_repetitions) },
			{ "test_min_max_stride", test_min_max_stride(no_repetitions) },
			{ "test_pc_collision_1acc", test_pc_collision(no_repetitions, 1) },
			{ "test_pc_collision_2acc", test_pc_collision(no_repetitions, 2) },
			{ "test_stride_less_than_cl_size", test_stride_less_than_cl_size(no_repetitions) },
			{ "test_random_offset_within_cl", test_random_offset_within_cl(no_repetitions) },
			{ "test_cross_page_boundary", test_cross_page_boundary(no_repetitions) },
		};
	}
};
