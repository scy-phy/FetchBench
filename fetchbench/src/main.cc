#include <cstdio>
#include <getopt.h>
#include <string>
#include <memory>

#include "json11.hpp"

#include "testcase.hh"
#include "testcases.hh"
#include "utils.hh"
#include "logger.hh"
#include "calibrate.hh"
#include "cacheutils.hh"

using json11::Json;
using std::string;
using std::unique_ptr;
using std::make_unique;

int main(int argc, char** argv) {
	// === parse command line options ===
	// (-c) CPU core to move the process to
	int opt_target_cpu = 0;
	// (-e) CPU core to move the counter thread to
	int opt_ctr_cpu = 1;
	// (-f) Flush+Reload Threshold
	size_t opt_fr_thresh = 0;
	// (-t) Which testcase to run
	string opt_testcase = "";
	// (-n) Noise threshold
	size_t opt_noise_thresh = std::numeric_limits<size_t>::max();
	// (-s) Flag: whether to sleep a short time before each probe access
	int opt_use_nanosleep = -1;
	// (-i) Flag to only run identification tests
	int opt_only_identification = 0;

	int opt;
	while ((opt = getopt(argc, argv, "c:e:f:t:n:s:i:")) != -1) {
		switch (opt) {
			case 'c':
				opt_target_cpu = atoi(optarg);
				break;
			case 'e':
				opt_ctr_cpu = atoi(optarg);
				break;
			case 'f':
				opt_fr_thresh = atoi(optarg);
				break;
			case 't':
				opt_testcase = string {optarg};
				break;
			case 'n':
				opt_noise_thresh = atoi(optarg);
				if (opt_noise_thresh > 1000) {
					fprintf(stderr, "Invalid noise threshold (must be in [0, 1000]).\n");
					exit(EXIT_FAILURE);
				}
				break;
			case 's':
				opt_use_nanosleep = atoi(optarg);
				if ( ! (opt_use_nanosleep == 0 || opt_use_nanosleep == 1)) {
					fprintf(stderr, "Invalid nanosleep value (-s) (must be either 0 or 1).\n");
					exit(EXIT_FAILURE);	
				}
				break;
			case 'i':
				opt_only_identification = atoi(optarg);
				if ( ! (opt_only_identification == 0 || opt_only_identification == 1)) {
					fprintf(stderr, "Invalid identification flag (-i) (must be either 0 or 1).\n");
					exit(EXIT_FAILURE);	
				}
				break;
			default: // unknown option
				fprintf(stderr,
					"Usage: %s\n"
					"  [-c <CPU core to run the tests on>]\n"
					"  [-e <CPU core to use for a counter thread (if enabled)>]\n"
					"  [-f <Flush+Reload threshold>]\n"
					"  [-n <Noise threshold (float in [0, 1000])>]\n"
					"  [-s <use_nanosleep flag (0 or 1)>]\n"
					"  [-i <only_identification flag (0 or 1)>]\n"
					"  [-t <testcase>]\n",
					argv[0]
				);
				exit(EXIT_FAILURE);
		}
	}

	// Pin process to first CPU core
	L::info("Pinning process to CPU %d\n", opt_target_cpu);
	pin_process_to_cpu(0, opt_target_cpu);

	// Initialize counter thread (if enabled and necessary on the platform)
	clock_init(opt_ctr_cpu);

	// Calibrate Flush+Reload threshold, noise threshold and sleep requirement (or use provided value)
	calibrate(opt_fr_thresh, opt_noise_thresh, opt_use_nanosleep);
	bool use_nanosleep = (opt_use_nanosleep != 0);
	L::info("Using Flush+Reload threshold: %zu, noise threshold: %zu, use_nanosleep: %d\n", opt_fr_thresh, opt_noise_thresh, use_nanosleep);

	// List of all testcases
	vector<unique_ptr<TestCaseBase>> testcases;
	testcases.push_back(make_unique<TestCaseAdjacent>(opt_fr_thresh, opt_noise_thresh, use_nanosleep));
	testcases.push_back(make_unique<TestCaseStride>  (opt_fr_thresh, opt_noise_thresh, use_nanosleep));
	testcases.push_back(make_unique<TestCaseStream>  (opt_fr_thresh, opt_noise_thresh, use_nanosleep));
	testcases.push_back(make_unique<TestCaseSMS>     (opt_fr_thresh, opt_noise_thresh, use_nanosleep));
	testcases.push_back(make_unique<TestCaseDCReplay>(opt_fr_thresh, opt_noise_thresh, use_nanosleep));
	testcases.push_back(make_unique<TestCasePointerArray>(opt_target_cpu, opt_ctr_cpu));
	testcases.push_back(make_unique<TestCasePointerChase>(opt_target_cpu, opt_ctr_cpu));

	// if no testcase is specified, run all testcases
	if (opt_testcase == "") {
		L::info("Running all test cases\n");
		for (unique_ptr<TestCaseBase> const& testcase : testcases) {
			L::info("Running test case: \"%s\"\n", testcase->id().c_str());
			Json j = testcase->run(opt_only_identification);
			L::info("%s\n", json_pretty_print(j.dump()).c_str());
			json_dump_to_file(j, "results-" + testcase->id() + ".json");
		}
	} else {
		bool found = false;
		for (unique_ptr<TestCaseBase> const& testcase : testcases) {
			if (opt_testcase == testcase->id()) {
				L::info("Running test case: \"%s\"\n", opt_testcase.c_str());
				Json j = testcase->run(opt_only_identification);
				L::info("%s\n", json_pretty_print(j.dump()).c_str());
				json_dump_to_file(j, "results-" + testcase->id() + ".json");
				found = true;
				break;
			}
		}
		if ( ! found) {
			L::err("Unknown testcase: \"%s\"\n", opt_testcase.c_str());
			clock_teardown();
			exit(EXIT_FAILURE);
		}
	}

	clock_teardown();

	return EXIT_SUCCESS;
}
