# Extending FetchBench

This document guides you through the process of implementing a testcase for a new prefetcher design in FetchBench.

## General Structure: Inheriting from `TestCaseBase`

For each prefetcher design, we implement a *testcase* in FetchBench. A testcase is a C++ class that inherits from [`TestCaseBase`](src/testcase.hh).

Follow the following steps to create a new testcase for your prefetcher design:

- Create a new `.hh` file for your testcase in [`src/`](src/), e.g., `src/testcase_example.hh`. Use the following template to implement a class in `testcase_example.hh` that inherits from [`TestCaseBase`](src/testcase.hh):

```c++
#pragma once

#include <string>
#include <limits>

#include "testcase.hh"
#include "cacheutils.hh"
#include "mapping.hh"
#include "logger.hh"

using std::string;

class TestCaseExample : public TestCaseBase {
private:
	// If you want to use any of FetchBench's globally determined
	// thresholds/flags in your testcase, pass them to the constructor as a
	// parameter and store them in class attribute variables. In this
	// example, we pass the Flush+Reload threshold and the noise threshold.
	size_t const fr_thresh;
	size_t const noise_thresh;

public:
	TestCaseExample(size_t fr_thresh, size_t noise_thresh)
	: fr_thresh {fr_thresh}
	, noise_thresh {noise_thresh}
	{}

	virtual string id() override {
		// Give your prefetcher a short, alphanumeric identifier. This will
		// be used to identify your testcase, for example to run only this
		// test from the command line.
		return "example";
	}

protected:
	virtual Json pre_test() override {
		// Implement any code here that should be run *before* the actual
		// test. E.g., disable/enable certain (unrelated) prefetchers on
		// certain architectures. get_arch() tells you the current
		// architecture (currently one of ARCH_INTEL, ARCH_AMD, ARCH_ARM,
		// ARCH_ARM_APPLE, or ARCH_UNKNOWN). If you want to return any
		// structured information, add it to the following JSON object.
		// This JSON will be integrated into the overall output JSON later.
		// For instance, you could indicate which architecture was
		// detected. See other testcases for examples.
		return Json::object {};
	}

	virtual Json post_test() override {
		// Implement any code here that should be run *after* the actual
		// test. For instance, you could re-enable unrelated prefetchers
		// that you disabled in the pre_test function. Again, you have the
		// option to return some structured information as a JSON object.
		return Json::object {};
	}
	
	virtual Json identify() override {
		// Implement your prefetcher identification test here. Refer to
		// existing testcases or the section "Common Patterns" in
		// EXTENDING.md for examples. Only if this test is positive, the
		// characterization test(s) (specified in the characterize()
		// function below) will be executed. If FetchBench runs in
		// identification-only mode (-i 1), only testcases implemented here
		// will be executed.

		// Identification tests MUST return a JSON object that contains at
		// least the boolean flag "identified". If you want to add any
		// additional information (i.e., some more detailed results of your
		// identification test), feel free to add them to the JSON object
		// as well. Again, this object will be part of the overall JSON
		// output.
		return Json::object {
			{ "identified", true },
		};
	}
	
	virtual Json characterize() override {
		// Implement your prefetcher characterization tests here. Refer to
		// existing testcases or the section "Common Patterns" in
		// EXTENDING.md for examples. For very simple prefetchers, where
		// the identification test tests all relevant characteristics
		// already, this might even be empty (i.e., just return an empty
		// JSON object). For more complex prefetchers, where you want to
		// find many characteristics of the prefetcher, we recommend
		// implementing helper functions first that test for specific
		// aspects. Then use this function only to call the helpers and
		// aggregate the results (see for example: testcase_stride.hh). In
		// the end, return a JSON structure that represents your
		// characterization results. We don't require a particular format
		// for that. Again, this object will be part of the overall JSON
		// output.
		return Json::object {};
	}
};
```

- Add an include for your new `.hh` file to [`src/testcases.hh`](src/testcases.hh)
```c++
#include "testcase_example.hh"
```

