# Using FetchBench

## Supported Platforms, Minimum Requirements

Currently, FetchBench runs on x86_64 CPUs, ARM Cortex-A CPUs and the Apple M1. The environments that we ran FetchBench in are listed in Table 2 in the paper. FetchBench requires a Linux environment.

The runtime depends on the tests that are executed and on the speed of the target CPU. For each potential prefetcher design, FetchBench first runs the identification test. If the identifaction test is positive, the corresponding characterization tests are run as well. In general, expect the following minimum requirements for a complete run of FetchBench on a processor:

<table>
    <tr>
        <th>Runtime</th>
        <td>1-7 hours</td>
    </tr>
    <tr>
        <th>RAM</th>
        <td>2 GB</td>
    </tr>
    <tr>
        <th>Disk</th>
        <td>100 MB</td>
    </tr>
</table>

## Building FetchBench

TL;DR: Run CMake and specify your favorite timing source. Then run Make.

FetchBench requires a C/C++ compiler, CMake, Make and Python 3 with the matplotlib module installed. To improve experimental results, a tool to set the CPU frequency (such as cpufrequtils) can also be helpful. On Debian-based systems, these can be installed as follows:

```
$ sudo apt install build-essential cmake cpufrequtils python3-matplotlib
```

You need to specify the timing source you want to use as a flag to the **initial** CMake call, for instance:

```
$ CXXFLAGS="-DARM_MSR" CFLAGS="$CXXFLAGS" ASMFLAGS="$CXXFLAGS" cmake -B build .
$ make -C build -j$(nproc)
```

Note that **CMake only respects these attributes when it initializes the build directory for the first time**. Remove and re-create the `build` directory whenever you want to change the `CXXFLAGS`.

The following flags can be specified:

- Timing sources
    - `-DCOUNTER_THREAD`
    - `-DARM_MSR`
    - `-DAPPLE_MSR`
    - `-DRDTSC` 
    - `-DGETTIME`
- `-DINTEL_DONT_DISABLE_OTHER_PREFETCHERS`: On Intel, we are able to use MSRs to control prefetchers. This requires (a) root privileges and (b) that SecureBoot is disabled. If either condition cannot be fulfilled, setting this macro disables the MSR accesses.

### Platform-Specific Hints

#### x86_64 Processors

On Intel and AMD CPUs, we recommend trying the timing source `RDTSC` first. If the results are not satisfactory, try `GETTIME`, then `COUNTER_THREAD`.

#### Raspberry Pi 4

When `ARM_MSR` is selected as a timing source, FetchBench uses the `PMCCNTR_EL0` register to gather high-resolution timestamps. This register may not be accessible from userspace by default. For the Raspberry Pi 4/Cortex-A72, [we provide a kernel module](../covert-channel/kernel-modules/rpi4-module-ccr/armv8) that makes this register accessible until the next reboot. This module can be built and loaded as follows:

```
$ sudo apt install raspberrypi-kernel-headers
$ cd <FetchBench Repo>/covert-channel/kernel-modules/rpi4-module-ccr/armv8
$ make
$ sudo bash module_load.sh
```

#### Other ARM Cortex-A Processors

If it is possible to get access to the `PMCCNTR_EL0` register as a timing source on the target platform (or a similar high-resolution and low-overhead timing source), we recommend to try that first. If this is not possible, we recommend to try first `GETTIME`, then `COUNTER_THREAD`.

#### Apple M1

When compiling for the Apple M1, make sure to also set the `__APPLE__` macro, like so:
```
$ CXXFLAGS="-D__APPLE__ -DAPPLE_MSR" CFLAGS="$CXXFLAGS" ASMFLAGS="$CXXFLAGS" cmake -B build .
```
We recommend using the `APPLE_MSR` timing source.

## Running Experiments

### Preparation: Setting the CPU Frequency to a Fixed Value

In general, setting the CPU frequency to a fixed value tends to make the results more stable. This can be achieved using the `cpufreq-*` utilities. In particular, `cpufreq-info` prints the minimum and maximum frequency per CPU core. Then, the frequency can be set like this:

```
$ sudo cpufreq-set -f 1.50GHz
```

To set the frequency on a specific CPU core, the target core can be specified with the `-c` parameter. 
In case this command fails, setting the CPU frequency may not be supported by the hardware or the operating system. In this case, an alternative solution worth trying is to switch to the performance governor like this:

```
$ sudo cpufreq-set -c 0 -g performance
```

The changes to the CPU frequency can be verified by calling `cpufreq-info` again.

### Running FetchBench

The easiest way to run the framework is:

```
$ sudo build/fetchbench | tee out.log
```

As noted above, we only need root privileges on Intel CPUs. If the build is configured to not write to MSRs or if the framework is run on other targets, it can be called without `sudo`.

The following command line arguments can optionally be specified to override specific parameters:

#### CPU Core Selection 
- `-c`: Core to pin the process to. Defaults to `0`.
- `-e`: Core to pin the counter thread to (if selected as timing source during build). Defaults to `1`.

#### Thresholds and Dealing With Noise
- `-f`: Flush+Reload threshold. If not specified, we try to determine it automatically.
- `-n`: Noise level threshold between `0` and `1000`. Used to filter out a constant noise floor. If not specified, we try to determine it automatically. On (nearly) noise-free platforms, `0` should work fine.
- `-s`: Whether to sleep a microsecond before probing the cache (`1`) or not (`0`). This sometimes improves the signal strength, especially on ARM. If not specified, we try to automatically determine what works better by running a basic stride prefetcher experiment in both configurations and comparing the results.

#### Running Testcases Selectively
- `-t`: Select a specific testcase to run (either `adjacent`, `stride`, `stream`, `sms`, `dcreplay`, `parr`, or `pchase`). If not specified, we run all of them.
- `-i`: Whether to run only identification tests (`1`) or run identification tests for all prefetchers and characterization tests for those with positive identification results (`0`). Defaults to `0`.

## Outputs
The code generates a lot of traces (`trace-*.json`), some figures based on these traces (`*.svg`), and result summaries (`results-*.json`). The result summaries are also printed to stdout.

## Extending FetchBench
See [EXTENDING.md](EXTENDING.md) for instructions on how to add testcases for other prefetcher designs to FetchBench.

## Building the Source Code Documentation
If you have [Doxygen](https://doxygen.nl/) installed, you can generate an HTML-based source code documentation for the FetchBench sources as follows:

```
make -C build doc_doxygen
```

The documentation is generated in `build/docs`.
