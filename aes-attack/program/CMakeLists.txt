cmake_minimum_required(VERSION 3.13.4)

set(PROJ_NAME "aes-interrupt-attack")

project(${PROJ_NAME} LANGUAGES CXX)

# add debug symbols
set(CMAKE_BUILD_TYPE Debug)

# define main executables
add_executable("attacker_automated" "src/attacker_automated.cc" "src/LUTHypothesis.cc" "src/cache_query.cc" "src/common_test.cc")
add_executable("victim" "src/victim.cc" "src/common_test.cc")
add_executable("fnf_calibrate" "src/fnf_calibrate.cc" "src/common_test.cc")
# enable warnings
target_compile_options("attacker_automated" PRIVATE "-Wall")
target_compile_options("victim" PRIVATE "-Wall")

# link with -pthread
find_package(Threads REQUIRED)
target_link_libraries("attacker_automated" Threads::Threads)

# enable C++17 support
target_compile_features("attacker_automated" PUBLIC cxx_std_17)
target_compile_features("victim" PUBLIC cxx_std_17)

# Compile attacker as PIC binary. This is beneficial for Flush&Reload on
# library code: with PIC set, the compiler does not generate stubs for
# library functions in the attacker binary. Thus, &function directly 
# points to the address of the library function in the shared function
# binary and not to a stub function in the attacker binary.
set_property(TARGET "attacker_automated" PROPERTY POSITION_INDEPENDENT_CODE TRUE)

include_directories("../lib/mbedtls/include")
file(GLOB MBEDLIB "../lib/mbedtls/library/*.so")
target_link_libraries("attacker_automated" ${MBEDLIB})
target_link_libraries("victim" ${MBEDLIB})
