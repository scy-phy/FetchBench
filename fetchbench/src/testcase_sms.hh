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

#include "testcase_sms_smsexperiment.hh"

using json11::Json;
using std::vector;
using std::pair;
using std::map;

class TestCaseSMS : public TestCaseBase {
private:
	size_t const fr_thresh;
	size_t const noise_thresh;
	bool use_nanosleep = false;

public:
	TestCaseSMS(size_t fr_thresh, size_t noise_thresh, bool use_nanosleep)
	: fr_thresh {fr_thresh}
	, noise_thresh {noise_thresh}
	, use_nanosleep {use_nanosleep}
	{}

	virtual string id() override {
		return "sms";
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
	 * Basic test for SMS prefetcher
	 *
	 * @param[in]  no_repetitions  Number of repetitions
	 *
	 * @return     JSON structure describing the result.
	 */
	Json test_trigger_same_pc_same_memory(size_t no_repetitions) {
		L::info("Test: %s\n", __FUNCTION__);
		Mapping mapping = allocate_mapping(17 * PAGE_SIZE);
		random_activity(mapping);
		flush_mapping(mapping);

		Mapping mapping1 { mapping.base_addr, 16 * PAGE_SIZE };
		Mapping mapping2 { mapping.base_addr + 16 * PAGE_SIZE, PAGE_SIZE };

		// base experiment
		vector<size_t> training_offsets {
			4 * CACHE_LINE_SIZE, 1 * CACHE_LINE_SIZE, 6 * CACHE_LINE_SIZE, 7 * CACHE_LINE_SIZE
		};
		vector<size_t> trigger_offsets { training_offsets[0] };
		SMSExperiment experiment { training_offsets, trigger_offsets, use_nanosleep, fr_thresh, noise_thresh };

		// run experiments: once with accessing additional regions between
		// training and triggering, once without.
		bool access_regions = false;
		vector<size_t> cache_histogram_noacc = experiment.collect_cache_histogram(mapping1, mapping2, no_repetitions, workload_sms_same_pc_same_memory, &access_regions);
		flush_mapping(mapping);
		random_activity(mapping2);
		access_regions = true;
		vector<size_t> cache_histogram_acc = experiment.collect_cache_histogram(mapping1, mapping2, no_repetitions, workload_sms_same_pc_same_memory, &access_regions);

		// evaluate
		vector<bool> prefetch_vector_noacc = experiment.evaluate_cache_histogram(cache_histogram_noacc, no_repetitions);
		vector<bool> prefetch_vector_acc = experiment.evaluate_cache_histogram(cache_histogram_acc, no_repetitions);
		experiment.dump(cache_histogram_noacc, prefetch_vector_noacc, "trace-sms-test_trigger_same_pc_same_memory-noacc.json");
		experiment.dump(cache_histogram_acc, prefetch_vector_acc, "trace-sms-test_trigger_same_pc_same_memory-acc.json");
		size_t prefetch_count_noacc = std::count(prefetch_vector_noacc.begin(), prefetch_vector_noacc.end(), true);
		size_t prefetch_count_acc = std::count(prefetch_vector_acc.begin(), prefetch_vector_acc.end(), true);

		// plot
		plot_sms(__FUNCTION__, {
			"trace-sms-test_trigger_same_pc_same_memory-noacc.json",
			"trace-sms-test_trigger_same_pc_same_memory-acc.json"
		});

		unmap_mapping(mapping);
		
		// Report positive test result if any prefetching was detected
		return Json::object {
			{"status", "completed"},
			{"triggers_prefetch_without_additional_region_accesses", (prefetch_count_noacc > 0)},
			{"triggers_prefetch_with_additional_region_accesses", (prefetch_count_acc > 0)},
		};
	}


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
		Mapping mapping = allocate_mapping(17 * PAGE_SIZE);
		random_activity(mapping);
		flush_mapping(mapping);

		Mapping mapping1 { mapping.base_addr, PAGE_SIZE };
		Mapping mapping2 { mapping.base_addr + 16 * PAGE_SIZE, PAGE_SIZE };

		// base experiment
		vector<size_t> training_offsets {
			4 * CACHE_LINE_SIZE, 1 * CACHE_LINE_SIZE, 6 * CACHE_LINE_SIZE, 7 * CACHE_LINE_SIZE
		};
		vector<size_t> trigger_offsets { training_offsets[0] };
		SMSExperiment experiment { training_offsets, trigger_offsets, use_nanosleep, fr_thresh, noise_thresh };

		// run experiment
		vector<size_t> cache_histogram = experiment.collect_cache_histogram(mapping1, mapping2, no_repetitions, workload_sms_same_pc_different_memory, nullptr);
		vector<bool> prefetch_vector = experiment.evaluate_cache_histogram(cache_histogram, no_repetitions);
		experiment.dump(cache_histogram, prefetch_vector, "trace-sms-test_trigger_same_pc_different_memory.json");
		size_t prefetch_count = std::count(prefetch_vector.begin(), prefetch_vector.end(), true);

		plot_sms(__FUNCTION__, {"trace-sms-test_trigger_same_pc_different_memory.json"});

		unmap_mapping(mapping);
		
		// Report positive test result if any prefetching was detected
		return Json::object {
			{"status", "completed"},
			{"triggers_prefetch", (prefetch_count > 0)},
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
		Mapping mapping = allocate_mapping(17 * PAGE_SIZE);
		random_activity(mapping);
		flush_mapping(mapping);

		Mapping mapping1 { mapping.base_addr, 16 * PAGE_SIZE };
		Mapping mapping2 { mapping.base_addr + 16 * PAGE_SIZE, PAGE_SIZE };

		// base experiment
		vector<size_t> training_offsets {
			4 * CACHE_LINE_SIZE, 1 * CACHE_LINE_SIZE, 6 * CACHE_LINE_SIZE, 7 * CACHE_LINE_SIZE
		};
		vector<size_t> trigger_offsets { training_offsets[0] };
		SMSExperiment experiment { training_offsets, trigger_offsets, use_nanosleep, fr_thresh, noise_thresh };

		// run experiments: once with accessing additional regions between
		// training and triggering, once without.
		bool access_regions = false;
		vector<size_t> cache_histogram_noacc = experiment.collect_cache_histogram(mapping1, mapping2, no_repetitions, workload_sms_different_pc_same_memory, &access_regions);
		flush_mapping(mapping);
		random_activity(mapping2);
		access_regions = true;
		vector<size_t> cache_histogram_acc = experiment.collect_cache_histogram(mapping1, mapping2, no_repetitions, workload_sms_different_pc_same_memory, &access_regions);

		// evaluate
		vector<bool> prefetch_vector_noacc = experiment.evaluate_cache_histogram(cache_histogram_noacc, no_repetitions);
		vector<bool> prefetch_vector_acc = experiment.evaluate_cache_histogram(cache_histogram_acc, no_repetitions);
		experiment.dump(cache_histogram_noacc, prefetch_vector_noacc, "trace-sms-test_trigger_different_pc_same_memory-noacc.json");
		experiment.dump(cache_histogram_acc, prefetch_vector_acc, "trace-sms-test_trigger_different_pc_same_memory-acc.json");
		size_t prefetch_count_noacc = std::count(prefetch_vector_noacc.begin(), prefetch_vector_noacc.end(), true);
		size_t prefetch_count_acc = std::count(prefetch_vector_acc.begin(), prefetch_vector_acc.end(), true);

		// plot
		plot_sms(__FUNCTION__, {
			"trace-sms-test_trigger_different_pc_same_memory-noacc.json",
			"trace-sms-test_trigger_different_pc_same_memory-acc.json"
		});

		unmap_mapping(mapping);
		
		// Report positive test result if any prefetching was detected
		return Json::object {
			{"status", "completed"},
			{"triggers_prefetch_without_additional_region_accesses", (prefetch_count_noacc > 0)},
			{"triggers_prefetch_with_additional_region_accesses", (prefetch_count_acc > 0)},
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
		Mapping mapping = allocate_mapping(17 * PAGE_SIZE);
		random_activity(mapping);
		flush_mapping(mapping);

		Mapping mapping1 { mapping.base_addr, PAGE_SIZE };
		Mapping mapping2 { mapping.base_addr + 16 * PAGE_SIZE, PAGE_SIZE };

		// base experiment
		vector<size_t> training_offsets {
			4 * CACHE_LINE_SIZE, 1 * CACHE_LINE_SIZE, 6 * CACHE_LINE_SIZE, 7 * CACHE_LINE_SIZE
		};
		vector<size_t> trigger_offsets { training_offsets[0] };
		SMSExperiment experiment { training_offsets, trigger_offsets, use_nanosleep, fr_thresh, noise_thresh };

		// run experiment
		vector<size_t> cache_histogram = experiment.collect_cache_histogram(mapping1, mapping2, no_repetitions, workload_sms_different_pc_different_memory, nullptr);
		vector<bool> prefetch_vector = experiment.evaluate_cache_histogram(cache_histogram, no_repetitions);
		experiment.dump(cache_histogram, prefetch_vector, "trace-sms-test_trigger_different_pc_different_memory.json");
		size_t prefetch_count = std::count(prefetch_vector.begin(), prefetch_vector.end(), true);

		plot_sms(__FUNCTION__, {"trace-sms-test_trigger_different_pc_different_memory.json"});

		unmap_mapping(mapping);
		
		// Report positive test result if any prefetching was detected
		return Json::object {
			{"status", "completed"},
			{"triggers_prefetch", (prefetch_count > 0)},
		};
	}

	/**
	 * Tests whether the prefetcher maps two differrent memory areas to the
	 * same prefetch data structure entry or not.
	 *
	 * @param[in]  no_repetitions  Number of repetitions
	 *
	 * @return     JSON structure describing the result.
	 */
	Json test_pc_collision(size_t no_repetitions) {
		L::info("Test: %s\n", __FUNCTION__);
		Mapping mapping = allocate_mapping(17 * PAGE_SIZE);
		random_activity(mapping);
		flush_mapping(mapping);

		Mapping mapping1 { mapping.base_addr, PAGE_SIZE };
		Mapping mapping2 { mapping.base_addr + 16 * PAGE_SIZE, PAGE_SIZE };

		// base experiment
		vector<size_t> training_offsets {
			4 * CACHE_LINE_SIZE, 1 * CACHE_LINE_SIZE, 6 * CACHE_LINE_SIZE, 7 * CACHE_LINE_SIZE
		};
		vector<size_t> trigger_offsets { training_offsets[0] };
		SMSExperiment experiment { training_offsets, trigger_offsets, use_nanosleep, fr_thresh, noise_thresh };

		ssize_t min_colliding_bits = -1;
		vector<string> json_dumps_file_paths;
		for (size_t colliding_bits = 5; colliding_bits <= 24; colliding_bits++) {
			L::debug("colliding_bits = %zu\n", colliding_bits);
			
			// run experiment
			vector<size_t> cache_histogram = experiment.collect_cache_histogram(mapping1, mapping2, no_repetitions, workload_sms_pc_collision, &colliding_bits);
			
			// evaluate
			vector<bool> prefetch_vector = experiment.evaluate_cache_histogram(cache_histogram, no_repetitions);
			string dump_filename = "trace-sms-test_pc_collision-coll_" + zero_pad(colliding_bits, 2) + ".json";
			experiment.dump(cache_histogram, prefetch_vector, dump_filename);
			json_dumps_file_paths.push_back(dump_filename);
			size_t prefetch_count = std::count(prefetch_vector.begin(), prefetch_vector.end(), true);
			if (prefetch_count > 0 && min_colliding_bits == -1) {
				L::debug("Found min_colliding_bits: %zd\n", min_colliding_bits);
				min_colliding_bits = colliding_bits;
			}

			// clear state
			random_activity(mapping1);
			random_activity(mapping2);
			flush_mapping(mapping1);
			flush_mapping(mapping2);
		}
		plot_sms(__FUNCTION__, json_dumps_file_paths);

		unmap_mapping(mapping);
		
		// Report positive test result if any prefetching was detected
		return Json::object {
			{"status", "completed"},
			{"collision_found", (min_colliding_bits >= 0)},
			{"min_colliding_bits", (int)min_colliding_bits},
		};
	}

	/**
	 * Tests the maximum region size sms prefetcher in +ve and -ve direction.
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

		Mapping mapping1 { mapping.base_addr, PAGE_SIZE };
		Mapping mapping2 { mapping.base_addr + PAGE_SIZE, PAGE_SIZE };
		size_t max_prefetch_pos = 0, max_prefetch_neg = 0, max_prefetch = 0;
		vector<string> dump_filenames;
		vector<size_t> training_offsets;

		// Increase the region size [6, 20] in +ve/-ve direction and test number of prefetched CL
		for (ssize_t sign : {-1, 1}) {
			for (size_t region_size = 5; region_size <= 20; region_size++) {
				// First access is always constant and remaining maccess
				// are randomly generated to avoid stride/pattern prefetching.
				size_t first_access_offset;
				if (sign > 0) {
					first_access_offset = 0;
				} else {
					first_access_offset = PAGE_SIZE - CACHE_LINE_SIZE;
				}
				training_offsets.push_back(first_access_offset);
				L::debug("access order: %zu ", training_offsets[0] / CACHE_LINE_SIZE);
				while (training_offsets.size() <= region_size) {
					size_t index = first_access_offset + sign * random_uint32(1, region_size) * CACHE_LINE_SIZE;
					if (std::find(training_offsets.begin(), training_offsets.end(), index) == training_offsets.end()) {
						training_offsets.push_back(index);
						L::debug("%zu ", index / CACHE_LINE_SIZE);
					}
				}
				L::debug("\n");

				// make sure the pseudo-random order does not contain a stride pattern
				for (size_t i = 2; i < training_offsets.size(); i++) {
					ssize_t deltas[2] = {
						(ssize_t)training_offsets[i - 1] - (ssize_t)training_offsets[i - 2],
						(ssize_t)training_offsets[i - 0] - (ssize_t)training_offsets[i - 1]
					};
					if (deltas[0] == deltas[1]) {
						std::shuffle(std::next(training_offsets.begin()), training_offsets.end(), *get_rng());
						i = 2;
						L::debug("Shuffled offsets because the list contained a stride, new access order: ");
						for (size_t training_offset : training_offsets) {
							L::debug("%zu ", training_offset);
						}
						L::debug("\n");
					}
				}

				vector<size_t> trigger_offsets { training_offsets[0] };
				SMSExperiment experiment { training_offsets, trigger_offsets, use_nanosleep, fr_thresh, noise_thresh };

				// run experiments
				vector<size_t> cache_histogram = experiment.collect_cache_histogram(mapping1, mapping2, no_repetitions, workload_sms_same_pc_different_memory, nullptr);
				random_activity(mapping);
				flush_mapping(mapping);
				vector<bool> prefetch_vector = experiment.evaluate_cache_histogram(cache_histogram, no_repetitions);

				// Dump cache histogram
				string sign_string = ((sign > 0)? "pos_":"neg_");
				string dump_filename = "trace-sms-test_direction_" + sign_string + zero_pad(region_size, 4) + ".json";
				experiment.dump(cache_histogram, prefetch_vector, dump_filename);
				dump_filenames.push_back(dump_filename);

				// Count prefetches
				size_t prefetch_count;
				if (sign > 0)
					prefetch_count = std::count(std::next(prefetch_vector.begin(), 1), std::next(prefetch_vector.begin(), region_size + 1), true);
				else
					prefetch_count = std::count(std::prev(prefetch_vector.end(), region_size+2), std::prev(prefetch_vector.end(), 1), true);

				L::debug("prefetched: %zu \n", prefetch_count);
				training_offsets.clear();

				if (prefetch_count > max_prefetch) {
					max_prefetch = prefetch_count;
				} else {
					if (sign > 0) {
						max_prefetch_pos = max_prefetch;
					} else {
						max_prefetch_neg = max_prefetch;
					}
					max_prefetch = 0;
					break;
				}
			}
		}
		plot_sms(__FUNCTION__, dump_filenames);
		unmap_mapping(mapping);

		return Json::object {
			{"status", "completed"},
			{"max_sms_positive", (int)max_prefetch_pos},
			{"max_sms_negative", (int)max_prefetch_neg},
		};
	}

	/**
	 * Tests the region boundary of sms prefetcher.
	 *
	 * @param[in]  no_repetitions  Number of repetitions
	 *
	 * @return     JSON structure describing the result.
	 */
	Json test_region_boundary(size_t no_repetitions) {
		L::info("Test: %s\n", __FUNCTION__);
		Mapping mapping = allocate_mapping(8 * PAGE_SIZE);
		random_activity(mapping);
		flush_mapping(mapping);

		Mapping mapping1 { mapping.base_addr, 4 * PAGE_SIZE };
		Mapping mapping2 { mapping.base_addr + 4 * PAGE_SIZE, 4 * PAGE_SIZE };
		size_t base_prefetch = 0;
		int region_boundary = 0;
		vector<string> dump_filenames;
		vector<size_t> training_offsets;
		size_t first_access_offset;
		size_t region_size = 5;

		// Test region boundary for 8 * CACHE_LINE_SIZE, and keep
		// incrementing by CACHE_LINE_SIZE until the SMS prefetcher stops
		// learning pattern
		size_t boundary_start = 8 * CACHE_LINE_SIZE;
		for (size_t boundary = boundary_start; boundary <= PAGE_SIZE && region_boundary == 0; boundary += CACHE_LINE_SIZE) {
			base_prefetch = 0;
			for (size_t test_id : {0, 1}) {
				// generate a pseudo-random access order
				first_access_offset = boundary - (region_size + 1) * CACHE_LINE_SIZE + test_id * CACHE_LINE_SIZE;
				assert(first_access_offset < boundary); // make sure there is no overflow

				training_offsets.push_back(first_access_offset);
				L::debug("Boundary: %zu (%s) access order: ", boundary, test_id? "test":"base");
				L::debug("%zu ", first_access_offset / CACHE_LINE_SIZE);
				while (training_offsets.size() <= region_size) {
					size_t index = first_access_offset + random_uint32(1, region_size) * CACHE_LINE_SIZE;
					if (std::find(training_offsets.begin(), training_offsets.end(), index) == training_offsets.end()) {
						training_offsets.push_back(index);
						L::debug("%zu ", index / CACHE_LINE_SIZE);
					}
				}
				L::debug("\n");

				// make sure the pseudo-random order does not contain a stride pattern
				for (size_t i = 2; i < training_offsets.size(); i++) {
					ssize_t deltas[2] = {
						(ssize_t)training_offsets[i - 1] - (ssize_t)training_offsets[i - 2],
						(ssize_t)training_offsets[i - 0] - (ssize_t)training_offsets[i - 1]
					};
					if (deltas[0] == deltas[1]) {
						std::shuffle(training_offsets.begin(), training_offsets.end(), *get_rng());
						i = 2;
						L::debug("Shuffled offsets because the list contained a stride, new access order: ");
						for (size_t training_offset : training_offsets) {
							L::debug("%zu ", training_offset);
						}
						L::debug("\n");
					}
				}

				// set up the experiment
				vector<size_t> trigger_offsets { training_offsets[0] };
				SMSExperiment experiment { training_offsets, trigger_offsets, use_nanosleep, fr_thresh, noise_thresh };

				// run experiments
				vector<size_t> cache_histogram = experiment.collect_cache_histogram(mapping1, mapping2, no_repetitions, workload_sms_same_pc_different_memory, nullptr);
				random_activity(mapping);
				flush_mapping(mapping);
				vector<bool> prefetch_vector = experiment.evaluate_cache_histogram(cache_histogram, no_repetitions);

				// Dump cache histogram
				string test_type = ((test_id == 0)? "base_":"test_");
				string dump_filename = "trace-sms-test_region_boundary_" + test_type + zero_pad(boundary, 4) + ".json";
				experiment.dump(cache_histogram, prefetch_vector, dump_filename);
				dump_filenames.push_back(dump_filename);

				// Count prefetches
				size_t prefetch_count;
				if (test_id > 0)
					prefetch_count = std::count(std::next(prefetch_vector.begin(), boundary/CACHE_LINE_SIZE - region_size), std::next(prefetch_vector.begin(), boundary/CACHE_LINE_SIZE + 1), true);
				else
					prefetch_count = std::count(std::next(prefetch_vector.begin(), boundary/CACHE_LINE_SIZE - region_size - 1), std::next(prefetch_vector.begin(), boundary/CACHE_LINE_SIZE), true);

				L::debug("prefetched: %zu\n", prefetch_count);
				training_offsets.clear();

				if (test_id == 0) {
					base_prefetch = prefetch_count;
				} else if (base_prefetch != prefetch_count){
					region_boundary = boundary;
				}
				// No SMS prefetcher detected
				if (base_prefetch == 0) {
					region_boundary = -1;
					break;
				}
			}
		}
		plot_sms(__FUNCTION__, dump_filenames);
		unmap_mapping(mapping);

		return Json::object {
			{"status", "completed"},
			{"region_boundary", (int)region_boundary},
		};
	}

	/**
	 * Tests the total number of entries in training stage of SMS
	 * prefetcher. In other words, maximum number of SMS regions
	 * that can be trained consequently.
	 *
	 * @param[in]  no_repetitions  Number of repetitions
	 *
	 * @return     JSON structure describing the result.
	 */
	Json test_training_entries(size_t no_repetitions) {
		L::info("Test: %s\n", __FUNCTION__);
		Mapping mapping = allocate_mapping(17 * PAGE_SIZE);
		random_activity(mapping);
		flush_mapping(mapping);

		Mapping mapping1 { mapping.base_addr, 16 * PAGE_SIZE };
		Mapping mapping2 { mapping.base_addr + 16 * PAGE_SIZE, PAGE_SIZE };
		vector<string> dump_filenames;
		size_t entries = 1;

		// base experiment
		vector<size_t> training_offsets {
			4 * CACHE_LINE_SIZE, 1 * CACHE_LINE_SIZE, 6 * CACHE_LINE_SIZE, 7 * CACHE_LINE_SIZE, 10 * CACHE_LINE_SIZE
		};
		vector<size_t> trigger_offsets { training_offsets[0] };
		SMSExperiment experiment { training_offsets, trigger_offsets, use_nanosleep, fr_thresh, noise_thresh };

		// run experiments: once with accessing additional regions between
		// training and triggering to observe the eviction of training entry.
		for (entries = 2; entries < 15; entries++) {
			vector<size_t> cache_histogram = experiment.collect_cache_histogram(mapping1, mapping2, no_repetitions, workload_sms_training_entries, &entries);
			flush_mapping(mapping);
			random_activity(mapping2);
			// evaluate
			vector<bool> prefetch_vector = experiment.evaluate_cache_histogram(cache_histogram, no_repetitions);
			string dump_filename = "trace-sms-test_training_entries_" + zero_pad(entries, 4) + ".json";
			experiment.dump(cache_histogram, prefetch_vector, dump_filename);
			dump_filenames.push_back(dump_filename);
			size_t prefetch_count = std::count(prefetch_vector.begin(), prefetch_vector.end(), true);
			L::debug("prefetch count: %zu entries: %zu\n\n", prefetch_count, entries);

			// break as soon as the prefetching < training
			if (prefetch_count < training_offsets.size() - 1) {
				break;
			}
		}
		unmap_mapping(mapping);
		// plot
		plot_sms(__FUNCTION__, dump_filenames);

		// Report positive test result if any prefetching was detected
		return Json::object {
			{"status", "completed"},
			{"max_training_entries", (int)(entries)},
		};
	}

	virtual Json identify() override {
		size_t no_repetitions = 40000 * (PAGE_SIZE / 4096);
		
		Json test_results_pc = test_trigger_same_pc_different_memory(no_repetitions);
		Json test_results_mem = test_trigger_different_pc_same_memory(no_repetitions);
		bool identified = (test_results_pc["triggers_prefetch"].bool_value() == true
				|| test_results_mem["triggers_prefetch_with_additional_region_accesses"].bool_value() == true);


		return Json::object {
			{ "identified", identified },
			{ "test_result_same_pc", test_results_pc },
			{ "test_result_same_mem", test_results_mem },

		};
	}

	virtual Json characterize() override {
		size_t no_repetitions = 40000 * (PAGE_SIZE / 4096);

		return Json::object {
			{ "test_trigger_same_pc_same_memory", test_trigger_same_pc_same_memory(no_repetitions) },
			{ "test_trigger_different_pc_different_memory", test_trigger_different_pc_different_memory(no_repetitions) },
			{ "test_region_boundary", test_region_boundary(2 * no_repetitions) },
			{ "test_direction", test_direction(no_repetitions) },
			{ "test_pc_collision", test_pc_collision(no_repetitions) },
			{ "test_training_entries", test_training_entries(2 * no_repetitions) },
		};
	}
};

