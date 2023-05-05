# Covert-channel on ARM Cortex-A72

## Modules

 1. `userspace-app` (Userspace application): 
 One end of the covert-channel that receives the secret data.
 
 2. `tfa-service` (EL3 service):
 Other end of the covert-channel that sends the secret data. This service runs in Arm Trusted Firmware (https://github.com/ARM-software/arm-trusted-firmware)

 3. `kernel-modules/rpi4-module-cache` (Kernel module):
 Kernel module that collects the hits and miss in cache.
 
 4. `kernel-modules/rpi4-module-ccr` (Kernel module):
 Kernel module for timer source
 

## Prerequisite
1. Clone OP-TEE project (It contains arm trusted firmware and OPTEE OS) [ Follow [doc](https://github.com/OP-TEE/manifest)]
 ```
 git clone https://github.com/OP-TEE/manifest
 ```
and copy the `tfa-service`
```
cp -r tfa-service/prefetch_induce_svc arm-trusted-firmware/services
```
2. Update the makefile to compile the new service

```diff --git a/bl31/bl31.mk b/bl31/bl31.mk
index 3964469..dd21e56 100644
--- a/bl31/bl31.mk
+++ b/bl31/bl31.mk
@@ -47,6 +47,7 @@ BL31_SOURCES          +=      bl31/bl31_main.c                                \
                                plat/common/aarch64/platform_mp_stack.S         \
                                services/arm_arch_svc/arm_arch_svc_setup.c      \
                                services/std_svc/std_svc_setup.c                \
+                               services/prefetch_induce_svc/prefetch_induce_svc.c \
                                ${PSCI_LIB_SOURCES}                             \
                                ${SPMD_SOURCES}                                 \
                                ${SPM_MM_SOURCES}                               \
```




## Compilation steps
1. `userspace-app`
```
mkdir build
cd build
cmake .. -P ../rkpi4.cmake
cd ..
make -C build
```
2. `rpi4-module-cache` and `rpi4-module-ccr`
```
make ARCH=arm64 CROSS_COMPILE=<ARM_TOOLCHAIN_PATH>/bin/aarch64-linux-gnu-
```
3. prefetch\_induce\_svc 
Follow [doc](https://optee.readthedocs.io/en/latest/building/gits/build.html) to build the service as part of OPTEE

## Usage
1. Flash the optee image generated in *out* directory
2. Load the kernel modules using 'module\_load.sh'
3. Run the *attacker* binary 

