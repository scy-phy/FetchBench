cmake_minimum_required(VERSION 3.13.4)

set(PROJ_NAME "attacker")

project(${PROJ_NAME} LANGUAGES CXX)

# add debug symbols
set(CMAKE_BUILD_TYPE Debug)

# add -fPIC: generate position-independent code (required to ensure the
# mapping for the shared library is actually shared, at least on my system)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# paths to use the kernel modules
include_directories("../kernel-modules/rpi4-module-cache")
include_directories("../kernel-modules/smc-module/")

# define main executables
add_executable("attacker" "src/attacker.cc")

# enable warnings
target_compile_options("attacker" PRIVATE "-Wall")

# enable C++17 support
target_compile_features("attacker" PUBLIC cxx_std_17)
