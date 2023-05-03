#pragma once

#include <string>
#include <ctime>
#include <fstream>

#include "json11.hpp"
#include "logger.hh"

using json11::Json;

class TestCaseBase {
protected:
	/**
	 * Code to run BEFORE the actual test, e.g., setting some MSRs to
	 * disable/enable certain hardware features.
	 *
	 * @return     A JSON structure describing the result (and pot.
	 *             errors).
	 */
	virtual Json pre_test() {
		return Json::object {};
	}

	/**
	 * The code to run the identification test for this prefetcher.
	 * The resulting Json MUST contain a boolean value "identified".
	 *
	 * @return     A JSON structure describing the result (and pot.
	 *             errors).
	 */
	virtual Json identify() = 0;

	/**
	 * The code to run the characterization tests for this prefetcher.
	 *
	 * @return     A JSON structure describing the result (and pot.
	 *             errors).
	 */
	virtual Json characterize() = 0;

	/**
	 * Code to run AFTER the actual test, e.g., setting some MSRs to
	 * disable/enable certain hardware features.
	 *
	 * @return     A JSON structure describing the result (and pot.
	 *             errors).
	 */
	virtual Json post_test() {
		return Json::object {};
	}

public:
	/**
	 * Returns a short identifier string for the testcase
	 *
	 * @return     Identifier string
	 */
	virtual std::string id() = 0;

	/**
	 * Run the whole test, incl. pre-test and post-test code.
	 *
	 * @param[in]  only_identification  Determines whether only the
	 *                                  identification test, or both
	 *                                  identification and characterization
	 *                                  tests are run.
	 *
	 * @return     A JSON structure describing the result (and pot.
	 *             errors).
	 */
	Json run(bool only_identification) {
		// take timestamp before the timestamp begins
		time_t time_begin = time(NULL);

		if (only_identification) {
			L::info("Running only identification tests.\n");
		}

		// Run pre-test and identification test in any case
		Json results_pre_test = pre_test();
		Json results_identification = identify();

		// Run characterization only if (a) the identification test was
		// successful and (b) characterization tests were not disabled
		Json results_characterization = {"skipped"};
		if ( ! only_identification && results_identification["identified"].bool_value() == true) {
			results_characterization = characterize();
		}
		
		// Run post-test in any case
		Json results_post_test = post_test();
		
		// take timestamp after the testcase finished
		time_t time_end = time(NULL);

		// collect all results in a Json structure
		return Json::object {
			{"pre_test", results_pre_test},
			{"identification", results_identification},
			{"characteristics", results_characterization},
			{"post_test", results_post_test},
			{"runtime_sec", (int)(time_end-time_begin)},
		};
	}
};
