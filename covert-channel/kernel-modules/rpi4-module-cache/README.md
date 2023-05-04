Kernel module to dump the content of the cache(s) on ARMv8-based platforms, especially on Cortex-A72.

`user/` contains a demo application that shows how to call the module from userspace.

**Note:** The Cortex A72 has a shared L2 cache, but each CPU core has its own L1 cache.
**For now, the moule only queries the L1 cache of CPU3**, all other L1 caches are ignored. If you are interested in the contents of L1, you should pin your userspace process to CPU 3 (either programmatically or by starting it via `taskset -c 3 ./my_program` on the command line).