- Instantiate a testcase object in [`src/main.cc`](src/main.cc): Add a line similar to the following below the comment "List of all testcases". If you want to pass any of the globally determined thresholds or flags to your testcase, also add those here.
```c++
testcases.push_back(make_unique<TestCaseExample>(opt_fr_thresh, opt_noise_thresh));
```

- Re-compile FetchBench and run your testcase by calling `build/fetchbench` with the `-t <id>` parameter (e.g. `build/fetchbench -t example`). If you don't specify the testcase explicitly, all testcases will be executed, including your new one.

## Common Patterns

Apart from general C++, the following code snippets may come in handy to implement your own testcases.

### `Mapping`: Allocating Memory

We use the `Mapping` struct throughout our code to describe the memory regions that we run tests in. All code related to `Mapping`s is implemented in `src/mapping`[`.hh`](src/mapping.hh)/[`.cc`](src/mapping.cc). A `Mapping` consists of a pointer to the base address of the memory region and its size (in bytes):

```c++
// (from src/mapping.hh)
typedef struct {
	uint8_t* base_addr;
	size_t size;
} Mapping;
```

Use the following pattern to allocate/de-allocate memory:

```c++
// Allocate a mapping
Mapping mapping = allocate_mapping(16 * PAGE_SIZE);

// Flush the whole mapping
flush_mapping(mapping);

// <perform tests in the mapping>

// De-allocate the mapping
unmap_mapping(mapping);
```

> **Side note:** When working with memory at cache-line granularity or page granularity, we recommend to use the macros `CACHE_LINE_SIZE` and `PAGE_SIZE` from `src/cacheutils.hh` instead of concrete values, as they may differ from platform to platform.

