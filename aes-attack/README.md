# AES Attack

## Compiler

As we use some clang-specific features, we recommend to compile all code with clang.
To do this, run the following command before building any code from this repository:

    export CC=clang
    export CXX=clang++

## `lib/mbedtls`

This folder contains mbedtls 3.3.0 (from https://github.com/Mbed-TLS/mbedtls/tree/8c89224991adff88d53cd380f42a2baa36f91454).

The only change we did to the original code is that we fix the alignment of the AES lookup tables in memory. Otherwise, the compiler would choose the alignment arbitrarily, making the experiment harder to reproduce in its current stage. This change is documented in the commit history of this repository (commit 5bceb03dfc6b6344911a64bc74e9e383ebc21976).

Compile mbedtls like this:

    cd lib/mbedtls
    DEBUG=1 SHARED=1 make no_test

As a result, a shared library file `libmbedcrypto.so.13` is produced in the `library/` subfolder.

## `program`

This folder contains the actual attack code.

### Build Instructions

First, we need to generate some code based on the mbedtls library binary. In particular, we place load instructions at locations in the attacker code where they collide with relevant load instructions in the victim code. We provide a python script that analyzes the mbedtls binary and generates the aligned code automatically:

    cd program
    objdump -d ../lib/mbedtls/library/libmbedcrypto.so | python3 ./generate_aligned_code.py > src/aligned_code.hh

Finally, build the attack:
    
    cmake -B build .
    make -C build

### Run the attack

In our experiments, we compiled and executed this attack on the Raspberry Pi 4, running Raspberry Pi OS 11.

First, you need to make sure that the cycle counter is accessible from user space, for example using https://github.com/jerinjacobk/armv8_pmu_cycle_counter_el0.

We recommend to run the attack in tmux:

    tmux
    build/attacker_automated > out.log

Detatch with `C-b d`, re-attach with `tmux at`.

When the attack is finished, find the results summary at the very end of `out.log`. A good result looks like this:

```
================
 Final Summary: 
================
=== LUT 0 ===
Key byte hypothesis  0: 1101____ (d0), correct: de (E_)
Key byte hypothesis  4: 1111____ (f0), correct: f0 (EE)
Key byte hypothesis  8: 0110____ (60), correct: 62 (E_)
Key byte hypothesis 12: 1001____ (90), correct: 92 (E_)
=== LUT 1 ===
Key byte hypothesis  5: 0001____ (10), correct: 1c (E_)
Key byte hypothesis  9: 0100____ (40), correct: 4d (E_)
Key byte hypothesis 13: 1010____ (a0), correct: ad (E_)
Key byte hypothesis  1: 0011____ (30), correct: 3d (E_)
=== LUT 2 ===
Key byte hypothesis 10: 1010____ (a0), correct: af (E_)
Key byte hypothesis 14: 1001____ (90), correct: 91 (E_)
Key byte hypothesis  2: 0101____ (50), correct: 5a (E_)
Key byte hypothesis  6: 0111____ (70), correct: 76 (E_)
=== LUT 3 ===
Key byte hypothesis 15: 1110____ (e0), correct: ef (E_)
Key byte hypothesis  3: 1111____ (f0), correct: f2 (E_)
Key byte hypothesis  7: 0101____ (50), correct: 58 (E_)
Key byte hypothesis 11: 1000____ (80), correct: 8f (E_)

```

Each "E" in the last column marks a correctly recovered nibble.