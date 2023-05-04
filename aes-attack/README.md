# AES Attack

## Compiler

As we use some clang-specific features, we recommend to compile all code with clang.
To do this, run the following command before building any code from this repository:

    export CC=clang
    export CXX=clang++

## `lib/mbedtls`

This folder contains mbedtls 3.3.0.

The only change we did to the original code is that we fix the alignment of the AES lookup tables in memory. Otherwise, the compiler would choose the alignment arbitrarily, making the experiment harder to reproduce in its current stage. This change is documented in the commit history of this repository.

Compile mbedtls like this:

    cd lib/mbedtls
    DEBUG=1 SHARED=1 make no_test

As a result, a shared library file `libmbedcrypto.so.13` is produced in the `library/` subfolder.

## `program`

This folder contains the actual attack code.

### Build Instructions

First, generate a code file based on the mbedtls library binary like this:

    cd program
    objdump -d ../lib/mbedtls/library/libmbedcrypto.so | python3 ./generate_aligned_code.py > src/aligned_code.hh

Finally, build the attack:
    
    cmake -B build .
    make -C build

### Run the attack

First, you need to make sure that the cycle counter is accessible from user space, for example using https://github.com/jerinjacobk/armv8_pmu_cycle_counter_el0.

We recommend to run the attack in tmux:

    tmux
    build/attacker_automated > out.log

Detatch with `C-b d`, re-attach with `tmux at`.