Behind the scenes, we call [`mmap(2)`](https://www.kernel.org/doc/man-pages/online/pages/man2/mmap2.2.html) to allocate the memory. Thus, the base address of a mapping is always page-aligned. Inspect the function `allocate_mapping()` in [`src/mapping.cc`](src/mapping.cc) for details.

### Loading Memory Addresses and Inspecting the Cache

#### Basic Primitives

We provide a few handy primitives in `src/cacheutils`[`.hh`](src/cacheutils.hh)/[`.cc`](src/cacheutils.cc) to load or flush memory addresses and to time these operations. The timing source to be used on a specific platform is determined by preprocessor definitions at compile time, more precisely on the first call to `cmake` (see section "Building the framework" in the [main README](README.md#building-the-framework)).

In particular, FetchBench provides the following primitives:

```c++
// Some example address
uint8_t* ptr = mapping.base_addr + 2 * CACHE_LINE_SIZE;

// Access an address (i.e., bring a cache line into the cache)
maccess(ptr);

// Flush an address (i.e., remove a cache line from the cache)
flush(ptr);

// Memory barrier (MFENCE on Intel, DSB ISH on ARM)
mfence();

// Get a current timestamp from the selected timing source
size_t time = rdtsc();
```

#### Inlining vs. Non-inlining `maccess`

By default, the compiler should inline calls to `maccess()`, `flush()`, and `mfence()` to avoid the computational overhead of a function call. However, especially in case of `maccess`, it depends on your testcase whether you want to inline the load instruction or not. For this reason, we also provide the function `maccess_noinline()` that does *not* inline the load instruction.

Why does this matter? Some prefetchers take the instruction address of the load instruction that is used to load a memory address into consideration for their prefetching decision. When calls are inlined, each `maccess()` is transformed into a separate load instruction (at a distinct instruction address). In contrast, when calling `maccess_noinline()`, the load will always be performed by the single load instruction in the `maccess_noinline` function (located at a single instruction address). Consider the following example:

```c++
// In this example, the compiler inlines each of the two maccess calls. As
// a result, the resulting assembly contains two load instructions at two
// different instruction addresses.
maccess(ptr);
maccess(ptr + 2 * CACHE_LINE_SIZE);

// In this example, the compiler does not inline the maccess function.
// Instead, it calls the function maccess_noinline twice. As a result, the
// load instruction used to load the two addresses will be located at the
// same instruction address for both of the calls.
maccess_noinline(ptr);
maccess_noinline(ptr + 2 * CACHE_LINE_SIZE);
```

#### Aligned `maccess` Functions

In addition, we also provide (non-inlining) variants of the `maccess` functions that are aligned at certain boundaries in memory. As a result, the load instructions in these functions are also (nearly) aligned to these boundaries. For each boundary, we provide two `maccess` functions.

We use these functions to test for program counter collisions. They are implemented in `src/aligned_maccess`[`.cc`](src/aligned_maccess.cc)/[`.hh`](src/aligned_maccess.hh). The functions are named `maccess_X_Y`. `X` determines the alignment, e.g. `X=5` means that the function is aligned at a 2^5=32 byte boundary in memory. `Y` is either `1` for the first function or `2` for the second function. For convenience, we also provide a function `get_maccess_functions(X)` that returns a pair of function pointers to the two functions aligned at a particular boundary 2^`X`. The following code snippet gives an example how to use this:

```c++
vector<size_t> offsets_train = {0, 2, 4, 6, 8};
size_t offset_probe = 10;
for (size_t colliding_bits = 5; colliding_bits <= 24; colliding_bits++) {
	// get pointers to co-aligned maccess functions
	pair<maccess_func_t, maccess_func_t> maccess_funcs = get_maccess_functions(colliding_bits);
	maccess_func_t const& maccess_train = maccess_funcs.first;
	maccess_func_t const& maccess_probe = maccess_funcs.second;
	
	for (size_t offset : offsets_train) {
		maccess_train(mapping1.base_addr + offset * CACHE_LINE_SIZE);
	}
	maccess_probe(mapping2.base_addr + offset_probe * CACHE_LINE_SIZE);

	// <...> inspect cache state and flush mappings
}
```

#### Probing: Inspecting the Cache State

Use the primitives above to implement a memory access sequence that triggers your prefetcher. After that, you likely want to inspect the cache state of your mapping. In essence, we use a pattern like the following to decide whether accessing a pointer is a hit or a miss using the global Flush+Reload threshold and the noise threshold:

```c++
size_t time = flush_reload_t(ptr);
if (time < fr_thresh && time > noise_thresh) {
	// hit
} else {
	// miss
}
```

### `L`: Logging

FetchBench comes with a rudimentary logging system implemented in [`src/logger.hh`](src/logger.hh). It allows the user to configure log messages, log levels etc. at a central point.
To emit a log message at a certain log level, use one of the following printf-like functions.
```c++
L::debug("Some message %d\n", some_variable);
L::info("Some message %d\n", some_variable);
L::warn("Some message %d\n", some_variable);
L::err("Some message %d\n", some_variable);
```

### Plotting

To better understand our measurements, we found it helpful to not only look at sequences of numbers, but also plot them, usually in a heatmap. Since the ideal visual representation is highly dependent on the prefetcher design under test, we do not provide a general plotting solution as part of FetchBench.

Our general approach to plotting results can be seen in [`src/testcase_stride.hh`](src/testcase_stride.hh), for example. For this more complex testcase, we implement an [additional helper class](src/testcase_stride_strideexperiment.hh) that represents all the parameters of an individual stride prefetcher experiment, such as the stride or the number of steps. The experiment class is also responsible for running the experiment and extracting the results. In the main testcase class, we then compose our testcase from multiple experiments.
For each experiment, we further implement a function that dumps the experiment parameters and measurements into a `trace_*.json` file. Finally, we call a Python script, [`plot_stride.py`](plot_stride.py), which reads the JSON files and plots them.

If you want to add plots to your own testcases, we recommend to follow a similar approach: export the relevant data into `trace_*.json` files and write a python script that plots your data in a way that is convenient for you to interpret them. Feel free to use the existing `plot_*.py` scripts as templates.