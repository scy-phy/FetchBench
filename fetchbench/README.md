# Using FetchBench

## Check out the code

```
git clone ...
cd FetchBench/fetchbench
```

## Building the framework

In general, first run CMake, then run Make.

You need to specify the timing source you want to use as a compiler flag. Pass it to the **initial** CMake call, for instance:

```
CXXFLAGS="-DARM_MSR" CFLAGS="$CXXFLAGS" ASMFLAGS="$CXXFLAGS" cmake -B build .
make -C build -j$(nproc)
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

On Apple M1, make sure to also set the `__APPLE__` macro, like so:
```
CXXFLAGS="-D__APPLE__ -DAPPLE_MSR" CFLAGS="$CXXFLAGS" ASMFLAGS="$CXXFLAGS" cmake -B build .
```

## Running tests

The easiest way to run the framework is:

```
sudo build/prefetch-test | tee out.log
```

As noted above, we only need root privileges on Intel. If the build is configured to not write to MSRs or if the framework is run on ARM, it can be called without `sudo`.

Here are some command line arguments that can be used to override specific parameters:

### CPU core selection 
- `-c`: Core to pin the process to. Defaults to `0`.
- `-e`: Core to pin the counter thread to (if enabled). Defaults to `1`.

### Thresholds and dealing with noise
- `-f`: Flush+Reload threshold. If not specified, we try to determine it automatically.
- `-n`: Noise level threshold between `0` and `1000`. Used to filter out a constant noise floor. If not specified, we try to dermine it automatically. On (nearly) noise-free platforms, `0` should work fine.
- `-s`: Whether to sleep a microsecond before probing the cache (`1`) or not (`0`). This sometimes improves the signal strength, especially on ARM. If not specified, we try to automatically determine what works better.

### Running tests selectively
- `-t`: Select a specific testcase to run (either `adjacent`, `stride`, `stream`, `sms`, `dcreplay`, `parr`, or `pchase`). If not specified, we run all of them.
- `-i`: Whether to run only identification tests (`1`) or run identification tests for all prefetchers and characterization tests for those with positive identification results (`0`). Defaults to `0`.

# Outputs
The code generates a lot of traces (`trace-*.json`), some figures based on these traces (`*.svg`), and result summaries (`results-*.json`). The result summaries are also printed to stdout.