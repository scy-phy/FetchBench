#pragma once

#include <cstdio>
#include <cstring>
#include <string>
#include <limits>

#include "testcase.hh"
#include "cacheutils.hh"
#include "mapping.hh"
#include "logger.hh"

using std::string;

class TestCasePointerChase : public TestCaseBase {
private:
	int const cpu_bin;
	int const cpu_ctrthread;
public:
	TestCasePointerChase(int cpu_bin, int cpu_ctrthread)
	: cpu_bin {cpu_bin}
	, cpu_ctrthread {cpu_ctrthread}
	{}

	virtual string id() override {
		return "pchase";
	}
	
protected:
	virtual Json pre_test() override {
		// stop global counter thread (if enabled). The external binary will
		// start its own.
		clock_teardown();
		// make sure all prefetchers are enabled
		architecture_t arch = get_arch();
		if (arch == ARCH_INTEL) {
			set_intel_prefetcher(-1, INTEL_L2_HW_PREFETCHER, true);
			set_intel_prefetcher(-1, INTEL_L2_ADJACENT_CL_PREFETCHER, true);
			set_intel_prefetcher(-1, INTEL_DCU_PREFETCHER, true);
			set_intel_prefetcher(-1, INTEL_DCU_IP_PREFETCHER, true);
		}
		return Json::object {};
	}

	virtual Json post_test() override {
		// restart global counter thread (if enabled)
		clock_init(cpu_ctrthread);
		if (get_arch() == ARCH_INTEL) {
			set_intel_prefetcher(-1, INTEL_L2_HW_PREFETCHER, true);
			set_intel_prefetcher(-1, INTEL_L2_ADJACENT_CL_PREFETCHER, true);
			set_intel_prefetcher(-1, INTEL_DCU_PREFETCHER, true);
			set_intel_prefetcher(-1, INTEL_DCU_IP_PREFETCHER, true);
		}
		return Json::object {};
	}
	
	virtual Json identify() override {
		size_t no_repetitions = 5000;

		L::info("Calling external binary...\n");
		fflush(stdout);
		pid_t pid = fork();
		switch(pid) {
			case -1:
				perror("fork() failed.");
				exit(EXIT_FAILURE);
			case 0: { // child
				#ifdef COUNTER_THREAD
					pin_process_to_cpu(0, cpu_bin, cpu_ctrthread);
				#else
					pin_process_to_cpu(0, cpu_bin);
				#endif
				std::string no_repetitions_str = std::to_string(no_repetitions);
				vector<char const *> argv {"sh", "runner_pchase.sh", no_repetitions_str.c_str(), NULL};
				execvp(const_cast<char* const>(argv.data()[0]), const_cast<char* const*>(argv.data()));
				break;
			}
			default: // parent
				L::info("Waiting for external binary to finish...\n");
				waitpid(pid, NULL, 0);
				break;
		}
		L::info("External binary finished.\n");

		plot_pchase("pchase", "pchase.log");

		// TODO: Separate identification and characterization properly
		// TODO: Automatically determine whether identification was successful or not
		return {};
	}
	
	virtual Json characterize() override {
		return {};
	}
};